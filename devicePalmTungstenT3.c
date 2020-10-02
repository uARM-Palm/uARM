//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_W86L488.h"
#include "i2cdev_TPS65010.h"
#include "sspdev_TSC210x.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"


static struct Tps65010 *mTps65010;
static struct Tsc210x *mTsc2101;
static struct W86L488 *mW86L488;

bool deviceHasGrafArea(void)
{
	return false;
}

enum RomChipType deviceGetRomMemType(void)
{
	return RomStrataFlash16x;
}

uint32_t deviceGetRamSize(void)
{
	return 64UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 1; //PXA26x
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	mTsc2101 = tsc210xInitSsp(sp->ssp, sp->gpio, 24, 37, TscType2101);
	if (!mTsc2101)
		ERR("Cannot init TSC2101");
	
	mTps65010 = tps65010Init(sp->i2c);
	if (!mTps65010)
		ERR("Cannot init TPS65010");
		
	mW86L488 = w86l488init(sp->mem, sp->gpio, W86L488_BASE_T3, vsd, 8);
	if (!mW86L488)
		ERR("Cannot init W86L488");
	
	//on the T3: hwuart is the debug/logging/etc port
	
	//gpios
	socGpioSetState(sp->gpio, 1, true);				//reset button
	socGpioSetState(sp->gpio, 2, !vsd);				//card in? (PXA gpio to check this)
	socGpioSetState(sp->gpio, 3, true);				//slider open
	socGpioSetState(sp->gpio, 12, true);			//hotsync button
	socGpioSetState(sp->gpio, 14, true);			//TPS int?
	
	//full battery
	tsc210xSetExtAdc(mTsc2101, TscExternalAdcBat1, 4100);
	
	//keypad
	if (!keypadAddGpioKey(kp, SDLK_HOME, 12, false))
		ERR("Cannot init hotsync button");
	
	if (!keypadDefineCol(kp, 0, 0) || !keypadDefineCol(kp, 1, 10) || !keypadDefineCol(kp, 2, 11))
		ERR("Cannot init keypad cols");
	
	if (!keypadDefineRow(kp, 0, 19) || !keypadDefineRow(kp, 1, 20) || !keypadDefineRow(kp, 2, 21) || !keypadDefineRow(kp, 3, 22) || !keypadDefineRow(kp, 4, 33))
		ERR("Cannot init keypad rows");
	
	if (!keypadAddMatrixKey(kp, SDLK_F1, 4, 0))
		ERR("Cannot init hardkey1 (datebook)\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F2, 0, 0))
		ERR("Cannot init hardkey2 (address)\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F3, 2, 2))
		ERR("Cannot init hardkey3 (ToDo)\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F4, 1, 2))
		ERR("Cannot init hardkey4 (Memos)\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F5, 3, 0))
		ERR("Cannot init hardkey5 (Voice Rec)\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_DOWN, 0, 1))
		ERR("Cannot init down key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_UP, 4, 1))
		ERR("Cannot init up key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_LEFT, 1, 1))
		ERR("Cannot init left key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_RIGHT, 2, 1))
		ERR("Cannot init right key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_RETURN, 3, 2))
		ERR("Cannot init select key\n");
	
	sp->dbgUart = sp->uarts[1];	//HWUART
}

void devicePeriodic(uint32_t cycles)
{
	if(!(cycles & 0x00007FFFUL))
		tsc210xPeriodic(mTsc2101);
}

void deviceTouch(int x, int y)
{
	//mimic values T|T3 adc actually produces
	// as X coord varies from 0 to 319, ADC values go from 3728 to 300
	// as Y coord varies from 0 to 479, ADC values go from 3793 to 153
	
	x = x >= 0 ? 300 + (319 - x) * 107 / 10 : x;
	y = y >= 0 ? 153 + (479 - y) * 76 / 10 : y;
	
	tsc210xPenInput(mTsc2101, x, y);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}
