//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_Camera.h"
#include "omap_DMA.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_CAMIF_BASE	0xFFFB6800UL
#define OMAP_CAMIF_SIZE	0x1C

struct OmapCamera {
	
	struct SocDma *dma;
	struct SocIc *ic;
	
	uint32_t mode;
	uint8_t ctrlClock, itStatus, status, gpio, peakCounter;
};

static void omapCameraPrvRecalcIrqAndDma(struct OmapCamera *cam)
{
	socIcInt(cam->ic, OMAP_I_CAMERA, false);
	socDmaExternalReq(cam->dma, DMA_REQ_CAMERA_RX, false);
}


static bool omapCameraPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapCamera *cam = (struct OmapCamera*)userData;
	uint32_t val = 0;
	bool ret = true;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_CAMIF_BASE) >> 2;
	
	if (write) {
		val = *(uint32_t*)buf;
		fprintf(stderr, "CAM 0x%08x->[0x%08x]\n", (unsigned)val, (unsigned)(OMAP_CAMIF_BASE + 4 * pa));
	}

	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				cam->ctrlClock = val & 0xff;
			else
				val = cam->ctrlClock;
			break;
		
		case 0x04 / 4:
			if (write)
				;	//ignored
			else
				val = cam->itStatus;
			break;
		
		case 0x08 / 4:
			if (write)
				cam->mode = val & 0x0007ffff;
			else
				val = cam->mode;
			break;
		
		case 0x0c / 4:
			if (write)
				;	//ignored
			else
				val = cam->status;
			break;
		
		case 0x10 / 4:
			if (write)
				return false;
			else
				ERR("camera fifo not impl\n");
			break;
		
		case 0x14 / 4:
			if (write)
				cam->gpio = val & 1;
			else
				val = cam->gpio;
			break;
		
		case 0x18 / 4:
			if (write)
				cam->peakCounter = val & 0x7f;
			else
				val = cam->peakCounter;
			break;
		
		default:
			ret = false;
			break;
	}

	if (!write) {
		*(uint32_t*)buf = val;
		fprintf(stderr, "CAM [0x%08x] -> 0x%08x (%s)\n", (unsigned)(OMAP_CAMIF_BASE + 4 * pa), (unsigned)val, ret ? "SUCCESS" : "FAIL");
	}
	
	return ret;
}

struct OmapCamera *omapCameraInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma* dma)
{
	struct OmapCamera *cam = (struct OmapCamera*)malloc(sizeof(*cam));
	
	if (!cam)
		ERR("cannot alloc CameraIF");
	
	memset(cam, 0, sizeof (*cam));
	cam->ic = ic;
	cam->dma = dma;
	
	if (!memRegionAdd(physMem, OMAP_CAMIF_BASE, OMAP_CAMIF_SIZE, omapCameraPrvMemAccessF, cam))
		ERR("cannot add CameraIF to MEM\n");
	
	return cam;
}
