//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_MemoryStickController.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define MS_CONTROLLER_SIZE 	0x400


struct MemoryStickController {
	
	uint16_t systemReg;
	
	//not sure
	uint16_t reg_0, reg_40, reg_50, reg_2c0, reg_2d0, reg_2e0, reg_2f0;

	//some sort of flags. w1c i think
	uint16_t reg_188, reg_180;
	
	//seems to be command results (64 bits in 2 32-bit regs)
	uint32_t reply_lo_2f0, reply_hi_2f8;

};

//reg 0x08 seems to be "system register" top 16 bits (IP calls it system reg 0x18)

static bool msCtrlrPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct MemoryStickController *msc = (struct MemoryStickController*)userData;
	uint32_t val = 0;
	bool ret = true;
	
	pa %= MS_CONTROLLER_SIZE;
	
	if (size == 4 && !write && (pa == 0x2f0 || pa == 0x2f8)) {
		
		*(uint32_t*)buf = (pa  == 0x2f0) ? msc->reply_lo_2f0 : msc->reply_hi_2f8;
		fprintf(stderr, "MSC %c [%04lx] == 0x%08lx\n", (char)(write ? 'W' : 'R'), (unsigned long)pa, (unsigned long)*(uint32_t*)buf);
		return true;
	}
	
	if (size != 2) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	if (write)
		val = *(uint16_t*)buf;
	
	switch (pa) {
		
		case 0x00:
			if (write)
				msc->reg_0 = val;
			else
				val = msc->reg_0;
			break;
		
		case 0x08:	//system reg top 16 bits?
			if (write)
				msc->systemReg = (msc->systemReg & 0x8a00) | (val & 0x75ff);
				//XXX: writing 0x0800 clears XINT too
			else
				val = msc->systemReg;
			break;
		
		case 0x40:
			if (write)
				msc->reg_40 = val;
			else
				val = msc->reg_40;
			break;
		
		case 0x50:
			if (write)
				msc->reg_50 = val;
			else
				val = msc->reg_50;
			break;
		
		case 0x180:
			if (write)
				msc->reg_180 &=~ val;
			else
				val = msc->reg_180;
			break;
		
		case 0x188:
			if (write)
				msc->reg_188 &=~ val;
			else
				val = msc->reg_188;
			break;
		
		case 0x2c0:
			if (write)
				msc->reg_2c0 = val;
			else
				val = msc->reg_2c0;
			break;
		
		case 0x2d0:
			if (write)
				msc->reg_2d0 = val &~ 1;
			else
				val = msc->reg_2d0;
			break;
		
		case 0x2e0:	//some sort of command, assembled from 2 bytes in 3452MGLib@7f0
			if (write)
				msc->reg_2e0 = val;
			else
				val = msc->reg_2e0;
			break;
		
		case 0x2f0:
			if (write)
				msc->reg_2f0 = val;
			else
				val = msc->reg_2f0;
			break;
		
		default:
			fprintf(stderr, "unknown reg 0x%08lx\n", (unsigned long)pa);
			ret = false;
			break;
	}
	
	if (ret)
		fprintf(stderr, "MSC %c [%04lx] == 0x%04lx\n", (char)(write ? 'W' : 'R'), (unsigned long)pa, (unsigned long)val);
	else
		fprintf(stderr, "MSC %c [%04lx] FAILED\n", (char)(write ? 'W' : 'R'), (unsigned long)pa);
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return ret;
}

struct MemoryStickController* msCtrlrInit(struct ArmMem *physMem, uint32_t baseAddr)
{
	struct MemoryStickController *msc = (struct MemoryStickController*)malloc(sizeof(*msc));
	
	if (!msc)
		ERR("cannot alloc MSC");
	
	memset(msc, 0, sizeof (*msc));
	
	msc->systemReg = 0x4455;
	
	if (!memRegionAdd(physMem, baseAddr, MS_CONTROLLER_SIZE, msCtrlrPrvMemAccessF, msc))
		ERR("cannot add MSC to MEM\n");
	
	return msc;
}

