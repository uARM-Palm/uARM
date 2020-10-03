//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_WDT.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_WDT_BASE		0xFFFEC800UL
#define OMAP_WDT_SIZE		12


struct OmapWdt {
	struct SocIc *ic;
	
	uint16_t ctrl, counter, reloadVal;
	uint8_t prescaleCtr;
	
	bool wdtMode, hadDisableFirstHalf;
};

static bool omapWdtPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapWdt *wdt = (struct OmapWdt*)userData;
	uint_fast16_t val = 0;
	
	if (size != 2 || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - OMAP_WDT_BASE) >> 2;
	
	if (write)
		val = *(uint16_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write) {
				if (wdt->wdtMode)
					val = val | 0xe00;
				
				if (!(wdt->ctrl & 0x80) && (val & 0x80))	//starting
					wdt->counter = wdt->reloadVal;
				
				wdt->ctrl = val;
			}
			else
				val = wdt->ctrl;
			break;
		
		case 0x04 / 4:
			if (write) {
				if (wdt->wdtMode)
					wdt->counter = val;
				else
					wdt->reloadVal = val;
			}
			else
				val = wdt->counter;
			break;
		
		case 0x08 / 4:
			if (write) {
				if (val & 0x8000) {
					wdt->ctrl |= 0x0e00;
					wdt->wdtMode = true;
					wdt->counter = 0xffff;
					wdt->reloadVal = 0xffff;
				}
				else if (wdt->hadDisableFirstHalf) {
					if ((val & 0xff) == 0xA0)
						wdt->wdtMode = false;
					else
						ERR("improper WDT disablement\n");
					wdt->hadDisableFirstHalf = false;
				}
				else if ((val & 0xff) == 0xf5)
					wdt->hadDisableFirstHalf = true;
			}
			else
				val = wdt->wdtMode ? 0x8000 : 0x0000;
			break;
	
		default:
			return false;
	}
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return true;
}

struct OmapWdt* omapWdtInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct OmapWdt *wdt = (struct OmapWdt*)malloc(sizeof(*wdt));
	
	if (!wdt)
		ERR("cannot alloc WDT");
	
	memset(wdt, 0, sizeof (*wdt));
	
	wdt->ic = ic;
	wdt->ctrl = 0x0002;
	wdt->counter = 0xffff;
	wdt->reloadVal = 0xffff;
	wdt->wdtMode = true;
	
	if (!memRegionAdd(physMem, OMAP_WDT_BASE, OMAP_WDT_SIZE, omapWdtPrvMemAccessF, wdt))
		ERR("cannot add WDT to MEM\n");
	
	return wdt;
}

void omapWdtPeriodic(struct OmapWdt *wdt)
{
	if (!wdt->wdtMode && !(wdt->ctrl & 0x0080))	//non-WDT mode timer can be stopped
		return;
	
	if (++wdt->prescaleCtr >= (2 << ((wdt->ctrl >> 9) & 7))) {
		wdt->prescaleCtr = 0;
		if (wdt->counter)
			wdt->counter--;
		else {
			
			if (wdt->wdtMode)
				ERR("WDT reset\n");
			else {
				
				if (wdt->ctrl & 0x0100)
					wdt->counter = wdt->reloadVal;
				else
					wdt->ctrl &=~ 0x0080;
				//edge triggered
				socIcInt(wdt->ic, OMAP_I_WDT, true);
				socIcInt(wdt->ic, OMAP_I_WDT, false);
			}
		}
	}
}
