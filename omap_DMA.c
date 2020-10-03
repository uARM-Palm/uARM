//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_DMA.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "util.h"
#include "mem.h"



#define OMAP_DMA_BASE			0xFFFEDC00UL
#define OMAP_DMA_SIZE			4

#define OMAP_DMA_CHS_BASE		0xFFFED800UL
#define OMAP_DMA_CHS_SIZE		0x300

#define NUM_CHANNELS			9

struct DmaChannelCfg {
	
	uint32_t src, dst;
	uint16_t cen, cfn, cfi, cei;
};

struct DmaChannel {
	uint16_t csdp, ccr, csr, cpc;
	uint8_t cicr;
	
	//regs config iface accesses
	struct DmaChannelCfg programming;
	
	//shadow regs DMA uses
	struct DmaChannelCfg active;
	
	//dma state we have
	uint16_t curElemIdx;	//next elem to xfer in cur frame
	uint16_t curFrameIdx;	//next frame to xfer in cur block
	
	uint32_t curSrcAddr;
	uint32_t curDstAddr;	//we cannot modify active.{src,dst} since in case of repetitive xfer we need them as is
	bool waitingForEndProg;
	
	//to help with efficiency
	uint8_t irqNo;
	struct DmaChannel *friendChannel;
};

struct SocDma {

	struct SocIc *ic;
	struct ArmMem *mem;
	
	uint32_t pendingReqs;
	struct DmaChannel ch[NUM_CHANNELS];
	uint8_t gcr;
};






static bool socDmaPrvCtrlMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocDma *dma = (struct SocDma*)userData;
	uint32_t val = 0;
	
	if ((size != 4 && size != 2) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - OMAP_DMA_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				dma->gcr = val & 0x0c;
			else
				val = dma->gcr;
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

static void socDmaPrvChannelInitialize(struct SocDma *dma, struct DmaChannel *ch, bool copyCfg)	//also handles re-initializing
{
	if (copyCfg)
		ch->active = ch->programming;
	ch->curElemIdx = 0;
	ch->curFrameIdx = 0;
	ch->cpc = 0;
	ch->waitingForEndProg = false;
	
	ch->curSrcAddr = ch->active.src;
	ch->curDstAddr = ch->active.dst;
	
	if (0)
	if (ch - dma->ch != 7) {
		static const char *modes[] = {"const", "linear", "single index", "double index"};
		fprintf(stderr, "DMA start ch %u %u byte x %u x %u from 0x%08lx to 0x%08lx\n",
			(unsigned)(ch - dma->ch), 1 << (ch->csdp & 3), ch->active.cen, ch->active.cfn, (unsigned long)ch->active.src, (unsigned long)ch->active.dst);
		fprintf(stderr, " src mode is %s, dst mode is %s\n", modes[(ch->ccr >> 12) & 3], modes[(ch->ccr >> 14) & 3]);
		fprintf(stderr, " cei = %d, cfi = %d\n", ch->active.cei, ch->active.cfi);
	}
}

static bool socDmaPrvChsMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocDma *dma = (struct SocDma*)userData;
	uint_fast16_t val = 0;
	struct DmaChannel *ch;
	uint_fast8_t chNo;
	
	if (size != 2) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa -= OMAP_DMA_CHS_BASE;
	
	if (write)
		val = *(uint16_t*)buf;
	
	chNo = pa / 0x40;
	pa %= 0x40;
	pa /= 2;
	
	if (chNo >= NUM_CHANNELS)
		return false;
	ch = &dma->ch[chNo];
	
	switch (pa) {
		
		case 0x00 / 2:
			if (write)
				ch->csdp = val & 0xffff;
			else
				val = ch->csdp;
			break;
		
		case 0x02 / 2:
			if (write) {
				
				ch->ccr &= 0x80;		//keep old enabled bits
				ch->ccr = val & 0xfb7f;	//do not set new enabled bit yet
				
				//sort out enablement
				if (!(ch->ccr & 0x0080) && (val & 0x0080)) {
					ch->ccr |= 0x0080;
					socDmaPrvChannelInitialize(dma, ch, true);
				}
				
				//sort out disablement
				if ((ch->ccr & 0x0080) && !(val & 0x0080)) {
					ch->ccr &=~ 0x0080;
					ch->waitingForEndProg = false;
				}
				
				//or maybe we're enabled and END_PROG we've been waiting for arrived
				if ((val & 0x0800) && ch->waitingForEndProg) {
					ch->waitingForEndProg = false;
					ch->ccr &=~ 0x0800;
					socDmaPrvChannelInitialize(dma, ch, true);
				}
			}
			else
				val = ch->ccr;
			break;
		
		case 0x04 / 2:
			if (write) {
				ch->cicr = val & 0x3f;
				//xxx: is this accurate? how else would ints be blocked if they get disabled concurrently with happening?
				ch->csr &= ch->cicr;
			}
			else
				val = ch->cicr;
			break;
		
		case 0x06 / 2:
			if (write)
				;	//write ignored
			else {
				
				val = ch->csr & 0x7f;
				ch->csr &= 0x40;	//other bits clear when read
				
				if (ch->friendChannel) {
					val |= ((uint32_t)ch->friendChannel->csr) << 7;
					ch->friendChannel->csr &= 0x40;	//other bits clear when read
				}
			}
			break;
		
		case 0x08 / 2:			//src.lo
			if (write)
				ch->programming.src = (ch->programming.src & 0xffff0000ul) | val;
			else
				val = (uint16_t)ch->programming.src;
			break;
		
		case 0x0a / 2:			//src.hi
			if (write)
				ch->programming.src = (ch->programming.src & 0xffff) | (((uint32_t)val) << 16);
			else
				val = ch->programming.src >> 16;
			break;
		
		case 0x0c / 2:			//dst.lo
			if (write)
				ch->programming.dst = (ch->programming.dst & 0xffff0000ul) | val;
			else
				val = (uint16_t)ch->programming.dst;
			break;
		
		case 0x0e / 2:			//dst.hi
			if (write)
				ch->programming.dst = (ch->programming.dst & 0xffff) | (((uint32_t)val) << 16);
			else
				val = ch->programming.dst >> 16;
			break;
		
		case 0x10 / 2:
			if (write)
				ch->programming.cen = val;
			else
				val = ch->programming.cen;
			break;
		
		case 0x12 / 2:
			if (write)
				ch->programming.cfn = val;
			else
				val = ch->programming.cfn;
			break;
		
		case 0x14 / 2:
			if (write)
				ch->programming.cfi = val;
			else
				val = ch->programming.cfi;
			break;
		
		case 0x16 / 2:
			if (write)
				ch->programming.cei = val;
			else
				val = ch->programming.cei;
			break;
		
		case 0x18 / 2:
			if (write)
				;	//fobidden and ignored
			else
				val = ch->cpc;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return true;
}

static uint32_t socDmaPrvAdjustAddress(struct SocDma *dma, struct DmaChannel *ch, uint32_t addr, uint_fast8_t accessSz, uint_fast8_t mode)
{
	switch (mode) {
		 case 0:	//constant addr
		 	break;
		 
		 case 1:	//post-increment
		 	addr += accessSz;
		 	break;
		 
		 case 2:	//single-indexed
		 	addr += accessSz - 1 + (int32_t)(int16_t)ch->active.cei - 1;
		 	break;
		 
		 case 3:	//double-indexed
		 	if (ch->curElemIdx)
		 		addr += accessSz - 1 + (int32_t)(int16_t)ch->active.cei;
		 	else
		 		addr += accessSz - 1 + (int32_t)(int16_t)ch->active.cfi;
		 	break;
		 
		 default:
		 	__builtin_unreachable();
	}
	
	return addr;
}

static void socDmaPrvChannelActIfNeeded(struct SocDma *dma, struct DmaChannel *ch)
{
	uint_fast8_t reqNum = ch->ccr & 0x1f, accessSz = ch->csdp & 3 /* lg2 */;
	uint_fast16_t i, nElems;
	bool entire = true;		//make memory-to-memory xfers instant
	char xferBuf[4];
	
	//bursting appears not mandatory so we do not do it
	//packing appears not mandatory so we do not do it
	//NOTE: maskable CSR bits are not set if respecitive CICR bits are clear
	
	//off? then we do nothing
	if (!(ch->ccr & 0x80))
		goto update_irq;
	
	//synchronized with a periph which is not requesting a transfer? do noting
	if (reqNum) {
		if (dma->pendingReqs & (1UL << reqNum))
			ch->csr |= 0x40;
		else {
			ch->csr &=~ 0x40;
			//no need for irq updates since this bit never generates irqs
			goto update_irq;
		}
		entire = false;
	}
	
	//some sanity checking
	if (accessSz == 3)
		ERR("DMA.ch[%u].CDSP.DATA_TYPE = 3\n", (unsigned)(ch - dma->ch));
	accessSz = 1 << accessSz;	//convert to actual size
	
	//sort out how much to xfer
	if (entire)	//mem to mem xfers are instant
		nElems = ch->active.cen * ch->active.cfn;
	else if (ch->ccr & 0x20)//transfer a frame
		nElems = ch->active.cen;
	else					//transfer an element
		nElems = 1;
	
	//fprintf(stderr, "DMA ch %lu: xferring %u %u-byte elems from 0x%08x to 0x%08x\n", ch - dma->ch, (unsigned)nElems, accessSz, ch->curSrcAddr, ch->curDstAddr);
	
	for (i = 0; i < nElems; i++) {
		
		if (!memAccess(dma->mem, ch->curSrcAddr, accessSz, false, xferBuf)) 
			ERR("DMA ch %u bus error on read at 0x%08lx\n", (unsigned)(ch - dma->ch), (unsigned long)ch->curSrcAddr);
		else if (!memAccess(dma->mem, ch->curDstAddr, accessSz, true, xferBuf))
			ERR("DMA ch %u bus error on write at 0x%08lx\n", (unsigned)(ch - dma->ch), (unsigned long)ch->curDstAddr);
		
		if (++ch->curElemIdx == ch->active.cen) {
			ch->curElemIdx = 0;
			ch->csr |= 0x08 & ch->cicr;
			if (++ch->curFrameIdx == ch->active.cfn)
				ch->curFrameIdx = 0;
		}
	
		ch->curSrcAddr = socDmaPrvAdjustAddress(dma, ch, ch->curSrcAddr, accessSz, (ch->ccr >> 12) & 3);
		ch->curDstAddr = socDmaPrvAdjustAddress(dma, ch, ch->curDstAddr, accessSz, (ch->ccr >> 14) & 3);
		
		if (ch->curFrameIdx == ch->active.cfn - 1)
			ch->csr |= 0x10 & ch->cicr;
		
		if (ch->curElemIdx >= ch->active.cen / 2)
			ch->csr |= 0x04 & ch->cicr;
	}
	
	if (!ch->curElemIdx && !ch->curFrameIdx) {
		
		if(0)
		if (ch - dma->ch != 7)
			fprintf(stderr, "DMA done\n");
		
		ch->csr |= 0x20 & ch->cicr;
		
		if (!(ch->ccr & 0x0100)) {				//.AUTOINIT
			ch->ccr &=~ 0x0080;					//channel stops
		}
		else if (ch->ccr & 0x0200) {			//.REPEAT
			socDmaPrvChannelInitialize(dma, ch, false);
		}
		else if (ch->ccr & 0x0800) {			//.END_PROG
			ch->ccr &=~ 0x0800;					//clear it
			socDmaPrvChannelInitialize(dma, ch, true);
		}
		else {
			ch->waitingForEndProg = true;
		}
	}
	
update_irq:
	socIcInt(dma->ic, ch->irqNo, (ch->csr & 0x3f) || (ch->friendChannel && (ch->friendChannel->csr & 0x3f)));
}

struct SocDma* socDmaInit(struct ArmMem *physMem, struct SocIc *ic)
{
	static const uint8_t irqNos[] = {OMAP_I_DMA_CH0_CH6, OMAP_I_DMA_CH1_CH7, OMAP_I_DMA_CH2_CH8, OMAP_I_DMA_CH3, OMAP_I_DMA_CH4, OMAP_I_DMA_CH5, OMAP_I_DMA_CH0_CH6, OMAP_I_DMA_CH1_CH7, OMAP_I_DMA_CH2_CH8};
	struct SocDma *dma = (struct SocDma*)malloc(sizeof(*dma));
	uint_fast8_t i;
	
	if (!dma)
		ERR("cannot alloc DMA");
	
	memset(dma, 0, sizeof (*dma));
	dma->ic = ic;
	dma->mem = physMem;
	dma->gcr = 8;
	
	for (i = 0; i < NUM_CHANNELS; i++) {
		dma->ch[i].cicr = 3;	//stopped or uninitialized
		dma->ch[i].irqNo = irqNos[i];
	}
	
	//cross-link friend channels
	for (i = 0; i < 3; i++) {
		dma->ch[i].friendChannel = &dma->ch[i + 6];
		dma->ch[i + 6].friendChannel = &dma->ch[i];
	}
	
	if (!memRegionAdd(physMem, OMAP_DMA_BASE, OMAP_DMA_SIZE, socDmaPrvCtrlMemAccessF, dma))
		ERR("cannot add DMA control to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_DMA_CHS_BASE, OMAP_DMA_CHS_SIZE, socDmaPrvChsMemAccessF, dma))
		ERR("cannot add DMA channels to MEM\n");
	
	return dma;
}

void socDmaExternalReq(struct SocDma* dma, uint_fast8_t chNum, bool requested)
{
	if (chNum < sizeof(dma->pendingReqs) * CHAR_BIT) {
		if (requested)
			dma->pendingReqs |= 1UL << chNum;
		else
			dma->pendingReqs &=~ 1UL << chNum;
	}
}

void socDmaPeriodic(struct SocDma* dma)
{
	uint_fast8_t i;
	
	for (i = 0; i < NUM_CHANNELS; i++)
		socDmaPrvChannelActIfNeeded(dma, &dma->ch[i]);
}
