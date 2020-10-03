//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_32kTMR.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"

#define OMAP_32kTMR_BASE			0xFFFB9000UL
#define OMAP_32kTMR_SIZE			0x10

struct Omap32kTmr {
	struct SocIc *ic;
	
	uint8_t cr;
	uint32_t counter, reloadVal;
};



static bool omap32kTmrPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct Omap32kTmr *tmr = (struct Omap32kTmr*)userData;
	uint32_t val = 0;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa % OMAP_32kTMR_SIZE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				tmr->reloadVal = val & 0xfffffful;
			else
				val = tmr->reloadVal;
			break;
		
		case 0x04 / 4:
			if (write)
				return false;
			else
				val = tmr->counter;
			break;
		
		case 0x08 / 4:
			if (write) {
				if ((val & 1) && !(tmr->cr & 1))	//just turned on - reload the reload value now!
					tmr->counter = tmr->reloadVal;
				tmr->cr = val & 0x0d;
				if (val & 2)
					tmr->counter = tmr->reloadVal;
			}
			else
				val = tmr->cr;
			break;
	
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

struct Omap32kTmr* omap32kTmrInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct Omap32kTmr *tmr = (struct Omap32kTmr*)malloc(sizeof(*tmr));
	
	if (!tmr)
		ERR("cannot alloc 32kTMR");
	
	memset(tmr, 0, sizeof (*tmr));
	
	tmr->ic = ic;
	tmr->cr = 8;
	tmr->counter = 0xfffffful;
	tmr->reloadVal = 0xfffffful;
	
	if (!memRegionAdd(physMem, OMAP_32kTMR_BASE, OMAP_32kTMR_SIZE, omap32kTmrPrvMemAccessF, tmr))
		ERR("cannot add 32kTMR to MEM\n");
	
	return tmr;
}

void omap32kTmrPeriodic(struct Omap32kTmr *tmr)
{
	if (tmr->cr & 1) {
		
		if (tmr->counter)
			tmr->counter--;
		else {
			
			if (tmr->cr & 0x08) {
				tmr->counter = tmr->reloadVal;
				if (!tmr->reloadVal)
					ERR("reload to zero?\n");
			}
			else
				tmr->cr &=~ 1;
			if (tmr->cr & 4) {
				//edge
				socIcInt(tmr->ic, OMAP_I_TIMER32K, true);
				socIcInt(tmr->ic, OMAP_I_TIMER32K, false);
			}
		}
	}
}
