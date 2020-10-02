//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA_LCD_H_
#define _PXA_LCD_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"

struct PxaLcd;



struct PxaLcd* pxaLcdInit(struct ArmMem *physMem, struct SocIc *ic, bool hardGrafArea);
void pxaLcdFrame(struct PxaLcd *lcd);


#endif

