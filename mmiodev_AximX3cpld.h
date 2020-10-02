//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _MMIO_AXIM_X3_CPLD_H_
#define _MMIO_AXIM_X3_CPLD_H_


#include <stdbool.h>
#include <stdint.h>
#include "mem.h"


struct AximX3cpld;



struct AximX3cpld* aximX3cpldInit(struct ArmMem *physMem);


#endif
