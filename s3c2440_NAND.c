//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_NAND.h"
#include "s3c24xx_IC.h"
#include "soc_GPIO.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"

//this emulated NAND unit does not support 16-bit NANDs and does not even try
//we'd need to duplicate the ECC logic, etc
//it also does not support range locking of NAND. It could be done but i couldn't be bothered

#define S3C2440_NAND_BASE	0x4e000000UL
#define S3C2440_NAND_SIZE	0x40


struct S3C24xxNand {
	
	struct SocIc *ic;
	
	struct NAND *nand;
	uint32_t nfsblk, nfeblk;
	uint16_t nfconf, nfcont;
	uint8_t nfstat;
	uint8_t eccm[4];
	uint8_t eccs[2];
	
	//ecc state
	uint8_t byteCntS;
	uint16_t byteCntM;
	
	//ECC we read
	uint32_t meccd[2], seccd;
	
	bool nandWasReady;
};

static uint_fast8_t s3c24xxNandPrvEccCalcBits(uint_fast8_t val)	//{sum_bits p4 p4' p2 p2' p1 p1'}
{
	uint_fast8_t t, ret = 0;
	
	//1 bits - evens
	t = val & 0x55;
	t ^= t >> 4;
	t ^= t >> 2;
	if (t & 1)
		ret += 1 << 0;
	
	//1 bits - odds
	t = val & 0xaa;
	t ^= t >> 4;
	t ^= t >> 2;
	if (t & 2)
		ret += 1 << 1;
	
	//2 bits - evens
	t = val & 0x33;
	t ^= t >> 4;
	t ^= t >> 1;
	if (t & 1)
		ret += 1 << 2;
	
	//2 bits - odds
	t = val & 0xcc;
	t ^= t >> 4;
	t ^= t >> 1;
	if (t & 4)
		ret += 1 << 3;
	
	//4 bits - evens
	t = val & 0x0f;
	t ^= t >> 2;
	t ^= t >> 1;
	if (t & 1)
		ret += 1 << 4;
	
	//4 bits - odds
	t = val & 0xf0;
	t ^= t >> 2;
	t ^= t >> 1;
	if (t & 16)
		ret += 1 << 5;
	
	//sumbits
	t = val;
	t ^= t >> 4;
	t ^= t >> 2;
	t ^= t >> 1;
	t = t & 1;
	if (t)
		ret += 1 << 7;
	
	return ret;
}

static void s3c24xxNandPrvEccsUpdate(struct S3C24xxNand *nand, uint_fast8_t val)
{
	uint_fast8_t i, t = s3c24xxNandPrvEccCalcBits(val);

	nand->eccs[0] ^= (t & 0x3c) >> 2;
	nand->eccs[1] ^= t << 6;
	t >>= 7;					//8-bit sum
	
	for (i = 0; i < 4; i++) {
		
		uint_fast8_t pos = i ? 3 : i + 4;
		
		if ((nand->byteCntS >> i) & 1)
			nand->eccs[pos / 4] ^= t << ((pos % 4) * 2 + 1);
		else
			nand->eccs[pos / 4] ^= t << ((pos % 4) * 2 + 0);
	}
	nand->byteCntS++;
}

static void s3c24xxNandPrvEccmUpdate(struct S3C24xxNand *nand, uint_fast8_t val)		//reverse engineered from trying various data on the chip - could be wrong but i suspect is right
{
	uint_fast8_t i, t = s3c24xxNandPrvEccCalcBits(val);

	nand->eccm[2] ^= t << 2;	//put P1..P4 into place
	t >>= 7;					//8-bit sum
	
	for (i = 0; i < 11; i++) {
		
		uint_fast8_t pos = (i < 9) ? i : i + 5;
		
		if ((nand->byteCntM >> i) & 1)
			nand->eccm[pos / 4] ^= t << ((pos % 4) * 2 + 1);
		else
			nand->eccm[pos / 4] ^= t << ((pos % 4) * 2 + 0);
	}
	nand->byteCntM++;
}

static void s3c24xxNandPrvEccReset(struct S3C24xxNand *nand)
{
	uint_fast8_t i;
	
	for (i = 0; i < sizeof(nand->eccs); i++)
		nand->eccs[i] = 0xff;
	
	for (i = 0; i < sizeof(nand->eccm); i++)
		nand->eccm[i] = 0xff;
	
	nand->byteCntS = 0;
	nand->byteCntM = 0;
}

static void s3c24xxNandPrvEccProcessByte(struct S3C24xxNand *nand, uint8_t byte)
{
	if (!(nand->nfcont & 0x40))
		s3c24xxNandPrvEccsUpdate(nand, byte);
	if (!(nand->nfcont & 0x20))
		s3c24xxNandPrvEccmUpdate(nand, byte);
}

static bool s3c24xxNandPrvDataR(struct S3C24xxNand *nand, uint_fast8_t size, uint32_t *valP)
{
	uint32_t val;
	uint8_t val8;
				
	if (!nandRead(nand->nand, false, false, &val8))
		return false;
	val = val8;
	s3c24xxNandPrvEccProcessByte(nand, val8);
	
	if (size >= 2) {
	
		if (!nandRead(nand->nand, false, false, &val8))
			return false;
		val += ((uint32_t)val8) << 8;
		s3c24xxNandPrvEccProcessByte(nand, val8);
		
		if (size == 4) {
		
			if (!nandRead(nand->nand, false, false, &val8))
				return false;
			val += ((uint32_t)val8) << 16;
			s3c24xxNandPrvEccProcessByte(nand, val8);
			
			if (!nandRead(nand->nand, false, false, &val8))
				return false;
			val += ((uint32_t)val8) << 24;
			s3c24xxNandPrvEccProcessByte(nand, val8);
		}
	}
	*valP = val;
	
	return true;
}

static bool s3c24xxNandPrvDataW(struct S3C24xxNand *nand, uint_fast8_t size, uint32_t val)
{
	if (!nandWrite(nand->nand, false, false, (uint8_t)val))
		return false;
	s3c24xxNandPrvEccProcessByte(nand, val);
	
	if (size >= 2) {
	
		if (!nandWrite(nand->nand, false, false, (uint8_t)(val >> 8)))
			return false;
		s3c24xxNandPrvEccProcessByte(nand, (uint8_t)(val >> 8));
		
		if (size == 4) {
		
			if (!nandWrite(nand->nand, false, false, (uint8_t)(val >> 16)))
				return false;
			s3c24xxNandPrvEccProcessByte(nand, (uint8_t)(val >> 16));
			
			if (!nandWrite(nand->nand, false, false, (uint8_t)(val >> 24)))
				return false;
			s3c24xxNandPrvEccProcessByte(nand, (uint8_t)(val >> 24));
		}
	}
	return true;
}

enum S3C2440nandEccErrorType {
	S3C2440nandEccNoError = 0,
	S3C2440nandEccCorrectibleError = 1,
	S3C2440nandEccUncorrectibleError = 2,
	S3C2440nandEccErrorInEccData = 3,
};

static enum S3C2440nandEccErrorType s3c24xxNandPrvEccCommonOps(uint8_t *diffs, const uint8_t *correctEccs, const uint32_t *readEccs, uint_fast8_t nbytes, uint_fast8_t nLastBitsToIgnore)
{
	uint_fast8_t i, ignoreMask = (1 << nLastBitsToIgnore) - 1, nDiffs = 0;
	bool dataCorrectible = true, dataPerfect = true;
	
	//split up the read values into our byte array format
	for (i = 0; i < nbytes / 2; i++) {
		diffs[i * 2 + 0] = readEccs[i];
		diffs[i * 2 + 1] = readEccs[i] >> 16;
	}
	
	//clean up bits that do not matter by coping from calced
	diffs[nbytes - 1] = (diffs[nbytes - 1] &~ ignoreMask) | (correctEccs[nbytes - 1] & ignoreMask);
	
	//calc differences
	for (i = 0; i < nbytes; i++)
		diffs[i] ^= correctEccs[i];
	
	//see if we have a correctable error. also count set bits to see if ecc data might be in error
	for (i = 0; i < nbytes; i++) {
		if (diffs[i])
			dataPerfect = false;
		if (((diffs[i] & 0x55) ^ ((diffs[i] >> 1) & 0x55)) != 0x55)
			dataCorrectible = false;
		if (diffs[i]) {
			if (diffs[i] & (diffs[i] - 1))
				nDiffs += 2;		//at least two bits set
			else
				nDiffs++;			//precisely one bit set
		}
	}
	
	if (dataPerfect)
		return S3C2440nandEccNoError;
	
	if (dataCorrectible)
		return S3C2440nandEccCorrectibleError;
	
	if (nDiffs == 1)
		return S3C2440nandEccErrorInEccData;
	
	return S3C2440nandEccUncorrectibleError;
}

static uint32_t s3c24xxNandPrvCorrect(struct S3C24xxNand *nand)
{
	uint8_t diffM[sizeof(nand->eccm)], diffS[sizeof(nand->eccs)];
	enum S3C2440nandEccErrorType eTypeM, eTypeS;
	uint_fast8_t i, errBitNo;
	uint_fast16_t errByteNo;
	uint32_t ret = 0;
	
	//sort out the kind of error we have, calculate differences
	eTypeM = s3c24xxNandPrvEccCommonOps(diffM, nand->eccm, nand->meccd, sizeof(diffM), 4);
	eTypeS = s3c24xxNandPrvEccCommonOps(diffS, nand->eccs, &nand->seccd, sizeof(diffS), 2);
	
	//record this in the output
	ret |= ((uint32_t)eTypeM) << 0;
	ret |= ((uint32_t)eTypeS) << 2;
	
	//correct as needed
	if (eTypeM == S3C2440nandEccCorrectibleError) {
		
		errByteNo = 0;
		errBitNo = 0;
		
		//calc bit no
		if (diffM[2] & 0x04)
			errBitNo += 1;
		if (diffM[2] & 0x10)
			errBitNo += 2;
		if (diffM[2] & 0x40)
			errBitNo += 4;
		
		//calc byte no
		for (i = 0; i < 9; i++) {
			if (diffM[i / 4] & (1 << (2 * (i % 4))))
				errByteNo += 1 << i;
		}
		if (diffM[3] & 0x10)
			errByteNo += 512;
		if (diffM[3] & 0x40)
			errByteNo += 1024;
		
		//record
		ret |= ((uint32_t)errBitNo) << 4;
		ret |= ((uint32_t)errByteNo) << 7;
	}
	
	if (eTypeS == S3C2440nandEccCorrectibleError) {
		
		errByteNo = 0;
		errBitNo = 0;
		
		//calc bit no
		if (diffS[1] & 0x40)
			errBitNo += 1;
		if (diffS[0] & 0x01)
			errBitNo += 2;
		if (diffS[0] & 0x04)
			errBitNo += 4;
		
		//calc byte no
		if (diffS[0] & 0x10)
			errByteNo += 1;
		if (diffS[0] & 0x40)
			errByteNo += 2;
		if (diffS[1] & 0x04)
			errByteNo += 4;
		if (diffS[1] & 0x10)
			errByteNo += 8;
		
		//record
		ret |= ((uint32_t)errBitNo) << 18;
		ret |= ((uint32_t)errByteNo) << 21;
	}
	
	return ret;
}

static bool s3c24xxNandPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxNand *nand = (struct S3C24xxNand*)userData;
	uint32_t val = 0;
	
	pa = pa - S3C2440_NAND_BASE;
	
	if (write) {
		if (size == 1)
			val = *(uint8_t*)buf;
		else if (size == 2)
			val = *(uint16_t*)buf;
		else if (size == 4)
			val = *(uint32_t*)buf;
		else
			return false;
	}
	
	switch(pa){
		
		case 0x00:
			if (write) {
				if (size == 1)
					return false;
				nand->nfconf = (nand->nfconf & 0x000f) | (val & 0x3770);
			}
			else
				val = nand->nfconf;
			break;
		
		case 0x04:
			if (write) {
				if (size == 1)
					return false;
				nand->nfcont = (nand->nfcont & 0x2000) | (val & 0x3763);	//lock tight stays set if it was ever set
				
				if (val & 0x10)
					s3c24xxNandPrvEccReset(nand);
			}
			else
				val = nand->nfcont;
			break;
		
		case 0x08:
			if (write)
				return nand->nand && nandWrite(nand->nand, true, false, val);
			else
				return false;
			break;
		
		case 0x0c:
			if (write)
				return nand->nand && nandWrite(nand->nand, false, true, val);
			else
				return false;
			break;
		
		case 0x10:
			if (!nand->nand)
				return false;
				
			if (write) {
				
				if (!s3c24xxNandPrvDataW(nand, size, val))
					return false;
			}
			else {
				
				if (!s3c24xxNandPrvDataR(nand, size, &val))
					return false;
			}
			break;
		
		case 0x14:		//these read as normal reads to data reg, but they also record values read for later correction calculation
		case 0x18:
		case 0x1c:
			if (!nand->nand)
				return false;
			if (size != 4)
				return false;
			if (!write && !s3c24xxNandPrvDataR(nand, size, &val))
				return false;
			//user can write ECC values they got elsewhere or can use this reg to read them same as NFDATA reg, IIUC
			if (pa == 0x1c)
				nand->seccd = val;
			else if (pa == 0x18)
				nand->meccd[1] = val;
			else
				nand->meccd[0] = val;
			break;
		
		case 0x20:
			if (write)
				nand->nfstat &=~ (val & 0x0c);
			else
				val = nand->nfstat | (nand->nfcont & 0x02) | ((nand->nand && nandIsReady(nand->nand)) ? 0x01 : 0x00);
			break;
		
		case 0x24:
			if (write)
				return false;
			if (size != 4)
				return false;
			val = s3c24xxNandPrvCorrect(nand);
			break;

		case 0x28:
			if (write)
				return false;
			if (size != 4)
				return false;
			val = 0;			//we provide no 16-bit support
			break;

		case 0x2c:
			if (write)
				;	//write ignored
			else if (size != 4)
				return false;
			else
				val = (((uint32_t)nand->eccm[3]) << 24) | (((uint32_t)nand->eccm[2]) << 16) | (((uint32_t)nand->eccm[1]) << 8) | nand->eccm[0];
			break;
		
		case 0x30:
			if (write)
				;	//write ignored
			else
				val = 0;
			break;
		
		case 0x34:
			if (write)
				;	//write ignored
			else if (size != 4)
				return false;
			else
				val = (((uint32_t)nand->eccs[1]) << 8) | nand->eccs[0];
			break;

		case 0x38:
			if (size != 4)
				return false;
			if (!write)
				val = nand->nfsblk;
			else if (nand->nfcont & 0x2000)	//NFSBLK is locked
				return false;
			else
				nand->nfsblk = val & 0x0ffffe0ul;
			break;

		case 0x3c:
			if (size != 4)
				return false;
			if (!write)
				val = nand->nfeblk;
			else if (nand->nfcont & 0x2000)	//NFEBLK is locked
				return false;
			else
				nand->nfeblk = val & 0x0ffffe0ul;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 1)
			*(uint8_t*)buf = val;
		else if (size == 1)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

static void s3c24xxNandPrvNandReadyCbk(void *userData, bool ready)
{
	struct S3C24xxNand *nand = (struct S3C24xxNand*)userData;
	
	if (((nand->nfcont & 0x100) && nand->nandWasReady && !ready) ||			//we were looking for the falling edge and we found it
		(!(nand->nfcont & 0x100) && !nand->nandWasReady && ready)) {		//we were looking for the rising edge and we found it
		nand->nfstat |= 4;
		
		if (nand->nfcont & 0x200)											//int enabled?
			socIcInt(nand->ic, S3C2440_I_NAND, true);
	}
}

struct S3C24xxNand* s3c24xxNandInit(struct ArmMem *physMem, struct NAND *nandChip, struct SocIc *ic, struct SocGpio *gpio)
{
	struct S3C24xxNand *nand = (struct S3C24xxNand*)malloc(sizeof(*nand));
	uint_fast8_t i;
	
	if (!nand)
		ERR("cannot alloc NAND unit");
	
	memset(nand, 0, sizeof (*nand));
	nand->nand = nandChip;
	nand->nfcont = 0x0262;
	nand->ic = ic;
	
	//setup NFCONF's initial value
	nand->nfconf = 0x1000;
	if (socGpioGetState(gpio, 162) == SocGpioStateHigh)	//NCON
		nand->nfconf |= 0x08;
	if (socGpioGetState(gpio, 125) == SocGpioStateHigh)	//GPG13
		nand->nfconf |= 0x04;
	if (socGpioGetState(gpio, 126) == SocGpioStateHigh)	//GPG14
		nand->nfconf |= 0x02;
	if (socGpioGetState(gpio, 127) == SocGpioStateHigh)	//GPG15
		nand->nfconf |= 0x01;
	
	if (nandChip) {
		nandSecondReadyCbkSet(nandChip, s3c24xxNandPrvNandReadyCbk, nand);
		nand->nandWasReady = nandIsReady(nandChip);
	}
	
	if (!memRegionAdd(physMem, S3C2440_NAND_BASE, S3C2440_NAND_SIZE, s3c24xxNandPrvMemAccessF, nand))
		ERR("cannot add NAND unit to MEM\n");
	
	return nand;
}

void s3c24xxNandPeriodic(struct S3C24xxNand* nand)
{
	if (nand->nand)
		nandPeriodic(nand->nand);
}


