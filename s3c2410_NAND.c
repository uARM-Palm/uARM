//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_NAND.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define S3C2410_NAND_BASE	0x4e000000UL
#define S3C2410_NAND_SIZE	0x18


struct S3C24xxNand {
	
	struct NAND *nand;
	uint16_t nfconf;
	uint8_t ecc[3];
	
	//ecc state
	uint16_t byteCnt;
};

static void s3c24xxNandPrvEccUpdate(struct S3C24xxNand *nand, uint_fast8_t val)		//reverse engineered from trying various data on the chip - could be wrong but i suspect is right
{
	uint_fast8_t t, i;
	
	if (nand->byteCnt >= 512)
		return;

	//1 bits - evens
	t = val & 0x55;
	t ^= t >> 4;
	t ^= t >> 2;
	if (t & 1)
		nand->ecc[2] ^= 1 << 2;
	
	//1 bits - odds
	t = val & 0xaa;
	t ^= t >> 4;
	t ^= t >> 2;
	if (t & 2)
		nand->ecc[2] ^= 1 << 3;
	
	//2 bits - evens
	t = val & 0x33;
	t ^= t >> 4;
	t ^= t >> 1;
	if (t & 1)
		nand->ecc[2] ^= 1 << 4;
	
	//2 bits - odds
	t = val & 0xcc;
	t ^= t >> 4;
	t ^= t >> 1;
	if (t & 4)
		nand->ecc[2] ^= 1 << 5;
	
	//4 bits - evens
	t = val & 0x0f;
	t ^= t >> 2;
	t ^= t >> 1;
	if (t & 1)
		nand->ecc[2] ^= 1 << 6;
	
	//4 bits - odds
	t = val & 0xf0;
	t ^= t >> 2;
	t ^= t >> 1;
	if (t & 16)
		nand->ecc[2] ^= 1 << 7;
	
	//8 bit sum
	t = val;
	t ^= t >> 4;
	t ^= t >> 2;
	t ^= t >> 1;
	t = t & 1;
	
	for (i = 0; i < 9; i++) {
		
		if ((nand->byteCnt >> i) & 1)
			nand->ecc[i / 4] ^= t << ((i % 4) * 2 + 1);
		else
			nand->ecc[i / 4] ^= t << ((i % 4) * 2 + 0);
	}
	nand->byteCnt++;
}

static void s3c24xxNandPrvEccReset(struct S3C24xxNand *nand)
{
	uint_fast8_t i;
	
	for (i = 0; i < 3; i++)
		nand->ecc[i] = 0xff;
	nand->byteCnt = 0;
}


static bool s3c24xxNandPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxNand *nand = (struct S3C24xxNand*)userData;
	uint32_t val = 0;
	
	if (size != 4 && size != 1) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = pa - S3C2410_NAND_BASE;
	
	if (write)
		val = (size == 1) ? *(uint8_t*)buf : *(uint32_t*)buf;
	
	switch(pa){
		
		case 0x00:
			if (write) {
				if (size != 4)
					return false;
				nand->nfconf = val & 0x9f77;
			}
			else
				val = nand->nfconf;
			break;
		
		case 0x04:
			if (write) {
				s3c24xxNandPrvEccReset(nand);
				return nand->nand && nandWrite(nand->nand, true, false, val);
			}
			else
				return false;
			break;
		
		case 0x08:
			if (write) {
				s3c24xxNandPrvEccReset(nand);
				return nand->nand && nandWrite(nand->nand, false, true, val);
			}
			else
				return false;
			break;
		
		case 0x0c:
			if (write) {
				if (!nand->nand || !nandWrite(nand->nand, false, false, val))
					return false;
			}
			else {
				uint8_t val8;
				
				if (!nand->nand || !nandRead(nand->nand, false, false, &val8))
					return false;
				
				val = val8;
			}
			s3c24xxNandPrvEccUpdate(nand, val);
			break;
		
		case 0x10:
			if (write)
				;			//write not allowed - ignored
			else
				val = (nand->nand && nandIsReady(nand->nand)) ? 1 : 0;
			break;
		
		case 0x14:
			if (write)
				;			//write not allowed - ignored
			else if (size == 1)
				val = nand->ecc[0];
			else if (size == 4)
				val = (((uint32_t)nand->ecc[2]) << 16) | (((uint32_t)nand->ecc[1]) << 8) | nand->ecc[0];
			else
				return false;
			break;
		
		case 0x15:
			if (write || size != 1)
				return false;
			val = nand->ecc[1];
			break;
		
		case 0x16:
			if (write || size != 1)
				return false;
			val = nand->ecc[2];
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 1)
			*(uint8_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

struct S3C24xxNand* s3c24xxNandInit(struct ArmMem *physMem, struct NAND *nandChip, struct SocIc *ic, struct SocGpio *gpio)
{
	struct S3C24xxNand *nand = (struct S3C24xxNand*)malloc(sizeof(*nand));
	uint_fast8_t i;
	
	if (!nand)
		ERR("cannot alloc NAND unit");
	
	memset(nand, 0, sizeof (*nand));
	
	nand->nand = nandChip;
	
	if (!memRegionAdd(physMem, S3C2410_NAND_BASE, S3C2410_NAND_SIZE, s3c24xxNandPrvMemAccessF, nand))
		ERR("cannot add NAND unit to MEM\n");
	
	return nand;
}

void s3c24xxNandPeriodic(struct S3C24xxNand* nand)
{
	if (nand->nand)
		nandPeriodic(nand->nand);
}


