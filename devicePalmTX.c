//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_TxNoramMarker.h"
#include "mmiodev_DirectNAND.h"
#include "ac97dev_WM9712L.h"
#include "pxa270_KPC.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
	GPIO	DIR	VAL	AFR	EDGE	NOTES
	?

*/

struct Device {
	
	struct TxNoRamMarker *noRamMarker;
	struct DirectNAND *nand;
	struct WM9712L *wm9712L;
	struct PxaKpc *kpc;
};

bool deviceHasGrafArea(void)
{
	return false;
}

enum RomChipType deviceGetRomMemType(void)
{
	return RomWriteError;
}

uint32_t deviceGetRamSize(void)
{
	return 32UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 2;	//PXA27x
}

struct Device* deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	static const struct NandSpecs nandSpecs = {
		.bytesPerPage = 2112,
		.blocksPerDevice = 1024,
		.pagesPerBlockLg2 = 6,
		.flags = NAND_HAS_SECOND_READ_CMD,
		.devIdLen = 5,
		.devId = {0xec, 0xf1, 0x00, 0x95, 0x40},
	};
	struct Device *dev;
	
	dev = (struct Device*)malloc(sizeof(*dev));
	if (!dev)
		ERR("cannot alloc device");
	
	dev->kpc = (struct PxaKpc*)sp->kpc;
	
	dev->nand = directNandInit(sp->mem, 0x06000000ul, 0x05000000ul, 0x04000000ul, 0x00fffffful, sp->gpio, 18, &nandSpecs, nandFile);
	if (!dev->nand)
		ERR("Cannot init NAND");
	
	dev->noRamMarker = txNoRamMarkerInit(sp->mem);
	if (!dev->noRamMarker)
		ERR("Cannot init 'NO RAM HERE' MARKER");
	
	dev->wm9712L = wm9712LInit(sp->ac97, sp->gpio, 39);
	if (!dev->wm9712L)
		ERR("Cannot init WM9712L");
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 0, false))
		ERR("Cannot init power key\n");
	
	wm9712LsetAuxVoltage(dev->wm9712L, WM9712LauxPinBmon, 4200 / 3);		//main battery is 4.2V
	
	socGpioSetState(sp->gpio, 0, true);		//battery high
	socGpioSetState(sp->gpio, 1, true);		//reset button
	socGpioSetState(sp->gpio, 10, true);	//hotsync button
	socGpioSetState(sp->gpio, 14, !vsd);	//sd card?
	
	socGpioSetState(sp->gpio, 37, true);		//no manufacturing test mode please
	socGpioSetState(sp->gpio, 90, true);		//no USB inserted

	return dev;
}

void devicePeriodic(struct Device *dev, uint32_t cycles)
{
	if (!(cycles & 0x000007FFUL))
		wm9712Lperiodic(dev->wm9712L);
	
	if (!(cycles & 0x000000FFUL))
		directNandPeriodic(dev->nand);
}

void deviceTouch(struct Device *dev, int x, int y)
{
	wm9712LsetPen(dev->wm9712L, (x >= 0) ? 320 + 9 * x : -1, (y >= 0) ? 3800 - 8 * y : y, 1000);
}

void deviceKey(struct Device *dev, uint32_t key, bool down)
{
	static const uint32_t map[3][4] = {
		{SDLK_ESCAPE /* power*/, SDLK_F2 /* h2 = cal */, SDLK_UP, SDLK_RIGHT},
		{SDLK_F2 /* h1 = home */, SDLK_F3 /* h3 = addr */, 0, 0},
		{SDLK_RETURN /* center */, SDLK_F4 /* h4 = web */, SDLK_DOWN, SDLK_LEFT},
	};
	uint_fast8_t r, c;
	
	for (c = 0; c < 3; c++) {
		for (r = 0; r < 4; r++) {
			if (map[c][r] == key) {
				pxaKpcMatrixKeyChange(dev->kpc, r, c, down);
				return;
			}
		}
	}
}