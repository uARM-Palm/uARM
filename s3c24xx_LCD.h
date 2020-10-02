//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_LCD_H_
#define _S3C24XX_LCD_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"

struct S3C24xxLcd;



struct S3C24xxLcd* s3c24xxLcdInit(struct ArmMem *physMem, struct SocIc *ic, bool hardGrafArea);
void s3c24xxLcdPeriodic(struct S3C24xxLcd *lcd);


#endif

