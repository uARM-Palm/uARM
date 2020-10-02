//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_WDT.h"
#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define S3C24XX_WDT_BASE		0x53000000UL
#define S3C24XX_WDT_SIZE		12


struct S3C24xxWdt {
	struct SocIc *ic;
	
	//regs
	uint16_t wtcon, wtdat, wtcnt;
	
	//state
	uint16_t prescalerLimit, prescalerCounter;
	
	bool soc40;
};

static void s3c24xxWdtPrvRecalcPrescalerLimit(struct S3C24xxWdt *wdt)
{
	wdt->prescalerLimit = ((uint16_t)(((wdt->wtcon >> 8) & 0xff) + 1)) << (((wdt->wtcon >> 3) & 3) + 4);
}

static bool s3c24xxWdtPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxWdt *wdt = (struct S3C24xxWdt*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_WDT_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write) {
				wdt->wtcon = val & 0xff3d;
				s3c24xxWdtPrvRecalcPrescalerLimit(wdt);
			}
			else
				val = wdt->wtcon;
			break;
		
		case 0x04 / 4:
			if (write)
				wdt->wtdat = wdt->wtcnt = val & 0xffff;	//guess. docs are atrocious
			else
				val = wdt->wtdat;
			break;
		
		case 0x08 / 4:
			if (write)
				wdt->wtcnt = val & 0xffff;
			else
				val = wdt->wtcnt;
			break;
	
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct S3C24xxWdt* s3c24xxWdtInit(struct ArmMem *physMem, struct SocIc *ic, uint_fast8_t socRev)
{
	struct S3C24xxWdt *wdt = (struct S3C24xxWdt*)malloc(sizeof(*wdt));
	
	if (!wdt)
		ERR("cannot alloc WDT");
	
	memset(wdt, 0, sizeof (*wdt));
	
	wdt->ic = ic;
	wdt->wtcon = 0x8021;
	wdt->wtdat = 0x8000;
	wdt->wtcnt = 0x8000;
	wdt->soc40 = !!socRev;
	
	s3c24xxWdtPrvRecalcPrescalerLimit(wdt);
	
	if (!memRegionAdd(physMem, S3C24XX_WDT_BASE, S3C24XX_WDT_SIZE, s3c24xxWdtPrvMemAccessF, wdt))
		ERR("cannot add WDT to MEM\n");
	
	return wdt;
}

void s3c24xxWdtPeriodic(struct S3C24xxWdt *wdt)
{
	if (!(wdt->wtcon & 0x20))		//enabled?
		return;
	
	if (++wdt->prescalerCounter >= wdt->prescalerLimit) {
		wdt->prescalerCounter = 0;
		
		if (wdt->wtcnt)
			wdt->wtcnt--;
		else {
			
			if (wdt->wtcon & 0x01)
				ERR("WDT resets device\n");
			
			if (wdt->wtcon & 0x04)
				socIcInt(wdt->ic, wdt->soc40 ? S3C2440_I_WDT : S3C2410_I_WDT, true);
		}
	}
}


