//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_ADC_H_
#define _S3C24XX_ADC_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"

struct S3C24xxAdc;



struct S3C24xxAdc* s3c24xxAdcInit(struct ArmMem *physMem, struct SocIc *ic);

void s3c24xxAdcPeriodic(struct S3C24xxAdc *adc);

void s3c24xxAdcSetPenPos(struct S3C24xxAdc *adc, int_fast16_t x, int_fast16_t y);	//negative for pen up
void s3c24xxAdcSetAuxAdc(struct S3C24xxAdc *adc, uint_fast8_t idx, uint16_t mV);	

#endif

