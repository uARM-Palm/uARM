//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_IC.h"
#include "soc_GPIO.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"



#define OMAP_SHARED_GPIO_BASE		0xFFFCE000UL
#define OMAP_SHARED_GPIO_SIZE		256

#define OMAP_MPUIO_BASE				0xFFFB5000UL
#define OMAP_MPUIO_SIZE				256

//we treat keyboard scanner as gpios since our keyboard code already works this way
//columns OUT (open collector) are 32..49. rows in (force-input with pullup) are 40..47

struct OmapGpioBank {

	uint8_t ofst;
	uint8_t irqNo;
	
	//all
	uint16_t inputs, latches, dir;
	
	//shared & mpuio
	uint16_t intMask, intSta, prevVals;
	
	//shared gpio only
	uint16_t intCtl, pinCtl;
	
	//mpuio only
	uint16_t eventLatch;
	uint8_t eventConfig;
	uint16_t unknown_0x0c;	//suspect to be like intCtl is for shared
	
	GpioChangedNotifF notifF[16];
	void *notifD[16];
	
	//for edge triggering
	uint16_t prevIntSta;
	bool irqIsEdge;
};

struct SocGpio {

	struct SocIc *ic;
	
	struct OmapGpioBank shared;
	struct OmapGpioBank mpuio;
	struct OmapGpioBank kbd;
	
	uint16_t mpuioDebounce;
	
	GpioDirsChangedF dirNotifF;
	void *dirNotifD;
};

static void socGpioPrvRecalc(struct SocGpio *gpio, struct OmapGpioBank *bank)
{
	uint_fast16_t diff, wentHi, wentLo, desiredChange = 0, vals = ((bank->inputs & bank->dir) | (bank->latches &~ bank->dir)) & bank->pinCtl, intCause;
	bool irq = false;
	uint_fast8_t i;
	
	diff = bank->prevVals ^ vals;
	wentHi = diff & vals;
	wentLo = diff &~ wentHi;
	
	//calc omap changes (what went hi? what went lo?)
	desiredChange |= wentHi & bank->intCtl & ~bank->intMask;
	desiredChange |= wentLo & (~bank->intCtl) & ~bank->intMask;
	
	bank->intSta |= desiredChange;
	intCause = bank->intSta & ~bank->intMask;
	
	if (bank->irqIsEdge) {
		
		if (intCause &~ bank->prevIntSta) {
			//edge
			socIcInt(gpio->ic, bank->irqNo, true);
			socIcInt(gpio->ic, bank->irqNo, false);
		}
		bank->prevIntSta = bank->intSta;
	}
	else {
		//level
		socIcInt(gpio->ic, bank->irqNo, !!intCause);
	}
	
	//notify emulator users
	for (i = 0; i < 16; i++, diff >>= 1) {
		
		if (!(diff & 1))
			continue;
		
		if (!bank->notifF[i])
			continue;
		
		bank->notifF[i](bank->notifD[i], bank->ofst + i, !!(bank->prevVals & (1UL << i)), !!(vals & (1UL << i)));
	}
	
	//handle "event mode"
	if ((bank->eventConfig & 1) && (desiredChange & (1 << (bank->eventConfig >> 1))))
		bank->eventLatch = vals;
	
	bank->prevVals = vals;
}

static bool socGpioPrvMpuioMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocGpio* gpio = (struct SocGpio*)userData;
	uint_fast16_t val = 0, old;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_MPUIO_BASE) >> 2;

	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				return false;	//not allowed
			else
				val = gpio->mpuio.inputs;
			break;
		
		case 0x04 / 4:
			if (write) {
				old = gpio->mpuio.latches;
				gpio->mpuio.latches = val;
				if (gpio->mpuio.latches != old)
					socGpioPrvRecalc(gpio, &gpio->mpuio);
			}
			else
				val = gpio->mpuio.latches;
			break;
		
		case 0x0c / 4:
			if (write)
				gpio->mpuio.unknown_0x0c = val;
			else
				val = gpio->mpuio.unknown_0x0c;
			break;
		
		case 0x08 / 4:
			if (write) {
				old = gpio->mpuio.dir;
				gpio->mpuio.dir |= val;
				if (gpio->mpuio.dir != old) {
					socGpioPrvRecalc(gpio, &gpio->mpuio);
					if (gpio->dirNotifF)
						gpio->dirNotifF(gpio->dirNotifD);
				}
			}
			else
				val = gpio->mpuio.dir;
			break;
		
		case 0x10 / 4:
			if (write)
				;	//not allowed as per docs - ignored in hw
			else
				val = ((gpio->kbd.inputs >> 8) & 0x1f) | 0xffe0;	//all highs are expected on all other bits
			break;
		
		case 0x14 / 4:
			if (write) {
				old = gpio->kbd.dir;
				gpio->kbd.dir &= 0xff00;
				gpio->kbd.dir |= val & 0x00ff;
				if (gpio->kbd.dir != old) {
					socGpioPrvRecalc(gpio, &gpio->kbd);
					if (gpio->dirNotifF)
						gpio->dirNotifF(gpio->dirNotifD);
				}
			}
			else
				val = gpio->kbd.dir & 0x00ff;
			break;
				
		case 0x18 / 4:
			if (write)
				gpio->mpuio.eventConfig = val & 0x3f;
			else
				val = gpio->mpuio.eventConfig;
			break;
		
		case 0x1c / 4:
			if (write) {
				gpio->mpuio.intCtl = val;
				socGpioPrvRecalc(gpio, &gpio->mpuio);
			}
			else
				val = gpio->mpuio.intCtl;
			break;
		
		case 0x20 / 4:
			if (write)
				;	//ignored
			else
				val = ((gpio->kbd.inputs >> 8) & 0x1f) == 0x1f;
			break;
		
		case 0x24 / 4:
			if (write)
				;	//not allowed as per docs - ignored in hw
			else {
				val = gpio->mpuio.intSta &~ gpio->mpuio.intMask;
				gpio->mpuio.intSta = 0;
			}
			break;
		
		case 0x28 / 4:
			if (write)
				gpio->kbd.intMask = (val & 1) ? 0xffff : 0x00ff;
			else
				val = gpio->kbd.intMask >> 15;
			break;
		
		case 0x2c / 4:
			if (write) {
				gpio->mpuio.intMask = val;
				socGpioPrvRecalc(gpio, &gpio->mpuio);
			}
			else
				val = gpio->mpuio.intMask;
			break;
		
		case 0x30 / 4:
			if (write)
				gpio->mpuioDebounce = val & 0x1ff;
			else
				val = gpio->mpuioDebounce;
			break;
		
		case 0x34 / 4:
			if (write)
				;	//not allowed as per docs - ignored in hw
			else
				val = gpio->mpuio.eventLatch;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2) 
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

static bool socGpioPrvSharedMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocGpio* gpio = (struct SocGpio*)userData;
	uint_fast16_t val = 0, old;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_SHARED_GPIO_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				return false;	//not allowed
			else
				val = gpio->shared.inputs & gpio->shared.pinCtl;
			break;
		
		case 0x04 / 4:
			if (write) {
				old = gpio->shared.latches;
				gpio->shared.latches &=~ gpio->shared.pinCtl;
				gpio->shared.latches |= val & gpio->shared.pinCtl;
				if (gpio->shared.latches != old)
					socGpioPrvRecalc(gpio, &gpio->shared);
			}
			else
				val = gpio->shared.latches & gpio->shared.pinCtl;
			break;
		
		case 0x08 / 4:
			if (write) {
				old = gpio->shared.dir;
				gpio->shared.dir &=~ gpio->shared.pinCtl;
				gpio->shared.dir |= val & gpio->shared.pinCtl;
				if (gpio->shared.dir != old) {
					socGpioPrvRecalc(gpio, &gpio->shared);
					if (gpio->dirNotifF)
						gpio->dirNotifF(gpio->dirNotifD);
				}
			}
			else
				val = gpio->shared.dir & gpio->shared.pinCtl;
			break;
		
		case 0x0c / 4:
			if (write) {
				
				gpio->shared.intCtl &=~ gpio->shared.pinCtl;
				gpio->shared.intCtl |= val & gpio->shared.pinCtl;
				socGpioPrvRecalc(gpio, &gpio->shared);
			}
			else
				val = gpio->shared.intCtl & gpio->shared.pinCtl;
			break;
		
		case 0x10 / 4:
			if (write) {
				
				gpio->shared.intMask &=~ gpio->shared.pinCtl;
				gpio->shared.intMask |= val & gpio->shared.pinCtl;
				socGpioPrvRecalc(gpio, &gpio->shared);
			}
			else
				val = gpio->shared.intMask & gpio->shared.pinCtl;
			break;
		
		case 0x14 / 4:
			if (write) {
				gpio->shared.intSta &=~ (val & gpio->shared.pinCtl);
				socGpioPrvRecalc(gpio, &gpio->shared);
			}
			else
				val = gpio->shared.intSta & gpio->shared.pinCtl;
			break;
		
		case 0x18 / 4:
			if (write) {
				gpio->shared.pinCtl = val;
				socGpioPrvRecalc(gpio, &gpio->shared);
			}
			else
				val = gpio->shared.pinCtl;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2) 
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

struct SocGpio* socGpioInit(struct ArmMem *physMem, struct SocIc *ic, uint_fast8_t socRev)
{
	struct SocGpio *gpio = (struct SocGpio*)malloc(sizeof(*gpio));
	
	if (!gpio)
		ERR("cannot alloc GPIO");
	
	memset(gpio, 0, sizeof (*gpio));
	
	gpio->ic = ic;
	
	//shared
	gpio->shared.latches = 0xffff;
	gpio->shared.dir = 0xffff;
	gpio->shared.intCtl = 0xffff;
	gpio->shared.intMask = 0xffff;
	gpio->shared.pinCtl = 0xffff;
	gpio->shared.irqNo = OMAP_I_GPIO;
	
	//mpuio
	gpio->mpuio.dir = 0xffff;
	gpio->mpuio.pinCtl = 0xffff;			//all of these permanently belong to the MPU
	gpio->mpuio.irqNo = OMAP_I_MPUIO;
	gpio->mpuio.ofst = 16;
	
	//keyboard
	gpio->kbd.dir = 0x00ff;					//columns all out, rows all in
	gpio->kbd.irqNo = OMAP_I_KEYBOARD;
	gpio->kbd.intMask = 0xff00;
	gpio->kbd.ofst = 32;
	gpio->kbd.irqIsEdge = true;
	
	if (!memRegionAdd(physMem, OMAP_SHARED_GPIO_BASE, OMAP_SHARED_GPIO_SIZE, socGpioPrvSharedMemAccessF, gpio))
		ERR("cannot add GPIO to MEM\n");

	if (!memRegionAdd(physMem, OMAP_MPUIO_BASE, OMAP_MPUIO_SIZE, socGpioPrvMpuioMemAccessF, gpio))
		ERR("cannot add MPUIO to MEM\n");
	
	socGpioPrvRecalc(gpio, &gpio->shared);
	socGpioPrvRecalc(gpio, &gpio->mpuio);
	socGpioPrvRecalc(gpio, &gpio->kbd);
	
	return gpio;
}

void socGpioSetState(struct SocGpio *gpio, uint_fast8_t gpioNum, bool on)
{
	struct OmapGpioBank *bank;
	uint_fast16_t old;
	
	if (gpioNum < 16)
		bank = &gpio->shared;
	else if (gpioNum < 32)
		bank = &gpio->mpuio;
	else if (gpioNum < 48)
		bank = &gpio->kbd;
	else
		return;
	
	gpioNum -= bank->ofst;
	
	old = bank->inputs;
	
	if (on)
		bank->inputs |= 1UL << gpioNum;
	else
		bank->inputs &=~ (1UL << gpioNum);
	
	if (bank->inputs != old)
		socGpioPrvRecalc(gpio, bank);
}

enum SocGpioState socGpioGetState(struct SocGpio *gpio, uint_fast8_t gpioNum)
{
	struct OmapGpioBank *bank;
	uint_fast16_t old;
	
	if (gpioNum < 16)
		bank = &gpio->shared;
	else if (gpioNum < 32)
		bank = &gpio->mpuio;
	else if (gpioNum < 48)
		bank = &gpio->kbd;
	else
		return SocGpioStateNoSuchGpio;
	
	gpioNum -= bank->ofst;
	
	if (!(bank->pinCtl & (1UL << gpioNum)))		//pins given to the DSP report as AFR
		return SocGpioStateAFR1;
	
	if (bank->dir & (1UL << gpioNum))
		return SocGpioStateHiZ;
	
	return (bank->latches & (1UL << gpioNum)) ? SocGpioStateHigh : SocGpioStateLow;
}

void socGpioSetNotif(struct SocGpio *gpio, uint_fast8_t gpioNum, GpioChangedNotifF notifF, void* userData)
{
	struct OmapGpioBank *bank;
	
	if (gpioNum < 16)
		bank = &gpio->shared;
	else if (gpioNum < 32)
		bank = &gpio->mpuio;
	else if (gpioNum < 48)
		bank = &gpio->kbd;
	else
		return;
	
	gpioNum -= bank->ofst;
	
	bank->notifF[gpioNum] = notifF;
	bank->notifD[gpioNum] = userData;
}

void socGpioSetDirsChangedNotif(struct SocGpio *gpio, GpioDirsChangedF notifF, void *userData)
{
	gpio->dirNotifF = notifF;
	gpio->dirNotifD = userData;
}

