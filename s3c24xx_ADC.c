//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_ADC.h"
#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define S3C24XX_ADC_BASE			0x58000000UL
#define S3C24XX_ADC_SIZE			0x18


struct S3C24xxAdc {

	struct SocIc *ic;
	
	//regs
	uint16_t adccon, adcdly, adcdat[2], adctsc;
	uint8_t adcupdn;
	
	//state
	uint8_t busy		: 2;
	uint8_t datRdy		: 2;
	
	//inputs
	uint8_t penDown		: 1;
	uint8_t sentPenDn	: 1;
	uint16_t penX, penY;
	uint16_t aux[8];
};

static void s3c24xxAdcPrvResult(struct S3C24xxAdc *adc, uint_fast8_t regIdx, uint_fast16_t val)
{
	val &= 0x3ff;
	val |= (adc->adctsc & 7) << 12;
	
	adc->adcdat[regIdx] = val;
	adc->datRdy |= 1 << regIdx;
}

static void s3c24xxAdcPrvGo(struct S3C24xxAdc *adc)
{
	if (adc->busy)
		ERR("adc already busy\n");
	adc->busy = 3;
}

static bool s3c24xxAdcPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxAdc *adc = (struct S3C24xxAdc*)userData;
	uint32_t val = 0;
	
	if ((size != 4 && size != 2) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_ADC_BASE) >> 2;

	if (write) {
		
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	//	fprintf(stderr, "ADC write 0x%08x -> [0x%08x]\n", val, pa * 4 + S3C24XX_ADC_BASE);
	}
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write) {
				adc->adccon = val & 0x7ffe;
				if ((val & 1) && !(adc->adccon & 1))
					s3c24xxAdcPrvGo(adc);
			}
			else
				val = adc->adccon | (adc->datRdy ? 0x8000 : 0x0000);
			break;
		
		case 0x04 / 4:
			if (write)
				adc->adctsc = val & 0x1ff;
			else
				val = adc->adctsc;
			break;
		
		case 0x08 / 4:
			if (write)
				adc->adcdly = val & 0xff;
			else
				val = adc->adcdly;
			break;
		
		case 0x0c / 4:
		case 0x10 / 4:
			if (write)
				return false;
			else {
				val = adc->adcdat[pa - 0x0c / 4];
				if (!adc->penDown)
					val |= 0x8000;
				adc->datRdy &=~ (1 << (pa - 0x0c / 4));
			}
			break;
		
		case 0x14 / 4:
			if (write)
				adc->adcupdn = val & 3;
			else
				val = adc->adcupdn;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
		
		
	//	fprintf(stderr, "ADC read [0x%08x] -> 0x%08x\n", pa * 4 + S3C24XX_ADC_BASE, val);
	}
	
	return true;
}

void s3c24xxAdcPeriodic(struct S3C24xxAdc *adc)
{
	if (adc->adccon & 4)	///stanby mode means we do nothing
		return;
	
	if ((adc->adctsc & 0x100) && !adc->penDown)
		adc->adcupdn |= 0x02;
	else if (!(adc->adctsc & 0x100) && adc->penDown)
		adc->adcupdn |= 0x01;
	
	if (adc->busy && !--adc->busy) {
		
		switch (adc->adctsc & 3) {
			
			case 0:
			case 3:
				if (adc->adctsc & 4) {
					s3c24xxAdcPrvResult(adc, 0, adc->penX);
					s3c24xxAdcPrvResult(adc, 1, adc->penY);
				}
				else
					s3c24xxAdcPrvResult(adc, 0, adc->aux[(adc->adccon >> 3) & 7]);
				break;
			
			case 1:
				s3c24xxAdcPrvResult(adc, 0, adc->penX);
				break;
			
			case 2:
				s3c24xxAdcPrvResult(adc, 1, adc->penY);
				break;
		}
		socIcInt(adc->ic, S3C24XX_I_ADC, true);
	}
}

struct S3C24xxAdc* s3c24xxAdcInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct S3C24xxAdc *adc = (struct S3C24xxAdc*)malloc(sizeof(*adc));
	
	if (!adc)
		ERR("cannot alloc ADC");
	
	memset(adc, 0, sizeof (*adc));
	adc->ic = ic;
	
	adc->adccon = 0x3fc4;
	adc->adcdly = 0x00ff;
	adc->adctsc = 0x58;
	
	if (!memRegionAdd(physMem, S3C24XX_ADC_BASE, S3C24XX_ADC_SIZE, s3c24xxAdcPrvMemAccessF, adc))
		ERR("cannot add ADC to MEM\n");
	
	return adc;
}

void s3c24xxAdcSetPenPos(struct S3C24xxAdc *adc, int_fast16_t x, int_fast16_t y)
{
	adc->penDown = x >= 0 && y >= 0;
	adc->penX = x;
	adc->penY = y;
	
	if (!adc->sentPenDn != !adc->penDown && (adc->adctsc & 3)) {	//pen down status changed
		
		if (!(adc->adctsc & 0x100) == !adc->sentPenDn)
			socIcInt(adc->ic, S3C24XX_I_TC, true);
	}
}

void s3c24xxAdcSetAuxAdc(struct S3C24xxAdc *adc, uint_fast8_t idx, uint16_t mV)
{
	if (idx >= sizeof(adc->aux) / sizeof(*adc->aux))
		return;
	
	adc->aux[idx] = (((uint32_t)mV) * 0x400 + (3300 / 2)) / 3300;
}	



