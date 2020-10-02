//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_RTC.h"
#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define S3C24XX_RTC_BASE			0x57000000UL
#define S3C24XX_RTC_SIZE			0x8c


struct S3C24xxRtc {

	struct SocIc *ic;
	
	//regs
	uint8_t rtccon, ticnt, rtcalm, almsec, almmin, almhour, almdate, almmon, almyear;
	uint8_t rtcrst, bcdsec, bcdmin, bcdhour, bcddate, bcdday, bcdmon, bcdyear;
	
	
	//state
	uint8_t ticntCounter, rtcCnt;
};

static bool s3c24xxRtcPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxRtc *rtc = (struct S3C24xxRtc*)userData;
	uint32_t val = 0;
	
	if ((size != 1 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_RTC_BASE) >> 2;

	if (write)
		val = (size == 1) ? *(uint8_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x40 / 4:
			if (write)
				rtc->rtccon = val & 0x0f;
			else
				val = rtc->rtccon;
			break;
		
		case 0x44 / 4:
			if (write) {
				rtc->ticntCounter = val & 0x7f;
				rtc->ticnt = val & 0xff;
			}
			else
				val = rtc->ticnt;
			break;
		
		case 0x50 / 4:
			if (write)
				rtc->rtcalm = val & 0x7f;
			else
				val = rtc->rtcalm;
			break;
		
		case 0x54 / 4:
			if (write)
				rtc->almsec = val & 0x7f;
			else
				val = rtc->almsec;
			break;
		
		case 0x58 / 4:
			if (write)
				rtc->almmin = val & 0x7f;
			else
				val = rtc->almmin;
			break;
		
		case 0x5c / 4:
			if (write)
				rtc->almhour = val & 0x3f;
			else
				val = rtc->almhour;
			break;
		
		case 0x60 / 4:
			if (write)
				rtc->almdate = val & 0x3f;
			else
				val = rtc->almdate;
			break;
		
		case 0x64 / 4:
			if (write)
				rtc->almmon = val & 0x1f;
			else
				val = rtc->almmon;
			break;
		
		case 0x68 / 4:
			if (write)
				rtc->almyear = val & 0xff;
			else
				val = rtc->almyear;
			break;
		
		case 0x6c / 4:
			if (write)
				rtc->rtcrst = val & 0x0f;
			else
				val = rtc->rtcrst;
			break;
		
		case 0x70 / 4:
			if (write)
				rtc->bcdsec = val & 0x7f;
			else
				val = rtc->bcdsec;
			break;
		
		case 0x74 / 4:
			if (write)
				rtc->bcdmin = val & 0x7f;
			else
				val = rtc->bcdmin;
			break;
		
		case 0x78 / 4:
			if (write)
				rtc->bcdhour = val & 0x3f;
			else
				val = rtc->bcdhour;
			break;
		
		case 0x7c / 4:
			if (write)
				rtc->bcddate = val & 0x3f;
			else
				val = rtc->bcddate;
			break;
		
		case 0x80 / 4:
			if (write)
				rtc->bcdday = val & 0x07;
			else
				val = rtc->bcdday;
			break;
		
		case 0x84 / 4:
			if (write)
				rtc->bcdmon = val & 0x1f;
			else
				val = rtc->bcdmon;
			break;
		
		case 0x88 / 4:
			if (write)
				rtc->bcdyear = val & 0xff;
			else
				val = rtc->bcdyear;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 1)
			*(uint8_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

static bool s3c24xxRtcPrvBcdIncrement(uint8_t *valP, uint_fast8_t min, uint_fast8_t max)
{
	uint_fast8_t val = 10 * (*valP >> 4) + (*valP & 0x0f);
	bool ret = false;
	
	if (val == max) {
		ret = true;
		val = min;
	}
	else
		val++;
	
	*valP = (val / 10) * 16 + val % 10;
	return ret;
}


static uint_fast8_t s3c24xxRtcPrvNumCurMonthDays(struct S3C24xxRtc *rtc)
{
	uint_fast8_t month = 10 * (rtc->bcddate >> 4) + (rtc->bcddate & 0x0f);
	uint_fast8_t year = 10 * (rtc->bcdyear >> 4) + (rtc->bcdyear & 0x0f);
	
	switch (month) {
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		
		case 2:
			return (year % 4) ? 28 : 29;
		
		default:
			__builtin_unreachable();
			return 0;
	}
}

void s3c24xxRtcPeriodic(struct S3C24xxRtc *rtc)
{
	//tick
	if (rtc->ticnt & 0x80) {
		if (rtc->ticntCounter)
			rtc->ticntCounter--;
		else {
			rtc->ticntCounter = rtc->ticnt & 0x7f;
			socIcInt(rtc->ic, S3C24XX_I_TICK, true);
		}
	}
	
	//RTC time
	if (++rtc->rtcCnt >= 128) {
		
		rtc->rtcCnt = 0;
		
		if (s3c24xxRtcPrvBcdIncrement(&rtc->bcdsec, 0, 60) &&
				s3c24xxRtcPrvBcdIncrement(&rtc->bcdmin, 0, 60) &&
				s3c24xxRtcPrvBcdIncrement(&rtc->bcdhour, 0, 24)) {
			
			(void)s3c24xxRtcPrvBcdIncrement(&rtc->bcdday, 1, 7);
			
			if (s3c24xxRtcPrvBcdIncrement(&rtc->bcddate, 1, s3c24xxRtcPrvNumCurMonthDays(rtc)) && s3c24xxRtcPrvBcdIncrement(&rtc->bcdmon, 1, 12))
				s3c24xxRtcPrvBcdIncrement(&rtc->bcdyear, 0, 100);
		}
	}
	
	//RTC alarm
	if (rtc->rtcalm & 0x40) {
		
		if ((!(rtc->rtcalm & 0x20) || (rtc->bcdyear == rtc->almyear)) &&
				(!(rtc->rtcalm & 0x10) || (rtc->bcdmon == rtc->almmon)) &&
				(!(rtc->rtcalm & 0x08) || (rtc->bcddate == rtc->almdate)) &&
				(!(rtc->rtcalm & 0x04) || (rtc->bcdhour == rtc->almhour)) &&
				(!(rtc->rtcalm & 0x02) || (rtc->bcdmin == rtc->almmin)) &&
				(!(rtc->rtcalm & 0x01) || (rtc->bcdsec == rtc->almsec)))
			socIcInt(rtc->ic, S3C24XX_I_RTC, true);
	}	
}

struct S3C24xxRtc* s3c24xxRtcInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct S3C24xxRtc *rtc = (struct S3C24xxRtc*)malloc(sizeof(*rtc));
	
	if (!rtc)
		ERR("cannot alloc RTC");
	
	memset(rtc, 0, sizeof (*rtc));
	rtc->ic = ic;
	rtc->bcdyear = 0x20;
	rtc->bcdmon = 0x09;
	rtc->bcddate = 0x19;
	rtc->bcdday = 0x06;
	
	if (!memRegionAdd(physMem, S3C24XX_RTC_BASE, S3C24XX_RTC_SIZE, s3c24xxRtcPrvMemAccessF, rtc))
		ERR("cannot add RTC to ADC\n");
	
	return rtc;
}

