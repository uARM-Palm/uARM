//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "sspdev_TSC210x.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
	E button map (3 lowest outputs driven, only 3 used, all 5 inputs used):
	
	btn		output		input
	up		1			2
	down	1			1
	left	1			0
	right	1			3
	center	1			4
	H1		0			0
	H2		0			1
	H3		0			2
	H4		0			3
	pwr		0			4
	
*/

static struct ArmRam *mWeirdBusAccess;
static struct Tsc210x *mTsc210x;

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
	return 0;
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	uint_fast8_t i;
	
	mWeirdBusAccess = ramInit(sp->mem, 0x08000000, 0x280, (uint32_t*)malloc(0x280));
	if (!mWeirdBusAccess)
		ERR("Cannot init RAM4");
	
	//PINTDAV is gpio shared 6
	mTsc210x = tsc210xInitUWire(sp->uw, 0, sp->gpio, 6, TscType2102);
	if (!mTsc210x)
		ERR("Cannot init TSC2102");
	
	for (i = 0; i < 2; i++) {
		if (!keypadDefineCol(kp, i, 32 + i))
			ERR("Cannot init keypad col %u as gpio %u", i, 32 + i);
	}
	for (i = 0; i < 5; i++) {
		if (!keypadDefineRow(kp, i, 40 + i))
			ERR("Cannot init keypad row %u as gpio %u", i, 40 + i);
	}
	
	
	//shared 0: Vusb active high
	socGpioSetState(sp->gpio, 0, false);
	
	//shared 1: VCC_in (Vusb or Vac) active low
	socGpioSetState(sp->gpio, 1, false);
	
	//shared 8 is SD wreite protect (high if protected)
	socGpioSetState(sp->gpio, 8, false);
	
	//shared 14 is headphone detect (active high)
	socGpioSetState(sp->gpio, 14, false);
	
	//mpuio 2 is AC-power detect (active low)
	socGpioSetState(sp->gpio, 16 + 2, false);
	
	//mpuio 4 is sd card detect (active low)
	socGpioSetState(sp->gpio, 16 + 4, !vsd);
	
	//full battery
	tsc210xSetExtAdc(mTsc210x, TscExternalAdcBat1, 4200);
	
	//keys
	if (!keypadAddMatrixKey(kp, SDLK_F1, 0, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F2, 1, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F3, 2, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F4, 3, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_ESCAPE, 4, 0))
		ERR("Cannot init power key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_DOWN, 1, 1))
		ERR("Cannot init down key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_UP, 2, 1))
		ERR("Cannot init up key\n");

	if (!keypadAddMatrixKey(kp, SDLK_LEFT, 0, 1))
		ERR("Cannot init left key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_RIGHT, 3, 1))
		ERR("Cannot init right key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_RETURN, 4, 1))
		ERR("Cannot init select key\n");
}

void devicePeriodic(uint32_t cycles)
{
	if(!(cycles & 0x00007FFFUL))
		tsc210xPeriodic(mTsc210x);
}

void deviceTouch(int x, int y)
{
	x = x >= 0 ? 966 - x * 28 / 10 : x;
	y = y >= 0 ? 31 + 22 * y / 10 : y;
	
	tsc210xPenInput(mTsc210x, x, y);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}