//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_PWL.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"

#define OMAP_PWL_BASE			0xFFFB5800UL
#define OMAP_PWL_SIZE			8

struct OmapPwl {
	
	uint8_t level, ctrl;
};



static bool omapPwlPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapPwl *pwl = (struct OmapPwl*)userData;
	uint32_t val = 0;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_PWL_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				pwl->level = val & 0xff;
			else
				val = pwl->level;
			break;
		
		case 0x04 / 4:
			if (write)
				pwl->ctrl = val & 1;
			else
				val = pwl->ctrl;
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

struct OmapPwl* omapPwlInit(struct ArmMem *physMem)
{
	struct OmapPwl *pwl = (struct OmapPwl*)malloc(sizeof(*pwl));
	
	if (!pwl)
		ERR("cannot alloc PWL");
	
	memset(pwl, 0, sizeof (*pwl));
	
	if (!memRegionAdd(physMem, OMAP_PWL_BASE, OMAP_PWL_SIZE, omapPwlPrvMemAccessF, pwl))
		ERR("cannot add PWL to MEM\n");
	
	return pwl;
}
