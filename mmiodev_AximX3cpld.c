//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_AximX3cpld.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define AXIM_X3_CPLD_BASE	0x08000000ul
#define AXIM_X3_CPLD_SIZE 	0x04


struct AximX3cpld {
	
	uint32_t val;
};



static bool aximX3cpldPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct AximX3cpld *cpld = (struct AximX3cpld*)userData;
	uint32_t val;
	
	pa -= AXIM_X3_CPLD_BASE;
	
	if(size != 4 || !write || pa) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	val = *(uint32_t*)buf;
	
	if (cpld->val != val)
		fprintf(stderr, " * CPLD 0x%08lx -> 0x%08lx\n", (unsigned long)cpld->val, (unsigned long)val);
	cpld->val = val;
	return true;
}
	

struct AximX3cpld* aximX3cpldInit(struct ArmMem *physMem)
{
	struct AximX3cpld* cpld = (struct AximX3cpld*)malloc(sizeof(*cpld));
	
	if (!cpld)
		ERR("cannot alloc AXIM's CPLD");
	
	memset(cpld, 0, sizeof (*cpld));
	
	if (!memRegionAdd(physMem, AXIM_X3_CPLD_BASE, AXIM_X3_CPLD_SIZE, aximX3cpldPrvMemAccessF, cpld))
		ERR("cannot add AXIM's CPLD to MEM\n");
	
	return cpld;
}
