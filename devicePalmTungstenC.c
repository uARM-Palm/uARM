//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "ac97dev_UCB1400.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

struct Device {
	struct UCB1400 *ucb1400;
};

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
	return 64UL << 20; //TX rom also supports 128M
}

enum RamTermination deviceGetRamTerminationStyle(void)
{
	return RamTerminationWriteIgnore;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 0;	//PXA25x
}

struct Device* deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	struct ArmRam *wifiMemSpace1, *wifiMemSpace2;
	struct Device *dev;
	
	dev = (struct Device*)malloc(sizeof(*dev));
	if (!dev)
		ERR("cannot alloc device");
	
	dev->ucb1400 = ucb1400Init(sp->ac97, sp->gpio, 3);
	if (!dev->ucb1400)
		ERR("Cannot init UCB1400");
	
	socGpioSetState(sp->gpio, 1, true);	//reset button
	socGpioSetState(sp->gpio, 7, true);	//hotsync button
	
	if (!keypadDefineCol(kp, 0, 0) || !keypadDefineCol(kp, 1, 9)|| !keypadDefineCol(kp, 2, 10) || !keypadDefineCol(kp, 3, 11))
		ERR("Cannot init keypad cols");
	
	if (!keypadDefineRow(kp, 0, 18) || !keypadDefineRow(kp, 1, 19) || !keypadDefineRow(kp, 2, 20) || !keypadDefineRow(kp, 3, 21) || !keypadDefineRow(kp, 4, 22) || !keypadDefineRow(kp, 5, 23) || !keypadDefineRow(kp, 6, 24) || !keypadDefineRow(kp, 7, 25) || !keypadDefineRow(kp, 8, 26) || !keypadDefineRow(kp, 9, 27) || !keypadDefineRow(kp, 10, 79) || !keypadDefineRow(kp, 11, 80))
		ERR("Cannot init keypad rows");
	
	//keypad
	if (!keypadAddMatrixKey(kp, SDLK_F1, 4, 0))
		ERR("Cannot init hardkey1 (datebook)\n");
	
	
	wifiMemSpace1 = ramInit(sp->mem, 0x28000000ul, 1024, (uint32_t*)malloc(1024));
	if(!wifiMemSpace1)
		ERR("Cannot init RAM4");
	
	wifiMemSpace2 = ramInit(sp->mem, 0x20000000ul, 1024, (uint32_t*)malloc(1024));
	if(!wifiMemSpace2)
		ERR("Cannot init RAM3");
	
	socGpioSetState(sp->gpio, 13, true);	//wifi not ready
	socGpioSetState(sp->gpio, 14, true);	//wifi not powered
	
	socGpioSetState(sp->gpio, 12, true);	//sd card not inserted

	ucb1400setGpioInputVal(dev->ucb1400, 0, false);	//external power is not on
	ucb1400setGpioInputVal(dev->ucb1400, 1, false);	//no headphones plugged in
	//UCB1400 pin3 is active high to enable speaker (active high)
	//UCB1400 pin 5 is vibrate (active high)
	//UCB1400 pin 7 is LED (active low)
	
	
	//AUX2 and 3 appear to be dock related
	//AUX0 is battery voltage
	ucb1400setAuxVoltage(dev->ucb1400, 0, 3800);		//i think this one measures voltage...
	
	sp->dbgUart = sp->uarts[0];	//FFUART
	
	return dev;
}

void devicePeriodic(struct Device *dev, uint32_t cycles)
{
	if (!(cycles & 0x000007FFUL))
		ucb1400periodic(dev->ucb1400);
}

void deviceTouch(struct Device *dev, int x, int y)
{
	ucb1400setPen(dev->ucb1400, (x >= 0 && y >= 0) ? 320 + 2 * x : -1, (x >= 0 && y >= 0) ? 960 - 2 * y : -1);
}

void deviceKey(struct Device *dev, uint32_t key, bool down)
{
	//nothing
}