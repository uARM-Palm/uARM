//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _UWIRE_DEV_ADS7846_H_
#define _UWIRE_DEV_ADS7846_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_uWire.h"
#include "soc_GPIO.h"

enum Ads7846auxType {
	Ads7846auxTypeBatt,
	Ads7846auxTypeAux,
	Ads7846auxTypeTemp0,
	Ads7846auxTypeTemp1,
};

struct Ads7846;

struct Ads7846* ads7846init(struct SocUwire *uwire, uint_fast8_t uWireCsNo, struct SocGpio *gpio, int8_t penDownGpio /* negative for none */);

//external
void ads7846penInput(struct Ads7846 *ads, uint16_t x, uint16_t y, uint16_t z);	//zero z for pen up

void ads7846setAdc(struct Ads7846 *ads, enum Ads7846auxType adc, uint32_t mV);



#endif

