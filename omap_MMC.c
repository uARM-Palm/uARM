//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_DMA.h"
#include "omap_MMC.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_MMC_BASE	0xFFFB7800UL
#define OMAP_MMC_SIZE	0x60

struct OmapMmc {
	
	struct SocDma *dma;
	struct SocIc *ic;
	struct VSD* vsd;
	
	uint32_t arg;
	uint16_t cmd, con, stat, ie, dto, blen, nblk, bufCfg, spi, sdio, syst, rsp[8];
	uint8_t cto;
	
	//internal status
	uint8_t buffer[2048];
	uint16_t bufferBytes;
	bool wasAcmd, dataXferActive;
};

static void omapMmcPrvRecalcIrqAndDma(struct OmapMmc *mmc)
{
	uint_fast16_t effectiveStat = mmc->stat &~ 0x0c00, bufferAmt;
	bool rxTrigger = false, txTrigger = false;
	
	if (mmc->dataXferActive) {
		
		if (mmc->cmd & 0x8000)		//data read
			rxTrigger = (mmc->bufferBytes + 1 / 2) > ((mmc->bufCfg >> 8) & 0x1f);
		else
			txTrigger = (mmc->bufferBytes + 1 / 2) >= ((mmc->bufCfg >> 0) & 0x1f);
	}
	if (!(mmc->bufCfg & 0x8000)) {	//rx does not use dma
		effectiveStat |= 0x0400;
		rxTrigger = false;
	}
	if (!(mmc->bufCfg & 0x0080)) {	//tx does not use dma
		effectiveStat |= 0x0800;
		txTrigger = false;
	}
	
	socIcInt(mmc->ic, OMAP_I_MMC, mmc->stat & mmc->ie);
	socDmaExternalReq(mmc->dma, DMA_REQ_MMC_RX, rxTrigger);
	socDmaExternalReq(mmc->dma, DMA_REQ_MMC_TX, txTrigger);
}

static void omapMmcPrvDataXferAdvance(struct OmapMmc *mmc)
{
	enum SdDataReplyType repl;
	
	if (!mmc->dataXferActive)
		ERR("data xfer cannot advance if there isn't one going on");
	
	//done?
	if (!mmc->nblk) {
		mmc->dataXferActive = false;
		mmc->stat |= 0x0008;	//Block_RS
		mmc->nblk = 1;		//reset value
		return;
	}
	
	mmc->nblk--;
	if (mmc->cmd & 0x8000) {	//we are reading data
		
		if (mmc->bufferBytes)
			ERR("advancing read while fifo contains data is wrong\n");
		
		repl = vsdDataXferBlockFromCard(mmc->vsd, mmc->buffer, mmc->blen);
		if (repl == SdDataOk)
			mmc->bufferBytes = mmc->blen;
		else
			mmc->stat |= 0x0040;
	}
	else {						//we are writing data
		
		if (mmc->bufferBytes != mmc->blen)
			ERR("advancing write while fifo lacks enough data is wrong\n");
		
		repl = vsdDataXferBlockToCard(mmc->vsd, mmc->buffer, mmc->blen);
		if (repl == SdDataOk)
			mmc->bufferBytes = 0;
		else
			mmc->stat |= 0x0020;	//XXX: is this an ok error val?
	}
	
	omapMmcPrvRecalcIrqAndDma(mmc);
}

static void omapMmcPrvCmdSend(struct OmapMmc *mmc)
{
	uint_fast8_t cmd = mmc->cmd & 0x3f, i;
	enum SdReplyType gotReplyType;
	uint8_t reply[18];
	
	
	mmc->stat &=~ 0x7dfd;
	gotReplyType = vsdCommand(mmc->vsd, cmd, mmc->arg, reply);

	switch ((mmc->cmd >> 8) & 7) {
		case 0:
			if (gotReplyType != SdReplyNone)
				mmc->stat |= 0x0100;	//command crc error
			break;
		case 1:
			if (gotReplyType != ((mmc->cmd & 0x0800) ? SdReply48bitsAndBusy : SdReply48bits))
				mmc->stat |= 0x0100;	//command crc error
			break;
		case 2:
			if (gotReplyType == SdReplyNone)
				mmc->stat |= 0x0080;	//command timed out
			else if (gotReplyType != SdReply136bits)
				mmc->stat |= 0x0100;	//command crc error
			break;
		case 3:
		case 6:
			if (gotReplyType == SdReplyNone)
				mmc->stat |= 0x0080;	//command timed out
			else if (gotReplyType != SdReply48bits)
				mmc->stat |= 0x0100;	//command crc error
			break;
		default:
			fprintf(stderr, "reply type %u unsupported\n", (mmc->cmd >> 8) & 7);
			if (gotReplyType == SdReplyNone)
				mmc->stat |= 0x0080;	//command timed out
			else
				mmc->stat |= 0x0100;	//command crc error
			break;
	}
	fprintf(stderr, "MMC CMD %d(0x%08x) -> %d, %02x %02x %02x %02x %02x %02x\n", cmd, mmc->arg, gotReplyType, reply[0], reply[1], reply[2], reply[3], reply[4], reply[5]);

	if ((mmc->wasAcmd && cmd == 41) || cmd == 1) {	//OCR_busy flag
		if (!(reply[0] & 0x80))
			mmc->stat |= 0x1100;
	}
	
	if (mmc->cmd & 0x0800) {			//busy was expected?
		mmc->stat |= 0x0004;			//busy state entered
		mmc->stat |= 0x0010;			//busy state exited
	}
	
	mmc->stat |= 0x0001;			//command done
	
	if (gotReplyType == SdReply136bits) {
		for (i = 0; i < 8; i++)
			mmc->rsp[i] = (((uint_fast16_t)reply[16 - i * 2 - 2]) << 8) + reply[16 - i * 2 - 1];
	}
	else if (gotReplyType != SdReplyNone) {
		for (i = 0; i < 2; i++)
			mmc->rsp[i + 6] = (((uint_fast16_t)reply[4 - i * 2 - 2 + 1]) << 8) + reply[4 - i * 2 - 1 + 1];
	}
	
	//adtc command? (has data)
	mmc->dataXferActive = ((mmc->cmd >> 12) & 3) == 3;
	if (mmc->dataXferActive && (mmc->cmd & 0x8000))	//read
		omapMmcPrvDataXferAdvance(mmc);
	
	mmc->wasAcmd = cmd == 55;
	omapMmcPrvRecalcIrqAndDma(mmc);
}

static bool omapMmcPrvDoSpiXfer(struct OmapMmc *mmc)
{
	ERR("MMC SPI XFER\n");
	return false;
}

static bool omapMmcPrvFifoW(struct OmapMmc *mmc, uint_fast16_t val)
{
	if (mmc->blen - mmc->bufferBytes < 2)
		ERR("FIFO W when buffer has %u bytes of a %u byte block\n", mmc->bufferBytes, mmc->blen);
	
	mmc->buffer[mmc->bufferBytes++] = val;
	mmc->buffer[mmc->bufferBytes++] = val >> 8;
	
	if (mmc->bufferBytes == mmc->blen)
		omapMmcPrvDataXferAdvance(mmc);

	omapMmcPrvRecalcIrqAndDma(mmc);
	
	return true;
}

static bool omapMmcPrvFifoR(struct OmapMmc *mmc, uint_fast16_t *valP)
{
	if (mmc->bufferBytes < 2)
		ERR("FIFO R when buffer has %u bytes\n", mmc->bufferBytes);
	
	*valP = (((uint_fast16_t)mmc->buffer[1]) << 8) + mmc->buffer[0];
	memmove(mmc->buffer, mmc->buffer + 2, mmc->bufferBytes -= 2);
	
	if (!mmc->bufferBytes)
		omapMmcPrvDataXferAdvance(mmc);

	omapMmcPrvRecalcIrqAndDma(mmc);
	
	return true;
}

static bool omapMmcPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMmc *mmc = (struct OmapMmc*)userData;
	uint_fast16_t val = 0;
	bool ret = true;
	
	if ((size != 4 && size != 2) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_MMC_BASE) >> 2;
	
	if (write) {
		val = size == 2 ? *(uint16_t*)buf : *(uint32_t*)buf;
	//	fprintf(stderr, "MMC 0x%08x->[0x%08x]\n", (unsigned)val, (unsigned)(OMAP_MMC_BASE + 4 * pa));
	}

	switch (pa) {
		
		case 0x00 / 4:	//MMC_CMD
			if (write) {
				mmc->cmd = val;
				omapMmcPrvCmdSend(mmc);
			}
			else
				val = mmc->cmd;
			break;
		
		case 0x04 / 4:	//MMC_ARGL
			if (write)
				mmc->arg = (mmc->arg & 0xffff0000ul) | val;
			else
				val = mmc->arg & 0xffff;
			break;
		
		case 0x08 / 4:	//MMC_ARGH
			if (write)
				mmc->arg = (mmc->arg & 0xffff) | (((uint32_t)val) << 16);
			else
				val = mmc->arg >> 16;
			break;
		
		case 0x0c / 4:	//MMC_CON
			if (write)
				mmc->con = val & 0xb8ff;
			else
				val = mmc->con;
			break;
		
		case 0x10 / 4:	//MMC_STAT
			if (write) {
				mmc->stat &=~ (val & 0x7dfd);
				omapMmcPrvRecalcIrqAndDma(mmc);
			}
			else
				val = mmc->stat;
			break;
		
		case 0x14 / 4:	//MMC_IE
			if (write) {
				mmc->ie = val & 0x7dfd;
				omapMmcPrvRecalcIrqAndDma(mmc);
			}
			else
				val = mmc->ie;
			break;
		
		case 0x18 / 4:	//MMC_CTO
			if (write)
				mmc->cto = val & 0xff;
			else
				val = mmc->cto;
			break;
		
		case 0x1c / 4:	//MMC_DTO
			if (write)
				mmc->dto = val;
			else
				val = mmc->dto;
			break;
		
		case 0x20 / 4:	//MMC_DATA
			if (write)
				ret = omapMmcPrvFifoW(mmc, val);
			else
				ret = omapMmcPrvFifoR(mmc, &val);
			break;
		
		case 0x24 / 4:	//MMC_BLEN
			if (write)
				mmc->blen = (val & 0x07ff) + 1;
			else
				val = mmc->blen ? mmc->blen - 1 : 0;
			break;
		
		case 0x28 / 4:	//MMC_NBLK
			if (write)
				mmc->nblk = (val & 0x07ff) + 1;
			else
				val = mmc->nblk ? mmc->nblk - 1 : 0;
			break;
		
		case 0x2c / 4:	//MMC_BUF
			if (write) {
				mmc->bufCfg = val & 0x9f9f;
				omapMmcPrvRecalcIrqAndDma(mmc);
			}
			else
				val = mmc->bufCfg;
			break;
		
		case 0x30 / 4:	//MMC_SPI
			if (write) {
				mmc->spi = val & 0x4f3f;
				if (val & 0x8000)
					ret = omapMmcPrvDoSpiXfer(mmc);
			}
			else
				val = mmc->spi;
			break;
		
		case 0x34 / 4:	//MMC_SDIO
			if (write) {
				mmc->sdio = val & 0x2020;
			}
			else
				val = mmc->sdio;
			break;
		
		case 0x38 / 4:	//MMC_SYST
			if (write)
				mmc->syst = val & 0x7ffe;
			else
				val = mmc->syst;
			break;
		
		case 0x3c / 4:	//MMC_REV
			if (write)
				ret = false;
			else
				val = 0x0028;
			break;
		
		case 0x40 / 4 ... 0x5c / 4:	//MMX_RSPx
			if (write)
				ret = false;
			else
				val = mmc->rsp[pa - 0x40 / 4];
			break;
			
		default:
			ret = false;
			break;
	}

	if (!write) {
		if (size == 2)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	//	fprintf(stderr, "MMC [0x%08x] -> 0x%08x (%s)\n", (unsigned)(OMAP_MMC_BASE + 4 * pa), (unsigned)val, ret ? "SUCCESS" : "FAIL");
	}
	
	return ret;
}

struct OmapMmc *omapMmcInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma* dma)
{
	struct OmapMmc *mmc = (struct OmapMmc*)malloc(sizeof(*mmc));
	
	if (!mmc)
		ERR("cannot alloc MMC");
	
	memset(mmc, 0, sizeof (*mmc));
	
	mmc->ic = ic;
	mmc->dma = dma;
	
	mmc->syst = 0x2000;
	
	if (!memRegionAdd(physMem, OMAP_MMC_BASE, OMAP_MMC_SIZE, omapMmcPrvMemAccessF, mmc))
		ERR("cannot add MMC to MEM\n");
	
	return mmc;
}

void omapMmcInsert(struct OmapMmc *mmc, struct VSD* vsd)
{
	mmc->vsd = vsd;
}

