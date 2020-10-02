//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_SDIO.h"
#include "s3c24xx_DMA.h"
#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"



#define S3C24XX_SDIO_BASE	0x5a000000UL
#define S3C24XX_SDIO_SIZE	0x50


struct S3C24xxSdio {
	
	struct SocDma *dma;
	struct SocIc *ic;
	struct VSD* vsd;
	bool soc40;
	
	uint32_t arg, resp[4], sdidtimer, sdidcon, sdiimsk;
	uint16_t cmd, sdicsta, blklen, sdidatsta, sdifsta;
	uint8_t sdicon, sdipre;
};

static void s3c24xxSdioPrvFifoReset(struct S3C24xxSdio *sdio)
{
	//nothing to do
}

static void s3c24xxSdioPrvUnitReset(struct S3C24xxSdio *sdio)
{
	//nothing to do. [yet?]
}

static void s3c24xxSdioPrvRecalc(struct S3C24xxSdio *sdio)
{
	bool irq = false;
	
	irq = irq || ((sdio->sdiimsk & (sdio->sdifsta >> 7))   & 0x0000003ful);
	irq = irq || ((sdio->sdiimsk & (sdio->sdidatsta << 3)) & 0x000037c0ul);
	irq = irq || ((sdio->sdiimsk & (sdio->sdicsta << 5))   & 0x0003c000ul);
	
	//not addressed - NoBusyInt - S2440 only
	//not addressed - FFfailInt - do our fifs even fail?
	
	socIcInt(sdio->ic, S3C24XX_I_SDIO, !!irq);
}

static bool s3c24xxSdioPrvFifoW(struct S3C24xxSdio *sdio, uint32_t val, uint_fast8_t size)
{
	ERR("SDIO FIFO W unimpl\n");
	
	return false;
}

static bool s3c24xxSdioPrvFifoR(struct S3C24xxSdio *sdio, uint32_t *valP, uint_fast8_t size)
{
	ERR("SDIO FIFO R unimpl\n");
	
	return false;
}

static void s3c24xxSdioPrvDoCommand(struct S3C24xxSdio *sdio)
{
	ERR("SDIO CMD unimpl\n");
}

static bool s3c24xxSdioPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxSdio *sdio = (struct S3C24xxSdio*)userData;
	uint32_t val = 0;
	
	pa = pa - S3C24XX_SDIO_BASE;
	
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
				sdio->sdicon = val & 0x3d;
				if (val & 2)
					s3c24xxSdioPrvFifoReset(sdio);
				if (val & 0x100)
					s3c24xxSdioPrvUnitReset(sdio);
			}
			else
				val = sdio->sdicon;
			break;
		
		case 0x04:
			if (write)
				sdio->sdipre = val & 0xff;
			else
				val = sdio->sdipre;
			break;
		
		case 0x08:
			if (size != 4)
				return false;
			if (write)
				sdio->arg = val;
			else
				val = sdio->arg;
			break;
		
		case 0x0c:
			if (size < 2)
				return false;
			if (write) {
				sdio->cmd = val & 0x1fef;
				if (!(sdio->cmd & 0x100) && (val & 0x100))
					s3c24xxSdioPrvDoCommand(sdio);
			}
			else
				val = sdio->cmd;
			break;
			
		case 0x10:
			if (size < 2)
				return false;
			if (write) {
				sdio->sdicsta &=~ (val & 0x1e00);
				s3c24xxSdioPrvRecalc(sdio);
			}
			else
				val = sdio->sdicsta;
			break;
		
		case 0x14:
		case 0x18:
		case 0x1c:
		case 0x20:
			if (size != 4)
				return false;
			if (write)
				return false;
			val = sdio->resp[(pa - 0x14) / 4];
			break;
		
		case 0x24:
			if (size < 2)
				return false;
			if (write)
				sdio->sdidtimer = val & 0x007ffffful;
			else
				val = sdio->sdidtimer;
			break;
		
		case 0x28:
			if (size < 2)
				return false;
			if (write)
				sdio->blklen = val & 0x0fff;
			else
				val = sdio->blklen;
			break;
		
		case 0x2c:
			if (size != 4)
				return false;
			if (write)
				sdio->sdidcon = val & 0x01fffffful;
			else
				val = sdio->sdidcon;
			//XXX: bit 14 changes meaning radicatlly between 2410 and 2440 here, bits 22+are 2440 only
			break;
			
		case 0x30:
			if (size != 4)
				return false;
			if (write)
				return false;
			val = 0;
			ERR("not implemented: SDIDCNT\n");
			break;
		
		case 0x34:
			if (size < 2)
				return false;
			if (write) {
				sdio->sdidatsta &=~ (val & 0x0ef8);
				s3c24xxSdioPrvRecalc(sdio);
			}
			else
				val = sdio->sdidatsta;
			//XXX: bit 8 here is 2410 only
			break;
		
		case 0x38:
			if (size < 2)
				return false;
			if (write) {
				sdio->sdifsta &=~ (val & 0xc200);
				if (val & 0x10000ul)
					s3c24xxSdioPrvFifoReset(sdio);
				s3c24xxSdioPrvRecalc(sdio);
			}
			else
				val = sdio->sdifsta;
			break;
		
		case 0x3c:
			if (!sdio->soc40)
				goto fifo_access;
	mask_access:
			if (size != 4)
				return false;
			if (write) {
				sdio->sdiimsk = val & 0x3ffdful;
				s3c24xxSdioPrvRecalc(sdio);
			}
			else
				val = sdio->sdiimsk;
			break;
		
		case 0x44:
		case 0x48:
		case 0x4c:
			if (!sdio->soc40)
				return false;
			//fallthrough
		
		case 0x40:
			if (!sdio->soc40)
				goto mask_access;
	fifo_access:
			if (write && !s3c24xxSdioPrvFifoW(sdio, val, size))
				return false;
			else if (!write && !s3c24xxSdioPrvFifoR(sdio, &val, size))
				return false;
			s3c24xxSdioPrvRecalc(sdio);
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

struct S3C24xxSdio* s3c24xxSdioInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma, uint_fast8_t socRev)
{
	struct S3C24xxSdio *sdio = (struct S3C24xxSdio*)malloc(sizeof(*sdio));
	uint_fast8_t i;
	
	if (!sdio)
		ERR("cannot alloc SDIO unit");
	
	memset(sdio, 0, sizeof (*sdio));
	sdio->dma = dma;
	sdio->ic = ic;
	sdio->soc40 = !!socRev;
	
	s3c24xxSdioPrvUnitReset(sdio);
	
	if (!memRegionAdd(physMem, S3C24XX_SDIO_BASE, S3C24XX_SDIO_SIZE, s3c24xxSdioPrvMemAccessF, sdio))
		ERR("cannot add SDIO unit to MEM\n");
	
	return sdio;
}

void s3c24xxSdioInsert(struct S3C24xxSdio *sdio, struct VSD* vsd)
{
	sdio->vsd = vsd;
}


