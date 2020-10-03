//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_ULPD.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_ULPD_BASE		0xFFFE0800UL
#define OMAP_ULPD_SIZE		2048


struct OmapUlpd {
	struct SocIc *ic;
	
	uint32_t lowFreqDivider;
	uint32_t highFreqCtr, lowFreqCtr;
	
	uint16_t SETUP_ANALOG_CELL3_ULPD1_REG;
	uint16_t CLOCK_CTRL_REG;
	
	uint8_t SOFT_REQ_REG, POWER_CTRL_REG;
};

//p 1138
static bool omapUlpdPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapUlpd *ulpd = (struct OmapUlpd*)userData;
	uint32_t val = 0;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - OMAP_ULPD_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x24 / 4:
			if (write)
				ulpd->SETUP_ANALOG_CELL3_ULPD1_REG = val;
			else
				val = ulpd->SETUP_ANALOG_CELL3_ULPD1_REG;
			break;
		
		case 0x30 / 4:
			if (write)
				ulpd->CLOCK_CTRL_REG = val;
			else
				val = ulpd->CLOCK_CTRL_REG;
			break;
		
		case 0x34 / 4:
			if (write)
				ulpd->SOFT_REQ_REG = val & 0x1f;
			else
				val = ulpd->SOFT_REQ_REG;
			break;
		
		case 0x50 / 4:
			if (write)
				ulpd->POWER_CTRL_REG = val & 0x0f;
			else
				val = ulpd->POWER_CTRL_REG;
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

struct OmapUlpd* omapUlpdInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct OmapUlpd *ulpd = (struct OmapUlpd*)malloc(sizeof(*ulpd));
	
	if (!ulpd)
		ERR("cannot alloc ULPD");
	
	memset(ulpd, 0, sizeof (*ulpd));
	
	ulpd->ic = ic;
	
	ulpd->SETUP_ANALOG_CELL3_ULPD1_REG = 0x03ff;
	ulpd->CLOCK_CTRL_REG = 0x0010;
	ulpd->SOFT_REQ_REG = 0x10;
	ulpd->POWER_CTRL_REG = 0x08;
	
	if (!memRegionAdd(physMem, OMAP_ULPD_BASE, OMAP_ULPD_SIZE, omapUlpdPrvMemAccessF, ulpd))
		ERR("cannot add ULPD to MEM\n");
	
	return ulpd;
}

void omapUlpdPeriodic(struct OmapUlpd *ulpd)
{
	ulpd->highFreqCtr++;
	if (++ulpd->lowFreqDivider == 375)
		ulpd->lowFreqCtr++;
	
	//TODO
}
