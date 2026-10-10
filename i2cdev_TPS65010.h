//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _TPS65010_H_
#define _TPS65010_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_I2C.h"
#include "soc_GPIO.h"

struct Tps65010;

//intGpio: PXA GPIO wired to the TPS65010 nINT output (active low), negative for none
struct Tps65010* tps65010Init(struct SocI2c* i2c, struct SocGpio *gpio, int8_t intGpio);

//power ("on/off") push button, reported in REGSTATUS bit 7 (TPS_REG_ONOFF)
void tps65010SetPowerButton(struct Tps65010 *tps, bool pressed);

#endif
