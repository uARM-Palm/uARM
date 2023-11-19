//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_AximX3cpld.h"
#include "mmiodev_W86L488.h"
#include "ac97dev_WM9705.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "ROM.h"

struct Device {
	
	struct ArmRom *secondFlashChip;
	struct AximX3cpld *CPLD;
	struct W86L488 *w86L488;
	struct WM9705 *wm9705;
};

bool deviceHasGrafArea(void)
{
	return false;
}

enum RomChipType deviceGetRomMemType(void)
{
	return RomStrataflash16x2x;
}

uint32_t deviceGetRamSize(void)
{
	return 64UL << 20;
}

enum RamTermination deviceGetRamTerminationStyle(void)
{
	return RamTerminationMirror;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 1;	//PXA26x
}

struct Device* deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	uint32_t romPieceSize = 32UL << 20;
	void *romPiece = malloc(romPieceSize);
	struct Device *dev;
	
	dev = (struct Device*)malloc(sizeof(*dev));
	if (!dev)
		ERR("cannot alloc device");
	
	dev->secondFlashChip = romInit(sp->mem, 0x04000000UL, &romPiece, &romPieceSize, 1, RomStrataflash16x2x);
	if (!dev->secondFlashChip)
		ERR("Cannot init axim's second flash chip");
	
	dev->wm9705 = wm9705Init(sp->ac97);
	if (!dev->wm9705)
		ERR("Cannot init WM9705");
	
	dev->w86L488 = w86l488init(sp->mem, sp->gpio, W86L488_BASE_AXIM, vsd, 8);
	if (!dev->w86L488)
		ERR("Cannot init W86L488");
	
	dev->CPLD = aximX3cpldInit(sp->mem);
	if (!dev->CPLD)
		ERR("Cannot init AXIM's CPLD");
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 0, false))
		ERR("Cannot init power key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F1, 13, true))
		ERR("Cannot init mini key L (voice rec)\n");
		
	if (!keypadAddGpioKey(kp, SDLK_F2, 3, true))
		ERR("Cannot init hardkey 1 (calendar)\n");
		
	if (!keypadAddGpioKey(kp, SDLK_F3, 2, true))
		ERR("Cannot init hardkey 2 (contacts)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F4, 4, true))
		ERR("Cannot init hardkey 3 (inbox)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F5, 11, true))
		ERR("Cannot init hardkey 4 (home)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F5, 9, true))
		ERR("Cannot init mini key R (wireless/media)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_PAGEUP, 16, true))
		ERR("Cannot init hardkey jog up\n");
	
	if (!keypadAddGpioKey(kp, SDLK_PAGEDOWN, 23, true))
		ERR("Cannot init hardkey jog down\n");
	
	if (!keypadAddGpioKey(kp, SDLK_HOME, 22, true))
		ERR("Cannot init hardkey jog select\n");

	if (!keypadAddGpioKey(kp, SDLK_DOWN, 84, true))
		ERR("Cannot init down key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_UP, 82, true))
		ERR("Cannot init up key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_LEFT, 85, true))
		ERR("Cannot init left key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RIGHT, 81, true))
		ERR("Cannot init right key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RETURN, 83, true))
		ERR("Cannot init select key\n");
	
	
	socGpioSetState(sp->gpio, 1, true);	//reset button not active
	
	socGpioSetState(sp->gpio, 5, true);	//battery door is closed
	
	//high end device with no wireless
	socGpioSetState(sp->gpio, 58, true);
	socGpioSetState(sp->gpio, 59, true);
	socGpioSetState(sp->gpio, 60, true);
	socGpioSetState(sp->gpio, 61, true);
	socGpioSetState(sp->gpio, 62, true);
	socGpioSetState(sp->gpio, 63, true);
	
	wm9705setAuxVoltage(dev->wm9705, WM9705auxPinBmon, 4200 / 3);		//main battery is 4.2V
	wm9705setAuxVoltage(dev->wm9705, WM9705auxPinAux, 1200);			//secondary battery is 1.2V
	wm9705setAuxVoltage(dev->wm9705, WM9705auxPinPhone, 1900);			//main battery temp is 10 degrees C
	
	sp->dbgUart = sp->uarts[0];	//FFUART
	
	return dev;
}

void devicePeriodic(struct Device *dev, uint32_t cycles)
{
	if (!(cycles & 0x0000007FUL))
		wm9705periodic(dev->wm9705);
}

void deviceTouch(struct Device *dev, int x, int y)
{
	wm9705setPen(dev->wm9705, (x >= 0 && y >= 0) ? 3930 - 15 * x : -1, (x >= 0 && y >= 0) ? 3864 - 11 * y : -1, 1000);
}

void deviceKey(struct Device *dev, uint32_t key, bool down)
{
	//nothing
}