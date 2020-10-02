//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_McBSP.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"



enum OmapMcBspPipeStateType {
	OmapMcBspPipeInReset,
	OmapMcBspPipeWaitForFrame,
	OmapMcBspPipePostFrameDelay,
	OmapMcBspPipePhase1,
	OmapMcBspPipePhase2,
};

struct OmapMcBspPipeState {
	enum OmapMcBspPipeStateType state;
	uint_fast8_t wordIdx;
	uint_fast8_t bitIdx;
};

struct OmapMcBsp {
	//emu info
	struct SocDma *dma;
	struct SocIc *ic;
	uint32_t base;
	uint8_t irqNoTx, irqNoRx, dmaNoTx, dmaNoRx;
	
	//data holding regs
	uint32_t drr, rbr, rsr, dxr, xsr;
	
	//configs
	uint16_t spcr1, spcr2, rcr1, rcr2, xcr1, xcr2, srgr1, srgr2, mcr1, mcr2, pcr;
	uint16_t rcer[8], xcer[8];
	
	//state
	uint32_t rbrFull			: 1;
	uint32_t drrFull			: 1;
	uint32_t rsrBits			: 5;		///xxx: set
	uint32_t xsrBits			: 5;		///xxx: set
	uint32_t curTxWordBitLen	: 6;		///xxx: set
	uint32_t curRxWordBitLen	: 6;		///xxx: set
	
	struct OmapMcBspPipeState rxState;
	struct OmapMcBspPipeState txState;
};


//XXX: XEMPTY Bit reset state shoudl be ???

#define OMAP_McBSP_SIZE		0x40

static uint_fast8_t omapMcBspPrvBitRev8(uint_fast8_t val)
{
	val = ((val & 0x55) << 1) | ((val >> 1) & 0x55);
	val = ((val & 0x33) << 2) | ((val >> 2) & 0x33);
	val = ((val & 0x0f) << 4) | ((val >> 4) & 0x0f);
	
	return val;
}

static uint32_t omapMcBspPrvCompandCompress(struct OmapMcBsp *sp, uint32_t val, uint_fast8_t txBitLen)
{
	uint32_t ret = 0, chord;
	int16_t cmp, val16 = val;
	
	if (txBitLen != 8)
		return val;
	
	switch ((sp->xcr2 >> 3) & 3) {		//XCOMPAND
		case 0:	//nothing
			ret = val;
			break;
		
		case 1:	//8bit, bitreversed
			ret = omapMcBspPrvBitRev8(val);
			break;
		
		case 2:	//uLaw
			if (val16 >= 0)
				ret |= 0x80;
			else
				val16 = -val16;
			
			val16 >>= 3;	//12 significant bits of now-unsigned data please
			
			for (chord = 0, cmp = 0x40; ; chord++, cmp <<= 1) {
				if (val16 >= cmp)
					continue;
				
				ret |= chord << 4;
				val16 >>= chord + 1;
				ret |= val16 & 0x0f;
				break;
			}
			break;
		
		case 3:	//aLaw

			if (val16 < 0) {
				ret |= 0x80;
				val16 = -val16;
			}
			
			val16 >>= 3;	//12 significant bits of now-unsigned data please
			
			for (chord = 0, cmp = 0x20; ; chord++, cmp <<= 1) {
				if (val16 >= cmp)
					continue;
				
				ret |= chord << 4;
				val16 >>= chord ? chord : 1;
				ret |= val16 & 0x0f;
				break;
			}
			break;
		default:
			__builtin_unreachable();
			ret = 0;
	}
	return ret;
}

static uint32_t omapMcBspPrvCompandExpand(struct OmapMcBsp *sp, uint32_t val, uint_fast8_t rxBitLen)
{
	uint32_t ret = 0;
	
	if (rxBitLen != 8)
		return val;
	
	switch ((sp->rcr2 >> 3) & 3) {		//RCOMPAND
		case 0:	//nothing
			ret = val;
			break;
		
		case 1:	//8bit, bitreversed
			ret = omapMcBspPrvBitRev8(val);
			break;
		
		case 2:	//uLaw
			ret = val & 0x0f;
			ret = ret * 2 + 0x21;
			ret <<= ((val >> 4) & 7);
			if (!(val & 0x80))
				ret = -ret;
			break;
		
		case 3:	//aLaw
			ret = val & 0x0f;
			if (val & 0x70) {
				ret = ret * 2 + 0x21;
				ret <<= ((val >> 4) & 7) - 1;
			}
			else
				ret = ret * 2 + 1;
			if (val & 0x80)
				ret = -ret;
			break;
		
		default:
			__builtin_unreachable();
			ret = 0;
	}
	return ret;
}

static void omapMcBspPrvUpdateIrqs(struct OmapMcBsp *sp)
{
	
}

static uint_fast8_t omapMcBspPrvBitLenUnpack(struct OmapMcBsp *sp, uint_fast8_t val)
{
	if (val < 5)
		return 4 * (val + 2);
	else if (val == 4)
		return 32;
	
	ERR("bit length %u not decodeable\n", val);
	return 0;
}

static uint_fast8_t omapMcBspPrvTxGoToDataPhase(struct OmapMcBsp *sp, struct OmapMcBspPipeState *ps, bool second)
{
	ps->state = second ? OmapMcBspPipePhase2 : OmapMcBspPipePhase1;
	ps->wordIdx = 0;
	return omapMcBspPrvBitLenUnpack(sp, ((second ? sp->xcr2 : sp->xcr1) >> 5) & 7);
}

//XXX: xempty needs to be adjusted when data written to buffer
static void omapMcBspPrvGetXsrIfNeeded(struct OmapMcBsp *sp)
{
	uint_fast8_t wordsThisPhase;
	
	if (sp->xsrBits)
		return;
	
	//sort out if we are about to go to a new phase
	switch (sp->txState.state) {
		case OmapMcBspPipePhase1:
			wordsThisPhase = ((sp->xcr1 >> 8) & 0x7f) + 1;
			break;
		
		case OmapMcBspPipePhase2:
			wordsThisPhase = ((sp->xcr2 >> 8) & 0x7f) + 1;
			break;
		
		default:
			ERR("phase %u does not transmit\n", sp->txState.state);
			break;
	}
	
	sp->txState.wordIdx++;
	
	if (sp->txState.wordIdx > wordsThisPhase)
		ERR("word index overflow\n");
	else if (sp->txState.wordIdx == wordsThisPhase) {
		
		if (sp->txState.state == OmapMcBspPipePhase2 || !(sp->xcr2 & 0x8000)) {
			
			sp->txState.state = OmapMcBspPipeWaitForFrame;
			return;
		}
		sp->curTxWordBitLen = omapMcBspPrvTxGoToDataPhase(sp, &sp->txState, true);
	}
	
	//we got here because we just shifted out the last bit in XSR
	if (sp->spcr2 & 2)	//transmitter was ready for data, but none arrived	
		sp->spcr2 &=~ 4;		//we have run empty
	else {
		sp->spcr2 &=~ 2;
		sp->xsr = omapMcBspPrvCompandCompress(sp, sp->dxr, sp->curTxWordBitLen);
	}
	
	if (!(sp->spcr2 & 4)) {		//we are empty
		
		sp->xsr = 0;
		fprintf(stderr, "McBsp@0x%08x: TX underrun\n", sp->base);
	}
	
	sp->xsr <<= 32 - sp->curTxWordBitLen;	//move data to top bits for transmission
	sp->xsrBits = sp->curTxWordBitLen;
	
	omapMcBspPrvUpdateIrqs(sp);
}

static bool omapMcBspPrvCalcTxBit(struct OmapMcBsp *sp)
{
	bool ret;
	
	//xxx check for transmitter reset
	
	omapMcBspPrvGetXsrIfNeeded(sp);
	
	ret = !!(sp->xsr >> 31);
	sp->xsr <<= 1;
	sp->xsrBits--;
	
	omapMcBspPrvGetXsrIfNeeded(sp);
	
	return ret;
}

static void omapMcBspPrvSetRbrIfNeeded(struct OmapMcBsp *sp)
{
	if (sp->rsrBits != sp->curRxWordBitLen)
		return;
	
	//TODO: copy to rbr
}

static void omapMcBspPrvAcceptRxBit(struct OmapMcBsp *sp, bool hi)
{
	//xxx check for receiver reset
	
	sp->rsr <<= 1;
	if (hi)
		sp->rsr++;
	sp->rsrBits++;
	
	omapMcBspPrvSetRbrIfNeeded(sp);
}

static void omapMcBspPrvDoBit(struct OmapMcBsp *sp)
{
	bool tx = omapMcBspPrvCalcTxBit(sp);
	bool rx = false;	//for now
	
	omapMcBspPrvAcceptRxBit(sp, rx);
	
	omapMcBspPrvUpdateIrqs(sp);
}

void omapMcBspPeriodic(struct OmapMcBsp *sp)
{
	uint_fast8_t i;
	
	//XXX: consider phases
	
//	for (i = 0; i < 4; i++)	//all word sizes are multiples of 4
//		omapMcBspPrvDoBit(sp);
	
	//for now, request DMA forever
	if (!(sp->spcr1 & 0x0001))
		socDmaExternalReq(sp->dma, sp->dmaNoRx, true);
	
	if (!(sp->spcr2 & 0x0001))
		socDmaExternalReq(sp->dma, sp->dmaNoTx, true);
}

static bool omapMcBspPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMcBsp *sp = (struct OmapMcBsp*)userData;
	uint_fast16_t val = 0;
	
	//XXX: scary hack
	if (size == 4 && write)
		size = 2;
	
	if (size != 2) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - sp->base) >> 1;
	
	if (write)
		val = *(uint16_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 2:	//DRR2
			if (write)
				return false;
			else
				val = sp->drr >> 16;
			break;
		
		case 0x02 / 2:	//DRR1
			if (write)
				return false;
			else {
				val = sp->drr & 0xffff;
				if (sp->rbrFull) {
					sp->drr = omapMcBspPrvCompandExpand(sp, sp->rbr, sp->curRxWordBitLen);
					sp->rbrFull = false;
				}
				else
					sp->drrFull = false;

				omapMcBspPrvUpdateIrqs(sp);
			}
			break;
		
		case 0x04 / 2:	//DXR2
			if (write)
				sp->dxr = (sp->dxr & 0xffff) + (((uint32_t)val) << 16);
			else
				return false;
			break;
		
		case 0x06 / 2:	//DXR1
			if (write) {
				sp->dxr = (sp->dxr & 0xffff0000ul) + val;
				sp->spcr2 |= 4;		//no longer empty
				sp->spcr2 &=~ 2;	//no longer ready for more data
				omapMcBspPrvUpdateIrqs(sp);
			}
			else
				return false;
			break;
		
		case 0x08 / 2:	//SPCR2
		
			if (write) {
				
				if ((sp->spcr2 & 0x80) && !(val & 0x80)) {	//frame sync logic reset
					
					//todo
				}
				else if (!(sp->spcr2 & 0x80) && (val & 0x80)) {	//frame sync logic remove from reset
					sp->rxState.state = OmapMcBspPipeWaitForFrame;
					
					//todo
				}
				
				if ((sp->spcr2 & 0x40) && !(val & 0x40)) {	//sample rate generator reset
					
					//todo
				}
				else if (!(sp->spcr2 & 0x40) && (val & 0x40)) {	//sample rate generator remove from reset
					sp->rxState.state = OmapMcBspPipeWaitForFrame;
					
					//todo
				}
				
				sp->spcr2 &= 0x0006;
				sp->spcr2 = val & 0x03f9;
				
				omapMcBspPrvUpdateIrqs(sp);
			}
			else
				val = sp->spcr2;
			break;
		
		case 0x0a / 2:	//SPCR1
		
			if (write) {
				
				if ((sp->spcr1 & 1) && !(val & 1))	//receiver reset
					sp->rxState.state = OmapMcBspPipeInReset;
				else if (!(sp->spcr1 & 1) && (val & 1)) {	//receiver remove from reset
					sp->rxState.state = OmapMcBspPipeWaitForFrame;
				
					sp->spcr1 &=~ 0x000e;
					sp->rbrFull = 0;
					sp->drrFull = 0;
					sp->rsrBits = 0;
				}
				sp->spcr1 &= 0x0006;
				sp->spcr1 |= val & 0xf8b9;
				
				if (val & 0x8000)
					ERR("loopback not supported\n");
				
				omapMcBspPrvUpdateIrqs(sp);
			}
			else
				val = sp->spcr1;
			break;
		
		case 0x0c / 2:	//RCR2
		
			if (write)
				sp->rcr2 = val;
			else
				val = sp->rcr2;
			break;
		
		case 0x0e / 2:	//RCR1
		
			if (write)
				sp->rcr1 = val & 0x7fe0;
			else
				val = sp->rcr1;
			break;
		
		case 0x10 / 2:	//XCR2
		
			if (write)
				sp->xcr2 = val;
			else
				val = sp->xcr2;
			break;
		
		case 0x12 / 2:	//XCR1
		
			if (write)
				sp->xcr1 = val & 0x7fe0;
			else
				val = sp->xcr1;
			break;
		
		case 0x14 / 2:	//SRGR2
		
			if (write)
				sp->srgr2 = val;
			else
				val = sp->srgr2;
			break;
		
		case 0x16 / 2:	//SRGR1
		
			if (write)
				sp->srgr1 = val;
			else
				val = sp->srgr1;
			break;
			
		case 0x18 / 2:	//MCR2
		
			if (write) {
				sp->mcr2 = val & 0x03fd;
				if (val & 3)
					ERR("multichannel TX mode not supported\n");
			}
			else
				val = sp->mcr2;
			break;
		
		case 0x1a / 2:	//MCR1
		
			if (write) {
				sp->mcr1 = val & 0x03fd;
				if (val & 1)
					ERR("multichannel RX mode not supported\n");
			}
			else
				val = sp->mcr1;
			break;
			
		case 0x1c / 2:	//RCERA
		case 0x1e / 2:	//RCERB
		
			if (write)
				sp->rcer[pa - 0x1c / 2 + 0] = val;
			else
				val = sp->rcer[pa - 0x1c / 2 + 0];
			break;
		
		case 0x20 / 2:	//XCERA
		case 0x22 / 2:	//XCERB
			if (write)
				sp->xcer[pa - 0x20 / 2 + 0] = val;
			else
				val = sp->xcer[pa - 0x20 / 2 + 0];
			break;
		
		case 0x24 / 2:	//PCR0
		
			if (write)
				sp->pcr = val & 0x7fff;
			else
				val = sp->pcr;
			break;
		
		case 0x26 / 2:	//RCERC
		case 0x28 / 2:	//RCERD
		
			if (write)
				sp->rcer[pa - 0x26 / 2 + 2] = val;
			else
				val = sp->rcer[pa - 0x26 / 2 + 2];
			break;
		
		case 0x2a / 2:	//XCERC
		case 0x2c / 2:	//XCERD
			if (write)
				sp->xcer[pa - 0x2a / 2 + 2] = val;
			else
				val = sp->xcer[pa - 0x2a / 2 + 2];
			break;
		
		case 0x2e / 2:	//RCERE
		case 0x30 / 2:	//RCERF
		
			if (write)
				sp->rcer[pa - 0x2e / 2 + 4] = val;
			else
				val = sp->rcer[pa - 0x2e / 2 + 4];
			break;
		
		case 0x32 / 2:	//XCERE
		case 0x34 / 2:	//XCERF
			if (write)
				sp->xcer[pa - 0x32 / 2 + 4] = val;
			else
				val = sp->xcer[pa - 0x32 / 2 + 4];
			break;
		
		case 0x36 / 2:	//RCERG
		case 0x38 / 2:	//RCERH
		
			if (write)
				sp->rcer[pa - 0x36 / 2 + 6] = val;
			else
				val = sp->rcer[pa - 0x36 / 2 + 6];
			break;
		
		case 0x3a / 2:	//XCERG
		case 0x3c / 2:	//XCERH
			if (write)
				sp->xcer[pa - 0x3a / 2 + 6] = val;
			else
				val = sp->xcer[pa - 0x3a / 2 + 6];
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return true;
}

struct OmapMcBsp* omapMcBspInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma, uint32_t base, uint8_t irqNoTx, uint8_t irqNoRx, uint8_t dmaNoTx, uint8_t dmaNoRx)
{
	struct OmapMcBsp *sp = (struct OmapMcBsp*)malloc(sizeof(*sp));
	
	if (!sp)
		ERR("cannot alloc McBSP @ 0x%08x", base);
	
	memset(sp, 0, sizeof (*sp));
	sp->dma = dma;
	sp->ic = ic;
	sp->irqNoTx = irqNoTx;
	sp->irqNoRx = irqNoRx;
	sp->dmaNoTx = dmaNoTx;
	sp->dmaNoRx = dmaNoRx;
	sp->base = base;
	
	if (!memRegionAdd(physMem, base, OMAP_McBSP_SIZE, omapMcBspPrvMemAccessF, sp))
		ERR("cannot add McBSP @ 0x%08x to MEM\n", base);
	
	return sp;
}
