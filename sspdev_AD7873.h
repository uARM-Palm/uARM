//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _AD7873_H_
#define _AD7873_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_SSP.h"
#include "soc_GPIO.h"

struct Ad7873;



struct Ad7873* ad7873Init(struct SocSsp* ssp, struct SocGpio* gpio, int8_t penIntGpio /* negative for none */);
void ad7873Periodic(struct Ad7873 *ad);

//external
void ad7873PenInput(struct Ad7873 *ad, int16_t x, int16_t y);	//negative for pen up

void ad7873setVbatt(struct Ad7873 *ad, uint16_t mV);
void ad7873setVaux(struct Ad7873 *ad, uint16_t mV);


#endif
