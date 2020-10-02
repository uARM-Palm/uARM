//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _AN32502A_H_
#define _AN32502A_H_

#include "soc_I2C.h"

struct An32502A;


struct An32502A* an32502aInit(struct SocI2c* i2c);

#endif
