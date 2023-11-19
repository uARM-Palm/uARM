//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _AC97_ucb1400_H_
#define _AC97_ucb1400_H_

#include <stdbool.h>
#include <stdint.h>
#include "soc_AC97.h"
#include "soc_GPIO.h"


struct UCB1400;




struct UCB1400* ucb1400Init(struct SocAC97* ac97, struct SocGpio *gpio, int8_t irqPin);
void ucb1400periodic(struct UCB1400 *ucb);


void ucb1400setAuxVoltage(struct UCB1400 *ucb, uint8_t adcIdx, uint32_t mV);			//0..3
void ucb1400setGpioInputVal(struct UCB1400 *ucb, uint8_t gpioIdx, bool high);			//0..8, external set for input pins
bool ucb1400getGpioOutputVal(struct UCB1400 *ucb, uint8_t gpioIdx);						//0..8, external get for output pins
void ucb1400setPen(struct UCB1400 *ucb, int16_t x, int16_t y);							//raw ADC values, negative for pen up




#endif

