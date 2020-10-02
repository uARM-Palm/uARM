//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_USB.h"
#include "omap_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_USB_BASE			0xFFFB4000UL
#define OMAP_USB_SIZE			256


struct OmapUsb {
	struct ArmMem *mem;
	struct SocDma *dma;
	struct SocIc *ic;
	
	
	uint16_t syscon1, irqSrc, ep0Cfg;
	uint16_t rxEpCfg[15];
	uint16_t txEpCfg[15];
	
	uint8_t epNum, irqEn, devStat;
};

static bool omapUsbPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapUsb *usb = (struct OmapUsb*)userData;
	uint32_t val = 0;
	
	if ((size != 4 && size != 2) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_USB_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x04 / 4:
			if (write)
				usb->epNum = val & 0x7f; //i am approximately 100% sure that this is not how this is uspposed to work.
			else
				val = usb->epNum;
			break;
		
		case 0x0c / 4:
			if (write) {
				if (val & 0x01)
					fprintf(stderr, "USB: %s ep %u\n", "reset", usb->epNum & 15);
				if (val & 0x02)
					fprintf(stderr, "USB: %s ep %u\n", "clear", usb->epNum & 15);
				if (val & 0x04)
					fprintf(stderr, "USB: %s ep %u\n", "FIFO ON", usb->epNum & 15);
				if (val & 0x40)
					fprintf(stderr, "USB: %s ep %u\n", "HALT", usb->epNum & 15);
				if (val & 0x80)
					fprintf(stderr, "USB: %s ep %u\n", "UNHALT", usb->epNum & 15);
			}
			else
				val = 0;	//read returns zero
			break;
		
		case 0x18 / 4:
			if (write)
				usb->syscon1 = val & 0x0117;
			else
				val = usb->syscon1;
			break;
		
		case 0x20 / 4:
			if (write)
				return false;
			else
				val = usb->devStat;
			break;
		
		case 0x28 / 4:
			if (write)
				usb->irqEn = val & 0x00b9;
			else
				val = usb->irqEn;
			break;
		
		case 0x30 / 4:
			if (write)
				;					//write ignored	//might be wrong
			else {
				val = usb->irqSrc;
				usb->irqSrc = 0;	//might be wrong
			}
			break;
		
		case 0x80 / 4:
			if (write)
				usb->ep0Cfg = val & 0x37ff;
			else
				val = usb->ep0Cfg;
			break;
		
		case 0x84 / 4 ... 0xbc / 4:
			if (write)
				usb->rxEpCfg[pa - 0x84 / 4] = val;
			else
				val = usb->rxEpCfg[pa - 0x84 / 4];
			break;
		
		case 0xc4 / 4 ... 0xfc / 4:
			if (write)
				usb->txEpCfg[pa - 0xc4 / 4] = val;
			else
				val = usb->txEpCfg[pa - 0xc4 / 4];
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

struct OmapUsb* omapUsbInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma)
{
	struct OmapUsb *usb = (struct OmapUsb*)malloc(sizeof(*usb));
	
	if (!usb)
		ERR("cannot alloc USB");
	
	memset(usb, 0, sizeof (*usb));
	usb->mem = physMem;
	usb->dma = dma;
	usb->ic = ic;
	
	if (!memRegionAdd(physMem, OMAP_USB_BASE, OMAP_USB_SIZE, omapUsbPrvMemAccessF, usb))
		ERR("cannot add USB to MEM\n");
	
	return usb;
}
