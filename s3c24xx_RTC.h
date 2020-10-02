//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_RTC_H_
#define _S3C24XX_RTC_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"

struct S3C24xxRtc;



struct S3C24xxRtc* s3c24xxRtcInit(struct ArmMem *physMem, struct SocIc *ic);

void s3c24xxRtcPeriodic(struct S3C24xxRtc *adc);


#endif

