//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_RTC_H_
#define _PXA_RTC_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"

struct PxaRtc;



struct PxaRtc* pxaRtcInit(struct ArmMem *physMem, struct SocIc *ic);
void pxaRtcUpdate(struct PxaRtc* rtc);


#endif

