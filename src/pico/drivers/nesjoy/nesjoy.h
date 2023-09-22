#pragma once
#include "inttypes.h"

#define D_JOY_DATA_PIN (16)
#define D_JOY_CLK_PIN (14)
#define D_JOY_LATCH_PIN (15)

void Init_NesJoystick();
void Deinit_NesJoystick();
bool decode_joy();
extern uint8_t data_joy;
extern bool is_joy_present;

#define NES_PAD_UP		(data_joy & 0x08)
#define NES_PAD_DOWN	(data_joy & 0x04)
#define NES_PAD_LEFT	(data_joy & 0x02)
#define NES_PAD_RIGHT	(data_joy & 0x01)
#define NES_PAD_A		(data_joy & 0x80)
#define NES_PAD_B		(data_joy & 0x40)
#define NES_PAD_SELECT	(data_joy & 0x20)
#define NES_PAD_START	(data_joy & 0x10)






