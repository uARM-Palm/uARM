//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_IC.h"
#include "soc_GPIO.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


//for external users we act as if each gpio port has 16 pins, except A that has 32
//we have 8 ports: A .. J with pins 0 .. 143. s3c2410 has no port J
//there is also an implied read-only port with these signals:
// nBatt_FLT, R/nB, NCON, nWAIT

#define NUM_GPIOS_FIRST_BANK	32
#define USEFUL_GPIOS_FIRST_BANK	25	//23 in s3c2410
#define NUM_GPIOS_NORMAL_BANK	16
#define NUM_NORMAL_BANKS		8
#define NUM_SPECIAL_GPIOS		4

#define S3C24XX_GPIO_BASE		0x56000000UL
#define S3C24XX_GPIO_SIZE		256

struct SamsungNormalGpioBank {
	
	GpioChangedNotifF notifF[16];
	void *notifD[16];
	
	//regs
	uint32_t con;
	uint16_t up;
	
	//state
	uint16_t latches;
	uint16_t inputs;
	uint16_t prevVals;
	
	//config
	uint16_t numGpios	:5;		//in this bank
	uint16_t irqOfst	:4;		//in gpio irq numbering
	uint16_t hasIrq		:1;		//does it at all?
};

struct SocGpio {

	struct SocIc *ic;
	bool soc40;
	
	//bank A. 32 bits. out or AFR
	uint32_t acon, adat;
	GpioChangedNotifF anotifF[USEFUL_GPIOS_FIRST_BANK];
	void *anotifD[USEFUL_GPIOS_FIRST_BANK];
	
	//B..J, 16 pins each
	struct SamsungNormalGpioBank bank[NUM_NORMAL_BANKS];
	
	//implied port
	uint8_t impliedPort;
	
	GpioDirsChangedF dirNotifF;
	void *dirNotifD;
	
	//irq stuff
	uint32_t exint[3], eintflt[4], eintmask, eintpend;
	
	//non-gpio stuff
	uint32_t misccr, dclkcon, inform[2], dsc[2];
	uint16_t mslcon;
	uint8_t gpStatus2;
};

static void socGpioPrvRecalc(struct SocGpio* gpio)
{
	uint_fast8_t bank, pin, i;
	
	for (bank = 0; bank < NUM_NORMAL_BANKS; bank++) {
		
		uint_fast16_t oldState = gpio->bank[bank].prevVals, pinMask = 1;
		
		gpio->bank[bank].prevVals = 0;
		for (pin = 0; pin < gpio->bank[bank].numGpios; pin++, pinMask <<= 1) {
			
			bool newPinState, oldPinState = !!(oldState & pinMask), pinChanged;
			uint_fast8_t extiNum = gpio->bank[bank].irqOfst + pin;
			
			switch ((gpio->bank[bank].con >> (2 * pin)) & 3) {
				
				case 0:	//input
				case 2:	//AFR that is input
					gpio->bank[bank].prevVals |= gpio->bank[bank].inputs & pinMask;
					break;
				
				case 1:	//output
					gpio->bank[bank].prevVals |= gpio->bank[bank].latches & pinMask;
					break;
				
				case 3:	//AFR that is output: read as zero
					break;
			}
			newPinState = !!(gpio->bank[bank].prevVals & pinMask);
			pinChanged = !newPinState != !oldPinState;
			
			if (pinChanged && gpio->bank[bank].notifF[pin])	//gpio changed? notif?
				gpio->bank[bank].notifF[pin](gpio->bank[bank].notifD[pin], bank * NUM_GPIOS_NORMAL_BANK + pin, oldPinState, newPinState);
			
			if (gpio->bank[bank].hasIrq) switch ((gpio->exint[extiNum / 8] >> (4 * (extiNum % 8))) & 0x07) {
				
				case 0:		//int on low level
					if (!newPinState)
						gpio->eintpend |= (1UL << extiNum);
					break;
				
				case 1:		//int on high level
					if (newPinState)
						gpio->eintpend |= (1UL << extiNum);
					break;
				
				case 2:		//int on falling edge
				case 3:
					if (pinChanged && !newPinState)
						gpio->eintpend |= (1UL << extiNum);
					break;
				
				case 4:		//int on rising edge
				case 5:
					if (pinChanged && newPinState)
						gpio->eintpend |= (1UL << extiNum);
					break;
				
				case 6:		//int on any edge
				case 7:
					if (pinChanged)
						gpio->eintpend |= (1UL << extiNum);
					break;
			}
		}
	}
	socIcInt(gpio->ic, S3C24XX_I_EINT0, !!(gpio->eintpend & 0x00000001UL));
	socIcInt(gpio->ic, S3C24XX_I_EINT1, !!(gpio->eintpend & 0x00000002UL));
	socIcInt(gpio->ic, S3C24XX_I_EINT2, !!(gpio->eintpend & 0x00000004UL));
	socIcInt(gpio->ic, S3C24XX_I_EINT3, !!(gpio->eintpend & 0x00000008UL));
	socIcInt(gpio->ic, S3C24XX_I_EINT4_7, !!(gpio->eintpend & 0x000000f0UL));
	socIcInt(gpio->ic, S3C24XX_I_EINT8_23, !!(gpio->eintpend & 0x00ffff00UL));
}

static bool socGpioPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocGpio* gpio = (struct SocGpio*)userData;
	uint32_t val = 0;
	uint_fast8_t i;
	
	if (size == 1 && !write) {		//support 1-byte reads
		
		uint32_t t;
		
		if (!socGpioPrvMemAccessF(userData, pa &~ 3, 4, false, &t))
			return false;
		
		t >>= 8 * (pa & 3);
		
		*(uint8_t*)buf = t;
		
		return true;
	}

	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_GPIO_BASE) >> 2;

	if (write)
		val = *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write) {
				gpio->acon = val & 0x007fffff;
				if (gpio->dirNotifF)
					gpio->dirNotifF(gpio->dirNotifD);
			}
			else
				val = gpio->acon;
			break;
		
		case 0x04 / 4:
			if (write) {
				val &= 0x007fffff;
				val ^= gpio->adat;	//calc diff
				gpio->adat ^= val;	//apply diff
				
				for (i = 0; i < USEFUL_GPIOS_FIRST_BANK; i++, val >>= 1) {
					
					if ((val & 1) && gpio->anotifF[i])
						gpio->anotifF[i](gpio->anotifD[i], i, !((gpio->adat >> i) & 1), !!((gpio->adat >> i) & 1));
				}
				socGpioPrvRecalc(gpio);
			}
			else
				val = gpio->adat;
			break;
		
		case 0xd0 / 4:
			if (!gpio->soc40)
				return false;
			pa -= 0xd0 / 4;	//make the math work
			pa += 0x80 / 4;
			//fallthrough
		case 0x10 / 4:
		case 0x20 / 4:
		case 0x30 / 4:
		case 0x40 / 4:
		case 0x50 / 4:
		case 0x60 / 4:
		case 0x70 / 4:
			if (write) {
				gpio->bank[pa / (0x10 / 4) - 1].con = val & ((1UL << (2 * gpio->bank[pa / (0x10 / 4) - 1].numGpios)) - 1);
				socGpioPrvRecalc(gpio);
				if (gpio->dirNotifF)
					gpio->dirNotifF(gpio->dirNotifD);
			}
			else
				val = gpio->bank[pa / (0x10 / 4) - 1].con;
			break;
		
		case 0xd4 / 4:
			if (!gpio->soc40)
				return false;
			pa -= 0xd0 / 4;	//make the math work
			pa += 0x80 / 4;
			//fallthrough
		case 0x14 / 4:
		case 0x24 / 4:
		case 0x34 / 4:
		case 0x44 / 4:
		case 0x54 / 4:
		case 0x64 / 4:
		case 0x74 / 4:
			if (write) {
				gpio->bank[pa / (0x10 / 4) - 1].latches = val & ((1UL << gpio->bank[pa / (0x10 / 4) - 1].numGpios) - 1);
				socGpioPrvRecalc(gpio);
			}
			else
				val = gpio->bank[pa / (0x10 / 4) - 1].prevVals;
			break;
		
		case 0xd8 / 4:
			if (!gpio->soc40)
				return false;
			pa -= 0xd0 / 4;	//make the math work
			pa += 0x80 / 4;
			//fallthrough
		case 0x18 / 4:
		case 0x28 / 4:
		case 0x38 / 4:
		case 0x48 / 4:
		case 0x58 / 4:
		case 0x68 / 4:
		case 0x78 / 4:
			if (write) {
				gpio->bank[pa / (0x10 / 4) - 1].up = val & ((1UL << gpio->bank[pa / (0x10 / 4) - 1].numGpios) - 1);
				//no need for recalc - we do not consider pullups i nour calculations
			}
			else
				val = gpio->bank[pa / (0x10 / 4) - 1].up;
			break;
		
		case 0x80 / 4:
			if (write)
				gpio->misccr = val & 0x007f377b;
			else
				val = gpio->misccr;
			break;
		
		case 0x84 / 4:
			if (write)
				gpio->dclkcon = val & 0x0ff30ff3;
			else
				val = gpio->dclkcon;
			break;
		
		case 0x88 / 4:
		case 0x8C / 4:
		case 0x90 / 4:
			if (write) {
				gpio->exint[pa - 0x88 / 4] = val;
				socGpioPrvRecalc(gpio);
			}
			else
				val = gpio->exint[pa - 0x88 / 4];
			break;
		
		case 0x94 / 4:
		case 0x98 / 4:
		case 0x9C / 4:
		case 0xA0 / 4:
			if (write)
				gpio->eintflt[pa - 0x94 / 4] = val;
			else
				val = gpio->eintflt[pa - 0x88 / 4];
			break;
		
		case 0xA4 / 4:
			if (write)
				gpio->eintmask = val & 0x00fffff0ul;
			else
				val = gpio->eintmask;
			break;
		
		case 0xA8 / 4:
			if (write)
				gpio->eintpend &=~ (val &~ 0x0ful);	//we use lower 4 bits but client isn't supposed to know
			else
				val = gpio->eintpend &~ 0x0ful;
			break;
		
		case 0xAC / 4:
			if (write)
				return false;
			else
				val = gpio->impliedPort;
			break;
		
		case 0xB0 / 4:
			if (write)
				return false;
			else
				val = gpio->soc40 ? 0x32440001ul : 0x32410002ul;
			break;
		
		case 0xB4 / 4:
			if (write)
				gpio->gpStatus2 &=~ val;
			else
				val = gpio->gpStatus2;
			break;
		
		case 0xB8 / 4:
		case 0xBC / 4:
			if (write)
				gpio->inform[pa - 0xB8 / 4] = val;
			else
				val = gpio->inform[pa - 0xB8 / 4];
			break;
		
		case 0xc4 / 4:
		case 0xc8 / 4:
			if (!gpio->soc40)
				return false;
			if (write)
				gpio->dsc[pa - 0xc4 / 4] = val;
			else
				val = gpio->dsc[pa - 0xB8 / 4];
			break;
		
		case 0xcc / 4:
			if (!gpio->soc40)
				return false;
			if (write)
				gpio->mslcon = val & 0xfff;
			else
				val = gpio->mslcon;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct SocGpio* socGpioInit(struct ArmMem *physMem, struct SocIc *ic, uint_fast8_t socRev)
{
	struct SocGpio *gpio = (struct SocGpio*)malloc(sizeof(*gpio));
	
	if (!gpio)
		ERR("cannot alloc GPIO");
	
	memset(gpio, 0, sizeof (*gpio));
	gpio->soc40 = !!socRev;
	gpio->ic = ic;
	
	gpio->acon = 0x007FFFFFUL;
	gpio->bank[0].numGpios = 11;						//B bank
	gpio->bank[1].numGpios = 16;						//C bank
	gpio->bank[2].numGpios = 16;						//D bank
	gpio->bank[2].up = 0x0000f000ul;
	gpio->bank[3].numGpios = 16;						//E bank
	gpio->bank[4].numGpios = 8;							//F bank
	gpio->bank[4].hasIrq = 1;
	gpio->bank[5].numGpios = 16;						//G bank
	gpio->bank[5].irqOfst = 8;
	gpio->bank[5].hasIrq = 1;
	gpio->bank[6].numGpios = gpio->soc40 ? 9 : 11;		//H bank
	if (gpio->soc40)
		gpio->bank[7].numGpios = 13;					//J bank
	
	gpio->misccr = gpio->soc40 ? 0x00010020ul : 0x00010330ul;
	gpio->eintmask = 0x00fffff0ul;
	gpio->gpStatus2 = 0x01;
	
	if (!memRegionAdd(physMem, S3C24XX_GPIO_BASE, S3C24XX_GPIO_SIZE, socGpioPrvMemAccessF, gpio))
		ERR("cannot add GPIO to MEM\n");
	
	socGpioPrvRecalc(gpio);
	
	return gpio;
}

void socGpioSetState(struct SocGpio *gpio, uint_fast8_t gpioNum, bool on)
{
	uint_fast8_t orig = gpioNum;
	
	if (gpioNum < NUM_GPIOS_FIRST_BANK) {
		
		ERR("GPIOA has no settable pins on S3C24xx\n");
	}
	else {
		gpioNum -= NUM_GPIOS_FIRST_BANK;
		if (gpioNum < NUM_GPIOS_NORMAL_BANK * NUM_NORMAL_BANKS) {
			
			uint_fast8_t bankNo = gpioNum / NUM_GPIOS_NORMAL_BANK;
			uint_fast8_t pinNo = gpioNum % NUM_GPIOS_NORMAL_BANK;
			
			if (pinNo > gpio->bank[bankNo].numGpios)
				ERR("GPIO%c has no pin %u on S3C24xx\n", 'B' + bankNo, pinNo);
			
			if (on)
				gpio->bank[bankNo].inputs |= 1UL << pinNo;
			else
				gpio->bank[bankNo].inputs &=~ (1UL << pinNo);
		}
		else {
			gpioNum -= NUM_GPIOS_NORMAL_BANK * NUM_NORMAL_BANKS;
			
			if (gpioNum < NUM_SPECIAL_GPIOS) {
				
				if (on)
					gpio->impliedPort |= 1UL << gpioNum;
				else
					gpio->impliedPort &=~ (1UL << gpioNum);
			}
			else
				ERR("gpio pin %u not understood\n", orig);
		}
	}
	
	socGpioPrvRecalc(gpio);
}

enum SocGpioState socGpioGetState(struct SocGpio *gpio, uint_fast8_t gpioNum)
{
	uint_fast8_t orig = gpioNum;
	
	if (gpioNum < NUM_GPIOS_FIRST_BANK) {
		
		if (gpioNum >= USEFUL_GPIOS_FIRST_BANK) {
			ERR("GPIOA has no settable pin %u on S3C24xx\n", gpioNum);
			return SocGpioStateNoSuchGpio;
		}
		else if ((gpio->acon >> gpioNum) & 1)
			return SocGpioStateAFR0;
		return ((gpio->adat >> gpioNum) & 1) ? SocGpioStateHigh : SocGpioStateLow;
	}
	gpioNum -= NUM_GPIOS_FIRST_BANK;
	
	if (gpioNum < NUM_GPIOS_NORMAL_BANK * NUM_NORMAL_BANKS) {
		
		uint_fast8_t bankNo = gpioNum / NUM_GPIOS_NORMAL_BANK;
		uint_fast8_t pinNo = gpioNum % NUM_GPIOS_NORMAL_BANK;
		
		if (pinNo > gpio->bank[bankNo].numGpios) {
			
			ERR("GPIO%c has no pin %u on S3C24xx\n", 'B' + bankNo, pinNo);
			return SocGpioStateNoSuchGpio;
		}
		switch ((gpio->bank[bankNo].con >> (pinNo * 2)) & 3) {
			case 0:
				return SocGpioStateHiZ;
			case 1:
				return ((gpio->bank[bankNo].latches >> pinNo) & 1) ? SocGpioStateHigh : SocGpioStateLow;
			case 2:
				return SocGpioStateAFR0;
			case 3:
				return SocGpioStateAFR1;
		}
	}
	gpioNum -= NUM_GPIOS_NORMAL_BANK * NUM_NORMAL_BANKS;
	if (gpioNum < NUM_SPECIAL_GPIOS)
		return ((gpio->impliedPort >> gpioNum) & 1) ? SocGpioStateHigh : SocGpioStateLow;
	
	ERR("gpio pin %u not understood\n", orig);
	return SocGpioStateNoSuchGpio;
}

void socGpioSetNotif(struct SocGpio *gpio, uint_fast8_t gpioNum, GpioChangedNotifF notifF, void* userData)
{
	uint_fast8_t orig = gpioNum;
	
	if (gpioNum < NUM_GPIOS_FIRST_BANK) {
		
		if (gpioNum >= USEFUL_GPIOS_FIRST_BANK)
			ERR("GPIOA has no GPIOA.%u pin on S3C24xx\n", gpioNum);
		else {
			gpio->anotifF[gpioNum] = notifF;
			gpio->anotifD[gpioNum] = userData;
		}
		return;
	}

	gpioNum -= NUM_GPIOS_FIRST_BANK;
	if (gpioNum < NUM_GPIOS_NORMAL_BANK * NUM_NORMAL_BANKS) {
		
		uint_fast8_t bankNo = gpioNum / NUM_GPIOS_NORMAL_BANK;
		uint_fast8_t pinNo = gpioNum % NUM_GPIOS_NORMAL_BANK;
		
		if (pinNo > gpio->bank[bankNo].numGpios)
			ERR("GPIO%c has no pin %u on S3C24xx\n", 'B' + bankNo, pinNo);
		else {
			
			gpio->bank[bankNo].notifF[pinNo] = notifF;
			gpio->bank[bankNo].notifD[pinNo] = userData;
		}
		return;
	}
	
	ERR("gpio pin %u does not support notification\n", orig);
}

void socGpioSetDirsChangedNotif(struct SocGpio *gpio, GpioDirsChangedF notifF, void *userData)
{
	gpio->dirNotifF = notifF;
	gpio->dirNotifD = userData;
}

