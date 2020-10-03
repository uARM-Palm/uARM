//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_MemCtrl.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define S3C24XX_MEM_CTRL_BASE	0x48000000UL
#define S3C24XX_MEM_CTRL_SIZE	0x34


struct S3C24xxMemCtrl {
	
	uint32_t bwscon, bankcon[8], refresh;
	uint16_t msrb[2];
	uint8_t banksize;
};


static bool s3c24xxMemCtrlPrvClockMgrMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxMemCtrl *mc = (struct S3C24xxMemCtrl*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - S3C24XX_MEM_CTRL_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch(pa){
		
		case 0x00 / 4:
			if (write)
				mc->bwscon = val & 0xfffffff6ul;
			else
				val = mc->bwscon;
			break;
		
		case 0x04 / 4:
		case 0x08 / 4:
		case 0x0c / 4:
		case 0x10 / 4:
		case 0x14 / 4:
		case 0x18 / 4:
		case 0x1c / 4:
		case 0x20 / 4:
			if (write)
				mc->bankcon[pa - 0x04 / 4] = val & 0x0001fffful;
			else
				val = mc->bankcon[pa - 0x04 / 4];
			break;
		
		case 0x24 / 4:
			if (write)
				mc->refresh = val & 0x00fc07fful;
			else
				val = mc->refresh;
			break;
		
		case 0x28 / 4:
			if (write)
				mc->banksize = val & 0xb7;
			else
				val = mc->banksize;
			break;
		
		case 0x2c / 4:
		case 0x30 / 4:
			if (write)
				mc->msrb[pa - 0x2c / 4] = val & 0x03ff;
			else
				val = mc->msrb[pa - 0x2c / 4];
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct S3C24xxMemCtrl* s3c24xxMemCtrlInit(struct ArmMem *physMem)
{
	struct S3C24xxMemCtrl *mc = (struct S3C24xxMemCtrl*)malloc(sizeof(*mc));
	uint_fast8_t i;
	
	if (!mc)
		ERR("cannot alloc memory controller");
	
	memset(mc, 0, sizeof (*mc));
	
	for (i = 0; i < 8; i++)
		mc->bankcon[i] = 0x0700;
	mc->refresh = 0x00ac0000ul;
	
	if (!memRegionAdd(physMem, S3C24XX_MEM_CTRL_BASE, S3C24XX_MEM_CTRL_SIZE, s3c24xxMemCtrlPrvClockMgrMemAccessF, mc))
		ERR("cannot add MEM CTRL to MEM\n");
	
	return mc;
}


