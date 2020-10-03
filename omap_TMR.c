//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_TMR.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"

#define OMAP_TMR_SIZE			0x10

struct OmapTmr {
	struct OmapMisc *misc;
	struct SocIc *ic;
	int8_t irqNo;
	
	uint8_t prescaleCtr, ctrl;
	uint32_t counter, reloadVal;
	
	//cpu clock rescaler;
	uint8_t cpuCtr;
};



static bool omapTmrPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapTmr *tmr = (struct OmapTmr*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa % OMAP_TMR_SIZE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write) {
				if (!(tmr->ctrl & 1) && (val & 1))	//starting
					tmr->counter = tmr->reloadVal;
				tmr->ctrl = val & 0x7f;
			}
			else
				val = tmr->ctrl;
			break;
		
		case 0x04 / 4:
			if (write)
				tmr->reloadVal = val;
			else
				return false;
			break;
		
		case 0x08 / 4:
			if (write)
				return false;
			else
				val = tmr->counter;
			break;
	
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct OmapTmr* omapTmrInit(struct ArmMem *physMem, struct SocIc *ic, struct OmapMisc *misc, uint32_t base, int8_t irqNo)
{
	struct OmapTmr *tmr = (struct OmapTmr*)malloc(sizeof(*tmr));
	
	if (!tmr)
		ERR("cannot alloc TMR@0x%08lx", (unsigned long)base);
	
	memset(tmr, 0, sizeof (*tmr));
	
	tmr->misc = misc;
	tmr->ic = ic;
	tmr->irqNo = irqNo;
	
	if (!memRegionAdd(physMem, base, OMAP_TMR_SIZE, omapTmrPrvMemAccessF, tmr))
		ERR("cannot add TMR@0x%08lx to MEM\n", (unsigned long)base);
	
	return tmr;
}

bool omapMiscIsTimerAtCpuSpeed(struct OmapMisc *misc);	//timers either run as cpu speed or 12mhz speed



void omapTmrPeriodic(struct OmapTmr *tmr)
{
	if (!(tmr->ctrl & 1))
		return;
	
	if (!omapMiscIsTimerAtCpuSpeed(tmr->misc) && ++tmr->cpuCtr < 8)	//pretend cpu is at 96 mhz
		return;
	
	tmr->cpuCtr = 0;
	
	if ((++tmr->prescaleCtr >= (2 << ((tmr->ctrl >> 2) & 7)))) {
		tmr->prescaleCtr = 0;
		
		if (tmr->counter)
			tmr->counter--;
		else {
			
			if (tmr->ctrl & 0x02)
				tmr->counter = tmr->reloadVal;
			else
				tmr->ctrl &=~ 1;
			if (tmr->irqNo >= 0) {
				socIcInt(tmr->ic, tmr->irqNo, true);		//edge triggered
				socIcInt(tmr->ic, tmr->irqNo, false);
			}
		}
	}
}
