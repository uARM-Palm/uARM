//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _AK4534_H_
#define _AK4534_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_I2C.h"
#include "soc_I2S.h"
#include "soc_GPIO.h"


struct AK4534;



struct AK4534* ak4534Init(struct SocI2c *i2c, struct SocI2s *i2s, struct SocGpio *gpio);



#endif
