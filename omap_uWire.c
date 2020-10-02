//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "soc_uWire.h"
#include "omap_DMA.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define OMAP_UWIRE_BASE		0xFFFB3000
#define OMAP_UWIRE_SIZE		0x1c


struct SocUwire {
	struct SocDma *dma;
	struct SocIc *ic;
	
	struct {
		UWireClientProcF cbk;
		void* data;
	} clients[4];
	
	uint16_t data, csr, sr2;
	uint8_t sr1, sr3, sr4, sr5;
	
	//state
	uint8_t curActiveCSs	: 4;	//bitfield
	uint8_t gotNewData		: 1;
};



static void socUwirePrvTransact(struct SocUwire *uw)
{
	uint_fast8_t i, bitsTx = (uw->csr >> 5) & 0x1f, bitsRx = uw->csr & 0x1f;
	uint_fast16_t ret = 0;
	
	//fprintf(stderr, "uW: 0x%04x (%u bits) => %u bits\n", uw->data >> (16 - bitsTx), bitsTx, bitsRx);

	for (i = 0; i < 4; i++) {
		if ((uw->curActiveCSs & (1 << i)) && uw->clients[i].cbk)
			ret |= (uw->clients[i].cbk(uw->clients[i].data, bitsTx, bitsRx, uw->data >> (16 - bitsTx))) & ((1 << bitsRx) - 1);
	}
	
	//fprintf(stderr, "uW: 0x%04x (%u bits) => 0x%04x (%u bits)\n", uw->data >> (16 - bitsTx), bitsTx, (unsigned)ret, bitsRx);
	
	uw->data = ret;
	uw->csr |= 0x8000;
	uw->csr &=~ 0x2000;
	
	if (uw->sr5 & 0x02) {
		if (bitsTx) {
			//edge
			socIcInt(uw->ic, OMAP_I_uWIRE_TX, true);
			socIcInt(uw->ic, OMAP_I_uWIRE_TX, false);
		}
		if (bitsRx) {
			//edge
			socIcInt(uw->ic, OMAP_I_uWIRE_RX, true);
			socIcInt(uw->ic, OMAP_I_uWIRE_RX, false);
		}
	}
}

static bool socUwirePrvMoveCsPins(struct SocUwire *uw, uint_fast8_t newCsPinsBitfield)	//return if any CS pins changes
{
	uint_fast8_t diffs = uw->curActiveCSs ^ newCsPinsBitfield;
	uint_fast8_t wentActive = diffs & newCsPinsBitfield;
	uint_fast8_t wentInactive = diffs & uw->curActiveCSs;
	uint_fast8_t i;
	
	for (i = 0; i < 4; i++) {
		if (wentActive & (1 << i)) {
		//	fprintf(stderr, "uW: client %u active\n", i);
			if (uw->clients[i].cbk)
				uw->clients[i].cbk(uw->clients[i].data, -1, -1, true);
		}
		if (wentInactive & (1 << i)) {
		//	fprintf(stderr, "uW: client %u inactive\n", i);
			if (uw->clients[i].cbk)
				uw->clients[i].cbk(uw->clients[i].data, -1, -1, false);
		}
	}
	
	uw->curActiveCSs = newCsPinsBitfield;
	
	return !!diffs;
}


static void socUwirePrvCsAct(struct SocUwire *uw)
{
	uint_fast8_t diffCsIdx = 1 << (uw->csr >> 10) & 3;
	uint_fast8_t newCSs = uw->curActiveCSs;
	uint_fast8_t diffCs = 1 << diffCsIdx;
	
	if (uw->csr & 0x1000)		//CS_CMD
		newCSs |= diffCs;
	else
		newCSs &=~ diffCs;
	
	//if chip select changed, that is all we'll do for now
	if (socUwirePrvMoveCsPins(uw, newCSs)) {
	
		//nothing
	}
	//did we get a start? if so, do a transaction
	else if (uw->csr & 0x2000) {
		
		socUwirePrvTransact(uw);
	}
	//done?
	else {
		uw->csr &=~ 0x4000;
	}
}

static bool socUwirePrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocUwire *uw = (struct SocUwire*)userData;
	uint_fast16_t val = 0;
	
	if (size != 2 || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_UWIRE_BASE) >> 2;
	
	if (write)
		val = *(uint16_t*)buf;
	
	switch (pa) {
		case 0x00 / 4:
			if (write) {
				uw->data = val;
				uw->gotNewData = 1;
			}
			else {
				val = uw->data;
				uw->csr &=~ 0x8000;
			}
			break;
			
		case 0x04 / 4:
			if (write) {
				uw->csr = (uw->csr & 0x8000) | (val & 0x3fff) | 0x4000;
				socUwirePrvCsAct(uw);		//CS moves regardless of command
			}
			else
				val = uw->csr;
			break;
		
		case 0x08 / 4:
			if (write)
				uw->sr1 = val & 0x003f;
			else
				val = uw->sr1;
			break;
		
		case 0x0c / 4:
			if (write)
				uw->sr2 = val & 0x0fc0;
			else
				val = uw->sr2;
			break;
		
		case 0x10 / 4:
			if (write)
				uw->sr3 = val & 0x0007;
			else
				val = uw->sr3;
			break;
		
		case 0x14 / 4:
			if (write)
				uw->sr4 = val & 0x0001;
			else
				val = uw->sr4;
			break;
		
		case 0x18 / 4:
			if (write)
				uw->sr5 = val & 0x000f;
			else
				val = uw->sr5;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return true;
}


bool socUwireAddClient(struct SocUwire *uw, uint_fast8_t cs, UWireClientProcF procF, void* userData)
{
	if (cs > sizeof(uw->clients) / sizeof(*uw->clients))
		return false;
	
	if (uw->clients[cs].cbk)
		return false;
	
	uw->clients[cs].data = userData;
	uw->clients[cs].cbk = procF;
	
	return true;
}

struct SocUwire* socUwireInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma)
{
	struct SocUwire *uw = (struct SocUwire*)malloc(sizeof(*uw));
	
	if (!uw)
		ERR("cannot alloc uWire");
	
	memset(uw, 0, sizeof (*uw));
	uw->dma = dma;
	uw->ic = ic;
	
	if (!memRegionAdd(physMem, OMAP_UWIRE_BASE, OMAP_UWIRE_SIZE, socUwirePrvMemAccessF, uw))
		ERR("cannot add uWire to MEM\n");
	
	return uw;
}

void socUwirePeriodic(struct SocUwire *uw)
{
	if (uw->csr & 0x4000) {	//csr was written
		
		socUwirePrvCsAct(uw);
	}
	else if ((uw->sr5 & 0x4) && uw->gotNewData) {	//auto-tx ?
		
		ERR("auto-tx TBD\n");
	}
}
