//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _TSC210x_H_
#define _TSC210x_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_uWire.h"
#include "soc_GPIO.h"
#include "soc_SSP.h"

struct Tsc210x;

enum TscChipType {
	TscType2101,
	TscType2102,
};

enum TscExternalAdcType {
	TscExternalAdcBat1,
	TscExternalAdcBat2,
	TscExternalAdcAux1,
	TscExternalAdcAux2,
	TscExternalAdcTemp1,
	TscExternalAdcTemp2,
};

struct Tsc210x* tsc210xInitSsp(struct SocSsp *ssp, struct SocGpio *gpio, int8_t chipSelectGpio, int8_t pintdavGpio /* negative for none */, enum TscChipType typ);
struct Tsc210x* tsc210xInitUWire(struct SocUwire *uwire, uint_fast8_t uWireCsNo, struct SocGpio *gpio, int8_t pintdavGpio /* negative for none */, enum TscChipType typ);

void tsc210xPeriodic(struct Tsc210x *tsc);

//external
void tsc210xPenInput(struct Tsc210x *tsc, int16_t x, int16_t y);	//negative for pen up

void tsc210xSetExtAdc(struct Tsc210x *tsc, enum TscExternalAdcType which, uint_fast16_t mV);


#endif
