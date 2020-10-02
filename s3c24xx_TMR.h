//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_TIMERS_H_
#define _S3C24XX_TIMERS_H_

#include "soc_DMA.h"
#include "soc_IC.h"
#include "mem.h"

struct S3C24xxTimers;


struct S3C24xxTimers* s3c24xxTimersInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma);
void s3c24xxTimersPeriodic(struct S3C24xxTimers* nand);





#endif
