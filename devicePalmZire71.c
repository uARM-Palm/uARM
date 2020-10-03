//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "uwiredev_ADS7846.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*

*/

static struct Ads7846 *mAds7846;
static struct ArmRam *mWeirdBusAccess;

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
	return 16UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 0;
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	uint_fast8_t i;
	
	mWeirdBusAccess = ramInit(sp->mem, 0x08000000, 0x280, (uint32_t*)malloc(0x280));
	if (!mWeirdBusAccess)
		ERR("Cannot init RAM4");
	
	for (i = 0; i < 2; i++) {
		if (!keypadDefineCol(kp, i, 32 + i))
			ERR("Cannot init keypad col %u as gpio %u", i, 32 + i);
	}
	for (i = 0; i < 5; i++) {
		if (!keypadDefineRow(kp, i, 40 + i))
			ERR("Cannot init keypad row %u as gpio %u", i, 40 + i);
	}
	
	mAds7846 = ads7846init(sp->uw, 0, sp->gpio, 6);
	if (!mAds7846)
		ERR("Cannot init ADS7846");

	//shared 0: Vusb active high
	socGpioSetState(sp->gpio, 0, false);
	
	//shared 1: VCC_in (Vusb or Vac) active low
	socGpioSetState(sp->gpio, 1, false);
	
	//shared 14 is headphone detect (active high)
	socGpioSetState(sp->gpio, 14, false);
	
	
	//shared 8 is SD write protect (high if protected)
	socGpioSetState(sp->gpio, 8, false);
	
	//mpuio 4 is sd card detect (active low)
	socGpioSetState(sp->gpio, 16 + 4, !vsd);
	
	//mpuio 2 is AC-power detect (active low)
	socGpioSetState(sp->gpio, 16 + 2, false);
	
	
	//keys
	if (!keypadAddMatrixKey(kp, SDLK_F1, 1, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F2, 2, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_ESCAPE, 4, 0))
		ERR("Cannot init power key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_DOWN, 1, 1))
		ERR("Cannot init down key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_UP, 2, 1))
		ERR("Cannot init up key\n");
	
	//battery is full
	ads7846setAdc(mAds7846, Ads7846auxTypeBatt, 4100);
}

void devicePeriodic(uint32_t cycles)
{
	
}

void deviceTouch(int x, int y)
{
	uint16_t z = (x >= 0 && y >= 0) ? 2048 : 0;
	uint16_t adcX, adcY;
	
	adcX = 3570 - x * 175 / 20;
	adcY = 274 + y * 158 / 20;
	
	ads7846penInput(mAds7846, adcX, adcY, z);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}
