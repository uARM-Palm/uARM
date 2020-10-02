//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_PWT.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"

#define OMAP_PWT_BASE			0xFFFB6000UL
#define OMAP_PWT_SIZE			12

struct OmapPwt {
	
	uint8_t freq, vol, ctrl;
};



static bool omapPwtPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapPwt *pwt = (struct OmapPwt*)userData;
	uint32_t val = 0;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_PWT_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				pwt->freq = val & 0x3f;
			else
				val = pwt->freq;
			break;
		
		case 0x04 / 4:
			if (write)
				pwt->vol = val & 0x7f;
			else
				val = pwt->vol;
			break;
	
		case 0x08 / 4:
			if (write)
				pwt->ctrl = val & 0x03;
			else
				val = pwt->ctrl;
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

struct OmapPwt* omapPwtInit(struct ArmMem *physMem)
{
	struct OmapPwt *pwt = (struct OmapPwt*)malloc(sizeof(*pwt));
	
	if (!pwt)
		ERR("cannot alloc PWT");
	
	memset(pwt, 0, sizeof (*pwt));
	
	if (!memRegionAdd(physMem, OMAP_PWT_BASE, OMAP_PWT_SIZE, omapPwtPrvMemAccessF, pwt))
		ERR("cannot add PWT to MEM\n");
	
	return pwt;
}
