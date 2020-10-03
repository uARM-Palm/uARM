//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "sspdev_TSC210x.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
	
	XYZ map
	btn		output		input
	up		1			2
	down	1			1
	
	H1		0			1
	H2		0			2
	pwr		0			3
	
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
	return 8UL << 20;
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
	//mpuio7 is sdio interrupt
	//mpuio 5 seems related to usb
	
	//mpuio interrupts: 3,4,5(AC)?
	//shared interrupts: 0(usb),6(touch)
	
	//shared gpios 8 and 9 are SD related. 9 is out, 8 is in
	
	//shared 0: Vusb active high
	socGpioSetState(sp->gpio, 0, false);
	
	//shared 1: VCC_in (Vusb or Vac) active low
	socGpioSetState(sp->gpio, 1, false);
	
	//shared 8 is SD write protect (high if protected)
	socGpioSetState(sp->gpio, 8, false);
	
	//shared 14 is headphone detect (active high)
	socGpioSetState(sp->gpio, 14, false);
	
	//mpuio 2 is AC-power detect (active low)
	socGpioSetState(sp->gpio, 16 + 2, true);
	
	//mpuio 4 is sd card detect (active low)
	socGpioSetState(sp->gpio, 16 + 4, !vsd);
	
	//full battery
	tsc210xSetExtAdc(mTsc210x, TscExternalAdcBat1, 4200);
	
	//keys
	if (!keypadAddMatrixKey(kp, SDLK_F1, 1, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F2, 2, 0))
		ERR("Cannot init hardkey2\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_ESCAPE, 3, 0))
		ERR("Cannot init power key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_DOWN, 1, 1))
		ERR("Cannot init down key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_UP, 2, 1))
		ERR("Cannot init up key\n");
}

void devicePeriodic(uint32_t cycles)
{
	if(!(cycles & 0x00007FFFUL))
		tsc210xPeriodic(mTsc210x);
}

void deviceTouch(int x, int y)
{
	x = x >= 0 ? 945 - x * 5 : x;
	y = y >= 0 ? 458 - 18 * y / 10 : y;
	
	tsc210xPenInput(mTsc210x, x, y);
}


void deviceKey(uint32_t key, bool down)
{
	//nothing
}