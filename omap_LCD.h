//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_LCD_H_
#define _OMAP_LCD_H_

#include "soc_DMA.h"
#include "soc_IC.h"
#include "mem.h"

struct OmapLcd;



struct OmapLcd* omapLcdInit(struct ArmMem *physMem, struct SocIc *ic, bool hardGrafArea);
void omapLcdPeriodic(struct OmapLcd *lcd);



#endif
