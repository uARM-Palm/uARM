//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _TPS65010_H_
#define _TPS65010_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_I2C.h"

struct Tps65010;

struct Tps65010* tps65010Init(struct SocI2c* i2c);

#endif
