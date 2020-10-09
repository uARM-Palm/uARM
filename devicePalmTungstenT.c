//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "uwiredev_ADS7846.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
	oslo/T|T button map (3 lowest outputs driven, only 3 used, all 5 inputs used):
	
	btn		output		input
	H1		0			0			
	H2		0			1
	H3		0			2
	H4		0			3
	center	0			4
	left	1			0
	down	1			1
	up		1			2
	right	1			3
	pwr		2			0
	voicRec	2			4
	
*/

static struct ArmRam *mWeirdBusAccess;
static struct Ads7846 *mAds7846;

bool deviceHasGrafArea(void)
{
	return true;
}

enum RomChipType deviceGetRomMemType(void)
{
	return RomStrataFlash16x;
}

uint32_t deviceGetRamSize(void)
{
	return 32UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 1;		//omap with DSP
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	uint_fast8_t i;
	
	mWeirdBusAccess = ramInit(sp->mem, 0x08000000ul, 0x280, (uint32_t*)malloc(0x280));
	if (!mWeirdBusAccess)
		ERR("Cannot init RAM4");
	
	for (i = 0; i < 3; i++) {
		if (!keypadDefineCol(kp, i, 32 + i))
			ERR("Cannot init keypad col %u as gpio %u", i, 32 + i);
	}
	for (i = 0; i < 5; i++) {
		if (!keypadDefineRow(kp, i, 40 + i))
			ERR("Cannot init keypad row %u as gpio %u", i, 40 + i);
	}
	
	//shared 0 is pen detect
	mAds7846 = ads7846init(sp->uw, 0, sp->gpio, 6);
	if (!mAds7846)
		ERR("Cannot init ADS7846");
	
	
	//shared 0: Vusb active high
	socGpioSetState(sp->gpio, 0, false);
	
	//xxx: these are from oslo:
		//shared 8 is sd card write protect (high when protected)
		socGpioSetState(sp->gpio, 8, false);
		
		//mpuio 12 is sd card detect (active low)
		socGpioSetState(sp->gpio, 16 + 12, !vsd);
	
	//shared 14 is headphone detect (active high)
	socGpioSetState(sp->gpio, 14, false);
	
	//mpuio 1 is hot sync button (Active high)
	socGpioSetState(sp->gpio, 16 + 1, false);
	
	//mpuio 2 is AC-power detect (active low)
	socGpioSetState(sp->gpio, 16 + 2, false);
	
	//mpuio 3 is slider detect (high when open)
	socGpioSetState(sp->gpio, 16 + 3, true);
	
	//battery is full
	ads7846setAdc(mAds7846, Ads7846auxTypeBatt, 4100);
	
	//keys
	if (!keypadAddMatrixKey(kp, SDLK_F1, 0, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F2, 1, 0))
		ERR("Cannot init hardkey2\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F3, 2, 0))
		ERR("Cannot init hardkey3\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F4, 3, 0))
		ERR("Cannot init hardkey4\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F5, 4, 2))
		ERR("Cannot init hardkey5\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_ESCAPE, 0, 2))
		ERR("Cannot init power key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_DOWN, 1, 1))
		ERR("Cannot init down key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_UP, 2, 1))
		ERR("Cannot init up key\n");

	if (!keypadAddMatrixKey(kp, SDLK_LEFT, 0, 1))
		ERR("Cannot init left key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_RIGHT, 3, 1))
		ERR("Cannot init right key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_RETURN, 4, 0))
		ERR("Cannot init select key\n");
}

void devicePeriodic(uint32_t cycles)
{

}

void deviceTouch(int x, int y)
{
	uint16_t z = (x >= 0 && y >= 0) ? 2048 : 0;
	uint16_t adcX, adcY;
	
	adcX = 3570 - x * 175 / 20;
	adcY = 3750 - y * 158 / 20;
	
	ads7846penInput(mAds7846, adcX, adcY, z);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}