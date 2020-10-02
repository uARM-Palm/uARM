//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "sspdev_AD7873.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"



struct Ad7873 {
	
	struct SocGpio *gpio;
	int8_t penIntGpio;
	
	uint16_t prevVal;
	
	bool penDown;
	int16_t penX, penY;
	
	uint16_t vBattAdcVal, vAuxAdcVal;
};



static uint_fast16_t ad7873prvSspClientProc(void* userData, uint_fast8_t nBits, uint_fast16_t got)
{
	struct Ad7873 *ad = (struct Ad7873*)userData;
	uint16_t ret = ad->prevVal;
	

	//sometimes frame gpio is gpio, sometimes frame. in this system we act like we're always selected 
	if (nBits != 12) {
		fprintf (stderr, "expecting 12-bit format!\n");
		return 0;
	}
	
	switch (got) {
		
		case 0x93:	//X+
			ad->prevVal = ad->penX;
			break;
		
		case 0xA6:	//Vbatt
			ad->prevVal = ad->vBattAdcVal;
			break;
		
		case 0xB8:	//Z1
			ad->prevVal = ad->penDown ? 0x7ff : 0x0000;
			break;
		
		case 0xE6:	//AUX (PD0 = 0)	(not sure how these differ)
		case 0xE7:	//AUX (PD0 = 1)
			ad->prevVal = ad->vAuxAdcVal;
			break;
		
		case 0xD3:	//Y+
		case 0xD0:
			ad->prevVal = ad->penY;
			break;
		
		default:
			fprintf(stderr, "AD7873: not sure how to deal with command 0x%04x\n", (unsigned)got);
			ad->prevVal = 0;
			break;
	}
	
	return ret;
}

struct Ad7873* ad7873Init(struct SocSsp* ssp, struct SocGpio* gpio, int8_t penIntGpio /* negative for none */)
{
	struct Ad7873 *ad = (struct Ad7873*)malloc(sizeof(*ad));
	
	if (!ad)
		ERR("cannot alloc AD7873");
	
	memset(ad, 0, sizeof (*ad));
	
	ad->gpio = gpio;
	ad->penIntGpio = penIntGpio;
	
	//connect to the SSP
	if (!socSspAddClient(ssp, ad7873prvSspClientProc, ad))
		ERR("cannot add TSC2101 to SSP\n");
	
	return ad;
}

void ad7873Periodic(struct Ad7873 *ad)
{
	if (ad->penIntGpio >= 0)
		socGpioSetState(ad->gpio, ad->penIntGpio, !ad->penDown);
}

void ad7873PenInput(struct Ad7873 *ad, int16_t x, int16_t y)
{
	ad->penDown = x >= 0 && y >= 0;
	
	if (ad->penDown) {
		ad->penY = x;		//yes they are cross-wired in the TG50
		ad->penX = y;
	}
	else {
		ad->penX = -1;
		ad->penY = -1;
	}
}

void ad7873setVbatt(struct Ad7873 *ad, uint16_t mV)	//Vref is 2.5V
{
	ad->vBattAdcVal = (mV / 4) * 4096 / 2500;
}

void ad7873setVaux(struct Ad7873 *ad, uint16_t mV)
{
	ad->vAuxAdcVal = mV * 4096 / 2500;
}

