//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _AC97_WM9705_H_
#define _AC97_WM9705_H_

#include <stdbool.h>
#include <stdint.h>
#include "soc_AC97.h"


struct WM9705;

enum WM9705auxPin {			//Vref is 3.3v, 
	WM9705auxPinBmon = 0,		//keep in mind this is divided by 3, so if battery is 3V, pass 1V to this
	WM9705auxPinAux,
	WM9705auxPinPhone,
	WM9705auxPinPcBeep,
};


struct WM9705* wm9705Init(struct SocAC97* ac97);
void wm9705periodic(struct WM9705 *wm);

void wm9705setAuxVoltage(struct WM9705 *wm, enum WM9705auxPin which, uint32_t mV);
void wm9705setPen(struct WM9705 *wm, int16_t x, int16_t y, int16_t press);		//raw ADC values, negative for pen up

#endif

