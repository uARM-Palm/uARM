//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_PwrClk.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define S3C24XX_CLOCK_PWR_MGR_BASE	0x4c000000UL
#define S3C24XX_CLOCK_PWR_MGR_SIZE	0x1c


struct S3C24xxPwrClk {
	
	uint32_t locktime, mpllcon, upllcon, clkcon;
	uint16_t camdivn;
	uint8_t clkslow, clkdivn;
};


static bool s3c24xxPwrClkPrvClockMgrMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxPwrClk *cpm = (struct S3C24xxPwrClk*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_CLOCK_PWR_MGR_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch(pa){
		
		case 0x00 / 4:
			if (write)
				cpm->locktime = val;
			else
				val = cpm->locktime;
			break;
		
		case 0x04 / 4:
			if (write)
				cpm->mpllcon = val & 0x000ff3f3ul;
			else
				val = cpm->mpllcon;
			break;
		
		case 0x08 / 4:
			if (write)
				cpm->upllcon = val & 0x000ff3f3ul;
			else
				val = cpm->upllcon;
			break;
		
		case 0x0c / 4:
			if (write)
				cpm->clkcon = val;
			else
				val = cpm->clkcon;
			break;
		
		case 0x10 / 4:
			if (write)
				cpm->clkslow = val & 0xb3;
			else
				val = cpm->clkslow;
			break;
		
		case 0x14 / 4:
			if (write)
				cpm->clkdivn = val & 0x0f;
			else
				val = cpm->clkdivn;
			break;
		
		case 0x18 / 4:
			if (write)
				cpm->camdivn = val & 0x131f;
			else
				val = cpm->camdivn;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct S3C24xxPwrClk* s3c24xxPwrClkInit(struct ArmMem *physMem)
{
	struct S3C24xxPwrClk *cpm = (struct S3C24xxPwrClk*)malloc(sizeof(*cpm));
	
	if (!cpm)
		ERR("cannot alloc Clocks & Power manager");
	
	memset(cpm, 0, sizeof (*cpm));
	
	cpm->locktime = 0x00fffffful;
	cpm->mpllcon = 0x0005c080ul;
	cpm->upllcon = 0x00028080ul;
	cpm->clkcon = 0x00fffff0ul;
	cpm->clkslow = 0x04;
	
	
	if (!memRegionAdd(physMem, S3C24XX_CLOCK_PWR_MGR_BASE, S3C24XX_CLOCK_PWR_MGR_SIZE, s3c24xxPwrClkPrvClockMgrMemAccessF, cpm))
		ERR("cannot add CLKMGR to MEM\n");
	
	return cpm;
}


