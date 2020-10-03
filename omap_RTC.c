//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_RTC.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_RTC_BASE		0xFFFB4800UL
#define OMAP_RTC_SIZE		256

struct Time {
	uint8_t sec, min, hr, day, month, yr;
};

struct OmapRtc {

	struct Time now;
	uint8_t nowWeekday;
	
	struct Time alarm;

	uint8_t ctrl, sta, intr, compL, compH;

	struct SocIc *ic;
};

static void omapRtcPrvIrqUpdate(struct OmapRtc *rtc, bool sendEvtIrqs)
{
	bool tick = false;
	
	if (!(rtc->intr & 8) || !(rtc->sta & 0x40))
		socIcInt(rtc->ic, OMAP_I_RTC_ALM, false);

	if (sendEvtIrqs && (rtc->intr & 4)) {
		switch (rtc->intr & 3){
			case 0:	//every second
				tick = true;
				break;
			
			case 1:	//every minute
				tick = !rtc->now.sec;
				break;
			
			case 2:	//every hour
				tick = !rtc->now.min;
				break;
			
			case 3:	//every day
				tick = rtc->now.hr == ((rtc->ctrl & 8) ? 12 : 0);
				break;
		
		}
		if (tick) {
			//edge
			socIcInt(rtc->ic, OMAP_I_RTC_TICK, true);
			socIcInt(rtc->ic, OMAP_I_RTC_TICK, false);
		}
	}
}

static uint_fast8_t omapRtcPrvNumCurMonthDays(struct OmapRtc *rtc, struct Time *tm)
{
	uint_fast8_t month = 10 * (tm->month >> 4) + (tm->month & 0x0f);
	uint_fast8_t year = 10 * (tm->yr >> 4) + (tm->yr & 0x0f);
	
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

static void omapRtcPrvNormalizeBcd(uint8_t *valP, uint_fast8_t min, uint_fast8_t max)
{
	uint_fast8_t val = 10 * (*valP >> 4) + (*valP & 0x0f);
	
	if (val < min)
		val = min;
	else if (val > max)
		val = max;
	
	*valP = (val / 10) * 16 + val % 10;
}

static void omapRtcPrvNormalizeTime(struct OmapRtc *rtc, struct Time *tm)
{
	uint8_t monthDays;
	
	omapRtcPrvNormalizeBcd(&tm->sec, 0, 59);
	omapRtcPrvNormalizeBcd(&tm->min, 0, 59);
	if (rtc->ctrl & 8) {	//12h
		uint8_t amPm = tm->hr & 0x80;
		tm->hr &= 0x7f;
		omapRtcPrvNormalizeBcd(&tm->hr, 1, 12);
		tm->hr |= amPm;
	}
	else
		omapRtcPrvNormalizeBcd(&tm->hr, 0, 23);
	omapRtcPrvNormalizeBcd(&tm->yr, 0, 99);
	omapRtcPrvNormalizeBcd(&tm->month, 1, 12);
	omapRtcPrvNormalizeBcd(&tm->day, 1, omapRtcPrvNumCurMonthDays(rtc, tm));
}

static uint_fast8_t omapRtcPrvHours12to24(uint_fast8_t hr)
{
	uint_fast8_t tm = 10 * ((hr & 0x70) >> 4) + (hr & 0x0f);
	if (tm == 12)
		tm = 0;
	if (hr & 0x80)
		tm += 12;
	
	return (tm / 10) * 16 + tm % 10;
}

static uint_fast8_t omapRtcPrvHours24to12(uint_fast8_t hr)
{
	uint_fast8_t amPm = 0;
	
	hr = (hr / 16 * 10) + hr % 16;
	if (hr >= 12) {
		amPm = 0x80;
		hr -= 12;
	}
	if (!hr)
		hr = 12;
	
	return hr / 10 * 16 + hr % 10 + amPm;
}

static bool omapRtcPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapRtc *rtc = (struct OmapRtc*)userData;
	uint_fast8_t val = 0;
	
	if ((size != 1 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - OMAP_RTC_BASE) >> 2;
	
	if (write)
		val = size == 1 ? *(uint8_t*)buf : *(uint32_t*)buf;

	if (pa < 0x40 / 4) {
		struct Time *tm = (pa >= 0x20 / 4) ? &rtc->alarm : &rtc->now;
		uint8_t *datumP = NULL;
		
		switch (pa % (0x20 / 4)) {
			case 0x00 / 4:
				datumP = &tm->sec;
				break;
			
			case 0x04 / 4:
				datumP = &tm->min;
				break;
			
			case 0x08 / 4:
				datumP = &tm->hr;
				break;
			
			case 0x0c / 4:
				datumP = &tm->day;
				break;
			
			case 0x10 / 4:
				datumP = &tm->month;
				break;
			
			case 0x14 / 4:
				datumP = &tm->yr;
				break;
			
			case 0x18 / 4:
				if (tm == &rtc->now)
					datumP = &rtc->nowWeekday;
				break;
		}
		
		if (!datumP)
			return false;
		if (write) {
			*datumP = val;
			omapRtcPrvNormalizeTime(rtc, tm);
			rtc->nowWeekday %= 7;
			omapRtcPrvIrqUpdate(rtc, false);
		}
		else
			val = *datumP;
	}
	else switch (pa) {
		
		case 0x40 / 4:
			if (write) {
				if ((val ^ rtc->ctrl) & 8) {	//12/24 hr mode changed
					
					if (val & 8) {				//went to 12h mode
						
						rtc->now.hr = omapRtcPrvHours24to12(rtc->now.hr);
						rtc->alarm.hr = omapRtcPrvHours24to12(rtc->alarm.hr);
					}
					else {
						
						rtc->now.hr = omapRtcPrvHours12to24(rtc->now.hr);
						rtc->alarm.hr = omapRtcPrvHours12to24(rtc->alarm.hr);
					}	
				}
				rtc->ctrl = val & 0x7f;
			}
			else
				val = rtc->ctrl;
			break;
		
		case 0x44 / 4:
			if (write) {
				rtc->sta &=~ (val & 0xc0);
				omapRtcPrvIrqUpdate(rtc, false);
			}
			else
				val = rtc->sta;
			break;
		
		case 0x48 / 4:
			if (write) {
				rtc->intr = val & 0x0f;
				omapRtcPrvIrqUpdate(rtc, false);
			}
			else
				val = rtc->intr;
			break;
		
		case 0x4c / 4:
			if (write)
				rtc->compL = val;
			else
				val = rtc->compL;
			break;
		
		case 0x50 / 4:
			if (write)
				rtc->compH = val;
			else
				val = rtc->compH;
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



struct OmapRtc* omapRtcInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct OmapRtc *rtc = (struct OmapRtc*)malloc(sizeof(*rtc));
	
	if (!rtc)
		ERR("cannot alloc RTC");
	
	memset(rtc, 0, sizeof (*rtc));
	rtc->ic = ic;
	rtc->sta = 0x80;
	rtc->now.sec = 1;
	rtc->now.min = 1;
	rtc->now.hr = 8;
	rtc->now.day = 0x28;
	rtc->now.month = 3;
	rtc->now.yr = 0x88;
	rtc->nowWeekday = 0;
	
	if (!memRegionAdd(physMem, OMAP_RTC_BASE, OMAP_RTC_SIZE, omapRtcPrvMemAccessF, rtc))
		ERR("cannot add RTC to MEM\n");
	
	return rtc;
}

//return true if overflowed
static bool omapRtcPrvBcdIncrement(uint8_t *valP, uint_fast8_t min, uint_fast8_t max)
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


void omapRtcPeriodic(struct OmapRtc *rtc)
{
	//time ticks
	if (omapRtcPrvBcdIncrement(&rtc->now.sec, 0, 59) && omapRtcPrvBcdIncrement(&rtc->now.min, 0, 59)) {
		
		bool goOn;
		
		if (rtc->ctrl & 8) {	//12 h
		
			uint8_t hr = omapRtcPrvHours12to24(rtc->now.hr);
			goOn = omapRtcPrvBcdIncrement(&hr, 0, 23);
			rtc->now.hr = omapRtcPrvHours24to12(hr);
		}
		else
			goOn = omapRtcPrvBcdIncrement(&rtc->now.hr, 0, 23);
		
		if (goOn && omapRtcPrvBcdIncrement(&rtc->now.day, 0, omapRtcPrvNumCurMonthDays(rtc, &rtc->now))) {
			
			//next month
			if (omapRtcPrvBcdIncrement(&rtc->now.month, 1, 12) && omapRtcPrvBcdIncrement(&rtc->now.yr, 0, 99))
				ERR("year overflows");
		}
	}
	
	if (!memcmp(&rtc->now, &rtc->alarm, sizeof(struct Time))) {
		rtc->sta |= 0x40;
		
		if (rtc->intr & 8)
			socIcInt(rtc->ic, OMAP_I_RTC_ALM, true);
	}
	
	omapRtcPrvIrqUpdate(rtc, true);
}

