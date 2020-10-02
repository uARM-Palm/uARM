//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _SOC_I2S_H_
#define _SOC_I2S_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"
#include "soc_DMA.h"
#include <stdbool.h> 
#include <stdint.h> 
#include <stdio.h> 




struct SocI2s;

struct SocI2s* socI2sInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma);
void socI2sPeriodic(struct SocI2s *i2s);



#endif

