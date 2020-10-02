//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_RTC_H_
#define _OMAP_RTC_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"

struct OmapRtc;



struct OmapRtc* omapRtcInit(struct ArmMem *physMem, struct SocIc *ic);
void omapRtcPeriodic(struct OmapRtc *rtc);



#endif
