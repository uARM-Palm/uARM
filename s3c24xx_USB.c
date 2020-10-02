//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_USB.h"
#include "s3c24xx_DMA.h"
#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define S3C24XX_USB_BASE			0x52000000UL
#define S3C24XX_USB_SIZE			0x270

#define NUM_EPS						5

struct UsbEp {
	uint8_t IN_CSR1, IN_CSR2, OUT_CSR1, OUT_CSR2, MAXP;
};

struct S3C24xxUsb {

	struct SocDma *dma;
	struct SocIc *ic;
	
	struct UsbEp ep[NUM_EPS];
	uint8_t EP_INT_REG, USB_INT_REG, EP_INT_EN_REG, USB_INT_EN_REG, PWR_REG, INDEX_REG;
};


static bool s3c24xxUsbPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxUsb *usb = (struct S3C24xxUsb*)userData;
	uint32_t val = 0;
	
	if ((size != 4 && size != 1) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_USB_BASE) >> 2;

	if (write)
		val = (size == 4) ? *(uint32_t*)buf : *(uint8_t*)buf;
	
	switch (pa) {
		
		case 0x144 / 4:
			if (write)
				usb->PWR_REG = val & 0x0f;
			else
				val = usb->PWR_REG;
			break;
		
		case 0x148 / 4:
			if (write)
				usb->EP_INT_REG &=~ (val & 0x1f);
			else
				val = usb->EP_INT_REG;
			break;
		
		case 0x158 / 4:
			if (write)
				usb->USB_INT_REG &=~ (val & 0x07);
			else
				val = usb->USB_INT_REG;
			break;
		
		case 0x15c / 4:
			if (write)
				usb->EP_INT_EN_REG = val & 0x1f;
			else
				val = usb->EP_INT_EN_REG;
			break;
		
		case 0x16c / 4:
			if (write)
				usb->USB_INT_EN_REG = val & 0x05;
			else
				val = usb->USB_INT_EN_REG;
			break;
		
		case 0x178 / 4:
			if (write)
				usb->INDEX_REG = (val >= NUM_EPS) ? 0 : val;
			else
				val = usb->INDEX_REG;
			break;
		
		case 0x180 / 4:
			if (write)
				usb->ep[usb->INDEX_REG].MAXP = val & 0x0f;
			else
				val = usb->ep[usb->INDEX_REG].MAXP;
			break;
		
		case 0x184 / 4:
			if (write)
				usb->ep[usb->INDEX_REG].IN_CSR1 = (usb->ep[usb->INDEX_REG].IN_CSR1 & 0x19) | (val & 0xee);
			else
				val = usb->ep[usb->INDEX_REG].IN_CSR1;
			break;
		
		case 0x188 / 4:
			if (write)
				usb->ep[usb->INDEX_REG].IN_CSR2 = val & 0xf0;
			else
				val = usb->ep[usb->INDEX_REG].IN_CSR2;
			break;
		
		case 0x190 / 4:
			if (write)
				usb->ep[usb->INDEX_REG].OUT_CSR1 = (usb->ep[usb->INDEX_REG].OUT_CSR1 & 0x41) | (val & 0xb0);
			else
				val = usb->ep[usb->INDEX_REG].OUT_CSR1;
			break;
		
		case 0x194 / 4:
			if (write)
				usb->ep[usb->INDEX_REG].OUT_CSR2 = val & 0xe0;
			else
				val = usb->ep[usb->INDEX_REG].OUT_CSR2;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 4)
			*(uint32_t*)buf = val;
		else
			*(uint8_t*)buf = val;
	}
	
	return true;
}

struct S3C24xxUsb* s3c24xxUsbInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma)
{
	struct S3C24xxUsb *usb = (struct S3C24xxUsb*)malloc(sizeof(*usb));
	uint_fast8_t i;
	
	
	if (!usb)
		ERR("cannot alloc USB");
	
	memset(usb, 0, sizeof (*usb));
	usb->dma = dma;
	usb->ic = ic;
	
	usb->EP_INT_EN_REG = 0xff;
	usb->USB_INT_EN_REG = 0x04;
	for (i = 0; i < NUM_EPS; i++)
		usb->ep[i].IN_CSR2 = 0x20;
	
	if (!memRegionAdd(physMem, S3C24XX_USB_BASE, S3C24XX_USB_SIZE, s3c24xxUsbPrvMemAccessF, usb))
		ERR("cannot add USB to MEM\n");
	
	return usb;
}





