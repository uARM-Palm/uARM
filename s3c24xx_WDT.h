//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_WDT_H_
#define _S3C24XX_WDT_H_



#include <stdint.h>
#include "soc_IC.h"
#include "mem.h"

struct S3C24xxWdt;



struct S3C24xxWdt* s3c24xxWdtInit(struct ArmMem *physMem, struct SocIc *ic, uint_fast8_t socRev);

void s3c24xxWdtPeriodic(struct S3C24xxWdt *wdt);	//once every 2 cpu cycles


#endif
