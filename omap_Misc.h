//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_MISC_H_
#define _OMAP_MISC_H_


#include <stdint.h>
#include "mem.h"

struct OmapMisc;



struct OmapMisc* omapMiscInit(struct ArmMem *physMem);

bool omapMiscIsTimerAtCpuSpeed(struct OmapMisc *misc);	//timers either run as cpu speed or 12mhz speed


#endif
