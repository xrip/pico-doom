//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     SDL implementation of system-specific input interface.
//


//#include "SDL.h"
//#include "SDL_keycode.h"
#include <doom/sounds.h>
#include <doom/s_sound.h>
#include <doom/doomstat.h>
#include "pico.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "d_event.h"
#include "i_input.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "hardware/uart.h"
#include <stdlib.h>
#if USB_SUPPORT
#include "pico/binary_info.h"
#include "tusb.h"
#include "hardware/irq.h"
bi_decl(bi_program_feature("USB keyboard support"))
bi_decl(bi_program_feature("USB mouse support"))
#endif
#include "ps2.h"
#include <pico/stdlib.h>

static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

// Lookup table for mapping ASCII characters to their equivalent when
// shift is pressed on a US layout keyboard. This is the original table
// as found in the Doom sources, comments and all.
static const char shiftxform[] =
        {
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                31, ' ', '!', '"', '#', '$', '%', '&',
                '"', // shift-'
                '(', ')', '*', '+',
                '<', // shift-,
                '_', // shift--
                '>', // shift-.
                '?', // shift-/
                ')', // shift-0
                '!', // shift-1
                '@', // shift-2
                '#', // shift-3
                '$', // shift-4
                '%', // shift-5
                '^', // shift-6
                '&', // shift-7
                '*', // shift-8
                '(', // shift-9
                ':',
                ':', // shift-;
                '<',
                '+', // shift-=
                '>', '?', '@',
                'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                '[', // shift-[
                '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
                ']', // shift-]
                '"', '_',
                '\'', // shift-`
                'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                '{', '|', '}', '~', 127
        };

// If true, I_StartTextInput() has been called, and we are populating
// the data3 field of ev_keydown events.
static boolean text_input_enabled = true;

// Bit mask of mouse button state.
static unsigned int mouse_button_state = 0;

// Disallow mouse and joystick movement to cause forward/backward
// motion.  Specified with the '-novert' command line parameter.
// This is an int to allow saving to config file
static int novert = 0;

// If true, keyboard mapping is ignored, like in Vanilla Doom.
// The sensible thing to do is to disable this if you have a non-US
// keyboard.

#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
int vanilla_keyboard_mapping = true;
#endif

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.
#if !NO_USE_MOUSE
int mouse_acceleration = 2;
int mouse_threshold = 10;
#endif

// Translates the SDL key to a value of the type found in doomkeys.h
int TranslateKey(int scancode)
{
    switch (scancode)
    {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return KEY_RCTRL;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return KEY_RSHIFT;

        case SDL_SCANCODE_LALT:
            return KEY_LALT;

        case SDL_SCANCODE_RALT:
            return KEY_RALT;

        default:
            if (scancode >= 0 && scancode < arrlen(scancode_translate_table))
            {
                return scancode_translate_table[scancode];
            }
            else
            {
                return 0;
            }
    }
}

// Get the localized version of the key press. This takes into account the
// keyboard layout, but does not apply any changes due to modifiers, (eg.
// shift-, alt-, etc.)
static int GetLocalizedKey(int scancode)
{
    // When using Vanilla mapping, we just base everything off the scancode
    // and always pretend the user is using a US layout keyboard.
    if (vanilla_keyboard_mapping)
    {
        return TranslateKey(scancode);
    }
    else
    {
        assert(false); return 0;
//        int result = sym->sym;
//
//        if (result < 0 || result >= 128)
//        {
//            result = 0;
//        }
//
//        return sym_<result;
    }
}

// Get the equivalent ASCII (Unicode?) character for a keypress.
int GetTypedChar(int scancode, boolean shiftdown)
{
    // We only return typed characters when entering text, after
    // I_StartTextInput() has been called. Otherwise we return nothing.
    if (!text_input_enabled)
    {
        return 0;
    }

    // If we're strictly emulating Vanilla, we should always act like
    // we're using a US layout keyboard (in ev_keydown, data1=data2).
    // Otherwise we should use the native key mapping.
    if (vanilla_keyboard_mapping)
    {
        int result = TranslateKey(scancode);

        // If shift is held down, apply the original uppercase
        // translation table used under DOS.
        if (shiftdown
            && result >= 0 && result < arrlen(shiftxform))
        {
            result = shiftxform[result];
        }

        return result;
    }
    else
    {
#if 0
        SDL_Event next_event;

        // Special cases, where we always return a fixed value.
        switch (sym->sym)
        {
            case SDLK_BACKSPACE: return KEY_BACKSPACE;
            case SDLK_RETURN:    return KEY_ENTER;
            default:
                break;
        }

        // The following is a gross hack, but I don't see an easier way
        // of doing this within the SDL2 API (in SDL1 it was easier).
        // We want to get the fully transformed input character associated
        // with this keypress - correct keyboard layout, appropriately
        // transformed by any modifier keys, etc. So peek ahead in the SDL
        // event queue and see if the key press is immediately followed by
        // an SDL_TEXTINPUT event. If it is, it's reasonable to assume the
        // key press and the text input are connected. Technically the SDL
        // API does not guarantee anything of the sort, but in practice this
        // is what happens and I've verified it through manual inspect of
        // the SDL source code.
        //
        // In an ideal world we'd split out ev_keydown into a separate
        // ev_textinput event, as SDL2 has done. But this doesn't work
        // (I experimented with the idea), because lots of Doom's code is
        // based around different responders "eating" events to stop them
        // being passed on to another responder. If code is listening for
        // a text input, it cannot block the corresponding keydown events
        // which can affect other responders.
        //
        // So we're stuck with this as a rather fragile alternative.

        if (SDL_PeepEvents(&next_event, 1, SDL_PEEKEVENT,
                           SDL_FIRSTEVENT, SDL_LASTEVENT) == 1
            && next_event.type == SDL_TEXTINPUT)
        {
            // If an SDL_TEXTINPUT event is found, we always assume it
            // matches the key press. The input text must be a single
            // ASCII character - if it isn't, it's possible the input
            // char is a Unicode value instead; better to send a null
            // character than the unshifted key.
            if (strlen(next_event.text.text) == 1
                && (next_event.text.text[0] & 0x80) == 0)
            {
                return next_event.text.text[0];
            }
        }
#else
        assert(false);
#endif

        // Failed to find anything :/
        return 0;
    }
}

void I_StartTextInput(int x1, int y1, int x2, int y2)
{
    text_input_enabled = true;

    if (!vanilla_keyboard_mapping)
    {
#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
        // SDL2-TODO: SDL_SetTextInputRect(...);
        SDL_StartTextInput();
#endif
    }
}

void I_StopTextInput(void)
{
    text_input_enabled = false;

    if (!vanilla_keyboard_mapping)
    {
#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
        SDL_StopTextInput();
#endif
    }
}


// Bind all variables controlling input options.
void I_BindInputVariables(void)
{
#if !NO_USE_MOUSE
    M_BindIntVariable("mouse_acceleration",        &mouse_acceleration);
    M_BindIntVariable("mouse_threshold",           &mouse_threshold);
#endif
#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
    M_BindIntVariable("vanilla_keyboard_mapping",  &vanilla_keyboard_mapping);
#endif
    M_BindIntVariable("novert",                    &novert);
}

#if PICO_NO_HARDWARE
#include "pico/scanvideo.h"
#else
#define WITH_SHIFT 0x8000
#endif

static void pico_key_down(int scancode, int modifiers) {
    event_t event;
    event.type = ev_keydown;
    event.data1 = TranslateKey(scancode);
    event.data2 = GetLocalizedKey(scancode);
    event.data3 = GetTypedChar(scancode, modifiers & WITH_SHIFT ? 1 : 0);

    if (at_exit_screen) {
        handle_exit_key_down(scancode, modifiers & WITH_SHIFT ? 1 : 0, exit_screen_kb_buffer_80, 80);
        return;
    }
    if (event.data1 != 0)
    {
        D_PostEvent(&event);
    }
}

static void pico_key_up(int scancode) {
    event_t event;
    event.type = ev_keyup;
    event.data1 = TranslateKey(scancode);
    // data2/data3 are initialized to zero for ev_keyup.
    // For ev_keydown it's the shifted Unicode character
    // that was typed, but if something wants to detect
    // key releases it should do so based on data1
    // (key ID), not the printable char.
    event.data2 = 0;
    event.data3 = 0;
    if (event.data1 != 0)
    {
        D_PostEvent(&event);
    }
}

struct keyboard_bits_t {
    bool _1: true;
    bool _2: true;
    bool _3: true;
    bool _4: true;
    bool _5: true;
    bool _6: true;
    bool _7: true;
    bool _8: true;
    bool _9: true;
    bool _0: true;

    bool f11: true;

    bool shift: true;
    bool ctrl: true;
    bool space: true;
    bool escape: true;
    bool enter: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};

struct keyboard_bits_t keyboard_bits = {};
struct keyboard_bits_t keyboard_bits_prev = {};

static void update_button_state(int scancode, bool button_state, bool previous_button_state) {
    if (button_state && !previous_button_state) {
        pico_key_down(scancode, 0);
    } else if (!button_state && previous_button_state) {
        pico_key_up(scancode);
    }
}

static void ps2kbd_tick() {
    keyboard_bits.space = KBD_SPACE;
    keyboard_bits.ctrl = KBD_L_CTRL || KBD_R_CTRL;

    keyboard_bits.enter = KBD_ENTER;
    keyboard_bits.escape = KBD_ESC;

    keyboard_bits.up = KBD_UP;
    keyboard_bits.down = KBD_DOWN;
    keyboard_bits.left = KBD_LEFT;
    keyboard_bits.right = KBD_RIGHT;

    keyboard_bits.f11 = KBD_F11;
    keyboard_bits._0 = KBD_0;
    keyboard_bits._1 = KBD_1;
    keyboard_bits._2 = KBD_2;
    keyboard_bits._3 = KBD_3;
    keyboard_bits._4 = KBD_4;
    keyboard_bits._5 = KBD_5;
    keyboard_bits._6 = KBD_6;
    keyboard_bits._7 = KBD_7;
    keyboard_bits._8 = KBD_8;
    keyboard_bits._9 = KBD_9;

    update_button_state(SDL_SCANCODE_LCTRL, keyboard_bits.ctrl, keyboard_bits_prev.ctrl);
    update_button_state(SDL_SCANCODE_SPACE, keyboard_bits.space, keyboard_bits_prev.space);

    update_button_state(SDL_SCANCODE_LEFT, keyboard_bits.left, keyboard_bits_prev.left);
    update_button_state(SDL_SCANCODE_RIGHT, keyboard_bits.right, keyboard_bits_prev.right);
    update_button_state(SDL_SCANCODE_UP, keyboard_bits.up, keyboard_bits_prev.up);
    update_button_state(SDL_SCANCODE_DOWN, keyboard_bits.down, keyboard_bits_prev.down);

    update_button_state(SDL_SCANCODE_RETURN, keyboard_bits.enter, keyboard_bits_prev.enter);
    update_button_state(SDL_SCANCODE_ESCAPE, keyboard_bits.escape, keyboard_bits_prev.escape);

    update_button_state(SDL_SCANCODE_0, keyboard_bits._0, keyboard_bits_prev._0);
    update_button_state(SDL_SCANCODE_1, keyboard_bits._1, keyboard_bits_prev._1);
    update_button_state(SDL_SCANCODE_2, keyboard_bits._2, keyboard_bits_prev._2);
    update_button_state(SDL_SCANCODE_3, keyboard_bits._3, keyboard_bits_prev._3);
    update_button_state(SDL_SCANCODE_4, keyboard_bits._4, keyboard_bits_prev._4);
    update_button_state(SDL_SCANCODE_5, keyboard_bits._5, keyboard_bits_prev._5);
    update_button_state(SDL_SCANCODE_6, keyboard_bits._6, keyboard_bits_prev._6);
    update_button_state(SDL_SCANCODE_7, keyboard_bits._7, keyboard_bits_prev._7);
    update_button_state(SDL_SCANCODE_8, keyboard_bits._8, keyboard_bits_prev._8);
    update_button_state(SDL_SCANCODE_9, keyboard_bits._9, keyboard_bits_prev._9);

    update_button_state(SDL_SCANCODE_F11, keyboard_bits.f11, keyboard_bits_prev.f11);



    keyboard_bits_prev = keyboard_bits;

}


#if PICO_NO_HARDWARE
static void pico_quit(void) {
    exit(0);
}
#endif

void I_InputInit(void) {
    Init_kbd();
#if PICO_NO_HARDWARE
    platform_key_down = pico_key_down;
    platform_key_up = pico_key_up;
    platform_quit = pico_quit;
#elif USB_SUPPORT
    tusb_init();
    irq_set_priority(USBCTRL_IRQ, 0xc0);
#endif

//    gpio_init(1);
//    gpio_set_dir(1,GPIO_IN);
//    gpio_pull_up(1);
}
#include "deh_str.h"
void I_GetEvent() {
    decode_kbd();
#if USB_SUPPORT
    tuh_task();
#endif

    ps2kbd_tick();

  //auto state = !KBD_PRESS;
/*  if (KBD_PRESS) {
        pico_key_down(SDL_SCANCODE_ESCAPE, 0);
        pico_key_up(SDL_SCANCODE_ESCAPE);
    }
    */
    //if (KBD_RELEASE) pico_key_up(SDL_SCANCODE_ESCAPE);

    return I_GetEventTimeout(50);
}

void I_GetEventTimeout(int key_timeout) {
#if PICO_ON_DEVICE && !NO_USE_UART

    if (uart_is_readable(uart_default)) {
        char c = uart_getc(uart_default);
        if (c == 26 && uart_is_readable_within_us(uart_default, key_timeout)) {
            c = uart_getc(uart_default);

            static int modifiers = 0;
            switch (c) {
                case 0:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint scancode = (uint8_t) uart_getc(uart_default);
                        if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
                            modifiers |= WITH_SHIFT;
                        }
                        pico_key_down(scancode, modifiers);
                    }
                    return;
                case 1:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint scancode = (uint8_t) uart_getc(uart_default);
                        if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
                            modifiers &= ~WITH_SHIFT;
                        }
                        pico_key_up(scancode);
                    }
                    return;
                case 2:
                case 3:
                case 5:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint __unused scancode = (uint8_t) uart_getc(uart_default);
                    }
                    return;
                case 4:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint __unused scancode = (uint8_t) uart_getc(uart_default);
                    }
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint __unused scancode = (uint8_t) uart_getc(uart_default);
                    }
                    return;
            }
        }
    }
#endif
}

#if USB_SUPPORT

#define MAX_REPORT  4
#define debug_printf(fmt,...) ((void)0)

// Each HID instance can has multiple reports
static struct
{
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

static void process_kbd_report(hid_keyboard_report_t const *report);
static void process_mouse_report(hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

static bool is_sandio_mouse(uint8_t dev_addr)
{
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    return vid ==0x19ca && pid == 0x0001; // Mindtribe Sandio 3D HID Mouse
}

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    debug_printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

    // Interface protocol (hid_interface_protocol_enum_t)
    const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    debug_printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

    // By default host stack will use activate boot protocol on supported interface.
    // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
    if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
    {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
        debug_printf("HID has %u reports \r\n", hid_info[instance].report_count);
    }

    const bool is_sandio = is_sandio_mouse(dev_addr);

    if (itf_protocol != HID_ITF_PROTOCOL_NONE || is_sandio)
    {
        // request to receive report
        // tuh_hid_report_received_cb() will be invoked when report is available
        if (!tuh_hid_receive_report(dev_addr, instance))
        {
            debug_printf("Error: cannot request to receive report\r\n");
        }
    }

    if (itf_protocol == HID_ITF_PROTOCOL_MOUSE)
    {
        key_nextweapon = '\'';
        key_prevweapon = '/';
    }

    if (is_sandio) {
        novert = 1;
        mouseSensitivity <<= 1;
    }

}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    debug_printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
    printf("USB: device %d disconnected\n", dev_addr);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
            TU_LOG2("HID receive boot keyboard report\r\n");
            process_kbd_report((hid_keyboard_report_t const*)report);
            break;

#if !NO_USE_MOUSE
        case HID_ITF_PROTOCOL_MOUSE:
            TU_LOG2("HID receive boot mouse report\r\n");
            process_mouse_report((hid_mouse_report_t const*)report);
            break;
#endif

        default:
            // Generic report requires matching ReportID and contents with previous parsed report info
            process_generic_report(dev_addr, instance, report, len);
            break;
    }

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance))
    {
        debug_printf("Error: cannot request to receive report\r\n");
    }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
    for(uint8_t i=0; i<6; i++)
    {
        if (report->keycode[i] == keycode)  return true;
    }

    return false;
}

static void check_mod(int mod, int prev_mod, int mask, int scancode) {
    if ((mod^prev_mod)&mask) {
        if (mod & mask)
            pico_key_down(scancode, 0);
        else
            pico_key_up(scancode);
    }
}

static bool caps_lock = false;
static int maybe_with_shift(bool is_shift)
{
    return ((is_shift && !caps_lock) || (!is_shift && caps_lock)) ? WITH_SHIFT : 0;
}

static void process_kbd_report(hid_keyboard_report_t const *report)
{
    static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

    //------------- example code ignore control (non-printable) key affects -------------//
    for(uint8_t i=0; i<6; i++)
    {
        if (report->keycode[i])
        {
            if (find_key_in_report(&prev_report, report->keycode[i]))
            {
                // exist in previous report means the current key is holding
            }
            else
            {
                // not existed in previous report means the current key is pressed
                bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
                pico_key_down(report->keycode[i], maybe_with_shift(is_shift));
                if (report->keycode[i] == HID_KEY_CAPS_LOCK)
                {
                    caps_lock = !caps_lock;
                    if (caps_lock) pico_key_down(SDL_SCANCODE_RSHIFT, 0);
                    else pico_key_up(SDL_SCANCODE_RSHIFT);
                }
            }
        }
        // Check for key depresses (i.e. was present in prev report but not here)
        if (prev_report.keycode[i]) {
            // If not present in the current report then depressed
            if (!find_key_in_report(report, prev_report.keycode[i]))
            {
                pico_key_up(prev_report.keycode[i]);
            }
        }
    }
    // synthesize events for modifier keys
    static const uint8_t mods[] = {
            KEYBOARD_MODIFIER_LEFTCTRL, SDL_SCANCODE_LCTRL,
            KEYBOARD_MODIFIER_RIGHTCTRL, SDL_SCANCODE_RCTRL,
            KEYBOARD_MODIFIER_LEFTALT, SDL_SCANCODE_LALT,
            KEYBOARD_MODIFIER_RIGHTALT, SDL_SCANCODE_RALT,
            KEYBOARD_MODIFIER_LEFTSHIFT, SDL_SCANCODE_LSHIFT,
            KEYBOARD_MODIFIER_RIGHTSHIFT, SDL_SCANCODE_RSHIFT,
    };
    for(int i=0;i<count_of(mods); i+= 2) {
        check_mod(report->modifier, prev_report.modifier, mods[i], mods[i+1]);
    }
    prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

#if !NO_USE_MOUSE
static int accel_mouse(int val)
{
    if (val < 0) return -accel_mouse(-val);
    return val > mouse_threshold ?
        (val - mouse_threshold) * mouse_acceleration + mouse_threshold : val;
}

static void process_mouse_report(hid_mouse_report_t const * report)
{
    static hid_mouse_report_t prev_report = { 0 };
    static uint8_t weapon_cycle = 0;

    uint8_t buttons_changed = report->buttons ^ prev_report.buttons;

    // special forward and backward keys
    if (buttons_changed)
    {
        static const uint8_t mouse_synth_keys[] = {
            MOUSE_BUTTON_FORWARD, HID_KEY_ARROW_UP,
            MOUSE_BUTTON_BACKWARD, HID_KEY_ARROW_DOWN,
        };
        for(int i=0; i < count_of(mouse_synth_keys); i+= 2)
        {
            uint8_t button = mouse_synth_keys[i];
            uint8_t key = mouse_synth_keys[i+1];
            if (report->buttons & button && !(prev_report.buttons & button)) pico_key_down(key, maybe_with_shift(false));
            if (!(report->buttons & button) && prev_report.buttons & button) pico_key_up(key);
        }
    }

    // mouse movement and first 3 buttons
    if (report->x != 0 || (!novert && report->y != 0) || buttons_changed)
    {
        event_t event;
        event.type = ev_mouse;
        event.data1 = report->buttons & 7;
        event.data2 = accel_mouse(report->x);
        event.data3 = novert ? 0 : -accel_mouse(report->y);
        D_PostEvent(&event);
    }

    // naive mouse wheel handling yet it works
    if (weapon_cycle) pico_key_up(weapon_cycle);
    if (report->wheel)
    {
        weapon_cycle = report->wheel > 0 ? HID_KEY_SLASH : HID_KEY_APOSTROPHE;
        pico_key_down(weapon_cycle, 0);
    }
    else
    {
        weapon_cycle = 0;
    }

    prev_report = *report;
}
#endif


#define TR 0x01
#define TF 0x02
#define TL 0x04
#define TB 0x08
#define RU (0x01 << 4)
#define RD (0x02 << 4)
#define RF (0x04 << 4)
#define RB (0x08 << 4)
#define LF (0x01 << 8)
#define LD (0x02 << 8)
#define LB (0x04 << 8)
#define LU (0x08 << 8)

struct rule {
    uint16_t mask;
    uint8_t code;
};

static const struct rule *rules = (struct rule[]){
    {TF, HID_KEY_ARROW_UP},
    {TB, HID_KEY_ARROW_DOWN},
    {TL, HID_KEY_COMMA},
    {TR, HID_KEY_PERIOD},
    {RF, HID_KEY_SPACE},
    {RB, HID_KEY_TAB},
    {RD, HID_KEY_ENTER},
    {RU, HID_KEY_ESCAPE},
    {LF, HID_KEY_SLASH},
    {LB, HID_KEY_APOSTROPHE},
    {LD, HID_KEY_Y},
    {LU, HID_KEY_N},
    {0, 0},
};

struct sandio_state {
    unsigned char top, right, left;
};


struct __attribute__ ((__packed__)) sandio_encoded_state {
    uint8_t constant;
    uint8_t dpi;
    uint8_t top;
    uint8_t right;
    uint8_t left;
    uint8_t magic_hi;
    uint8_t magic_lo;
};

unsigned char sandio_decode_button(uint8_t encoded, uint8_t off) {
    uint8_t val = encoded - off;
    return (unsigned char)(val & 0xF0 ? 0 : val & 0x0F);
}

/* buttons state is sent "encrypted"
 * thanks to https://github.com/EssentialNPC/SandioKeyMapper/blob/6bf5069cb02822964217b78aaedca098281208f2/GetRIData.cpp#L454
 */
struct sandio_state sandio_decode(const void *_encoded) {
    const struct sandio_encoded_state *encoded = _encoded;

    uint16_t magic = ((uint16_t)encoded->magic_hi) << 8 | encoded->magic_lo;
    uint8_t off = 240 * ((21 * (uint32_t)magic + 7) % 2048) / 2048;

    struct sandio_state decoded = {
        .top   = sandio_decode_button(encoded->top, off),
        .right = sandio_decode_button(encoded->right, off),
        .left  = sandio_decode_button(encoded->left, off),
    };

    return decoded;
}

static uint16_t activated = 0, history = 0;
static void process_multi_axis_controller_report(uint8_t const* report, uint16_t len)
{
    if (len != 7) return;

    const struct sandio_state state = sandio_decode(report);

    uint16_t packed = state.top | (state.right << 4) | (state.left << 8);
    if (packed == history) {
        return;
    }
    history = packed;
    for (int i = 0;; ++i) {
        const struct rule r = rules[i];
        if (!r.mask) {
            break; // end of rules
        }
        if (packed & r.mask & ~activated) {
            pico_key_down(r.code, maybe_with_shift(false));
            activated |= r.mask;
        }
        if (~packed & r.mask & activated) {
            pico_key_up(r.code);
            activated &= ~r.mask;
        }
    }
    pico_key_down(SDL_SCANCODE_RSHIFT, 0); // always run
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void) dev_addr;

    uint8_t const rpt_count = hid_info[instance].report_count;
    tuh_hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
    tuh_hid_report_info_t* rpt_info = NULL;

    if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
    {
        // Simple report without report ID as 1st byte
        rpt_info = &rpt_info_arr[0];
    }else
    {
        // Composite report, 1st byte is report ID, data starts from 2nd byte
        uint8_t const rpt_id = report[0];

        // Find report id in the arrray
        for(uint8_t i=0; i<rpt_count; i++)
        {
            if (rpt_id == rpt_info_arr[i].report_id )
            {
                rpt_info = &rpt_info_arr[i];
                break;
            }
        }

        report++;
        len--;
    }

    if (!rpt_info)
    {
        debug_printf("Couldn't find the report info for this report !\r\n");
        return;
    }

    // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
    // - Keyboard                     : Desktop, Keyboard
    // - Mouse                        : Desktop, Mouse
    // - Gamepad                      : Desktop, Gamepad
    // - Consumer Control (Media Key) : Consumer, Consumer Control
    // - System Control (Power key)   : Desktop, System Control
    // - Generic (vendor)             : 0xFFxx, xx
    if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
    {
        switch (rpt_info->usage)
        {
            case HID_USAGE_DESKTOP_KEYBOARD:
                TU_LOG1("HID receive keyboard report\r\n");
                // Assume keyboard follow boot report layout
                process_kbd_report( (hid_keyboard_report_t const*) report );
                break;

#if !NO_USE_MOUSE
            case HID_USAGE_DESKTOP_MOUSE:
                TU_LOG1("HID receive mouse report\r\n");
                // Assume mouse follow boot report layout
                process_mouse_report( (hid_mouse_report_t const*) report );
                break;
#endif

           case HID_USAGE_DESKTOP_MULTI_AXIS_CONTROLLER:
                TU_LOG1("HID receive multi-axis controller report\r\n");
                process_multi_axis_controller_report(report, len);
                break;

            default: break;
        }
    }
}

#endif
