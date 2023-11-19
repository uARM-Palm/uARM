//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_TxNoRamMarker.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define TX_NO_RAM_MARKER_BASE	0xA5FFFFFCul
#define TX_NO_RAM_MARKER_SIZE 	0x04


struct TxNoRamMarker {
	
	uint32_t written;
};



static bool txNoRamMarkerPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct TxNoRamMarker *mrkr = (struct TxNoRamMarker*)userData;
	uint32_t val;
	
	pa -= TX_NO_RAM_MARKER_BASE;
	
	if (size != 4 || pa) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	if (write)
		mrkr->written = *(uint32_t*)buf;
	else
		*(uint32_t*)buf = ~mrkr->written;	//we need to be sure written value mismatches read
	
	return true;
}
	

struct TxNoRamMarker* txNoRamMarkerInit(struct ArmMem *physMem)
{
	struct TxNoRamMarker* mrkr = (struct TxNoRamMarker*)malloc(sizeof(*mrkr));
	
	if (!mrkr)
		ERR("cannot alloc TX's 'NO RAM HERE' MARKER\n");
	
	memset(mrkr, 0, sizeof (*mrkr));
	
	if (!memRegionAdd(physMem, TX_NO_RAM_MARKER_BASE, TX_NO_RAM_MARKER_SIZE, txNoRamMarkerPrvMemAccessF, mrkr))
		ERR("cannot add TX's 'NO RAM HERE' MARKER to MEM\n");
	
	return mrkr;
}
