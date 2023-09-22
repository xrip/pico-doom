
#include <stdio.h>
#include "inttypes.h"
#include "pico/stdlib.h"
#include "nesjoy.h"

//Joy joy1 = {2, 5, 4, 0, 0, 0};

// #define MAX_JOY_MODE 5

// //uint8_t cfg_def_joy_mode=0;

// const char __in_flash() *joy_text[MAX_JOY_MODE]={
// 	"    Ext Joystick     ",
// 	"  Kempston Joystick  ",
// 	" Interface2 Joystick ",
// 	"   Cursor Joystick   ",
// 	" ---===[NONE]===---  "
// };

uint8_t data_joy=0;
uint8_t old_data_joy=0;
bool is_joy_present;

void  inJoyInit(uint gpio){
	gpio_init(gpio);
	gpio_set_dir(gpio,GPIO_IN);
	gpio_pull_up(gpio);
}
void  outJoyInit(uint gpio){
	gpio_init(gpio);
	gpio_set_dir(gpio,GPIO_OUT);
	//gpio_pull_up(gpio);
}

void  pinDeInit(uint gpio){
	gpio_deinit(gpio);
	gpio_disable_pulls(gpio);
};


uint8_t NES_joy_get_data(){
	gpio_put(D_JOY_LATCH_PIN,1);
	sleep_us(15);//12
	gpio_put(D_JOY_LATCH_PIN,0);
	sleep_us(8);//6
	uint8_t data = 0;
	for(int i=0;i<8;i++){		   
		gpio_put(D_JOY_CLK_PIN,0);  
		sleep_us(15);//10
		data<<=1;
		data|=gpio_get(D_JOY_DATA_PIN);
		sleep_us(15);//10
		gpio_put(D_JOY_CLK_PIN,1); 
		sleep_us(15);//10
	}
	if (data==0){
		is_joy_present=false;
		return 0;
	} else {
		is_joy_present=true;
		// data=(data&0x0f)|((data>>2)&0x30)|((data<<3)&0x80)|((data<<1)&0x40);
		return ~data;
	}
};

bool decode_joy(){
	data_joy=NES_joy_get_data();
	if(is_joy_present){
		if (data_joy!=old_data_joy){
			old_data_joy=data_joy;
			printf ("data joystick %02X \n", data_joy);
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

void Init_NesJoystick(){
		outJoyInit(D_JOY_CLK_PIN);
		gpio_put(D_JOY_CLK_PIN,1);
		outJoyInit(D_JOY_LATCH_PIN);
		gpio_put(D_JOY_LATCH_PIN,1);
		inJoyInit(D_JOY_DATA_PIN);
		NES_joy_get_data();	
};
void Deinit_NesJoystick(){
		pinDeInit(D_JOY_CLK_PIN);
		pinDeInit(D_JOY_LATCH_PIN);
		pinDeInit(D_JOY_DATA_PIN);
};