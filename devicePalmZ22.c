//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include <stdlib.h>
#include "s3c24xx_ADC.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "nand.h"
#include "RAM.h"

/*
	GPIO CONFIG:
	
	56000000	005e03ff 0041fc00
	56000010	0000002a 000007fa 00000007
	56000020	aaaa02a8 0000eef1 0000ff1e
	56000030	00000000 0000ffff 00000000
	56000040	01155400 0000ec7f 000037e6
	56000050	0000aaaa 000000ff 000000ff
	56000060	ff000008 0000effc 0000f007
	56000070	00140aaa 000001ef 000001ff
	56000080	00010333 00000000 22222222 222222f2
	56000090	22222222                   00000000
	560000a0	00000000 00fffd00 00000000 0000000b
	560000b0	32410002 00000000 300bee08

	all active low
	power		F0
	datebook	F2
	address		F1
	center		F3
	up			F4
	down		F5
	left		F6
	right		F7

	GPIOA		0..31
	GPIOB		32..47
	GPIOC		48..63
	GPIOD		64..79
	GPIOE		80..95
	GPIOF		96..111
	GPIOG		112..127
	GPIOH		128..143
*/

////touch lef tot right is cb to 32d. top to bottom is 370 to 085


static struct ArmRam *mWeirdBusAccess;			//likely for d-cache cleaning
static struct S3C24xxAdc *mAdc;
static struct NAND *mNand;

bool deviceHasGrafArea(void)
{
	return true;
}

enum RomChipType deviceGetRomMemType(void)
{
	return RomWriteIgnore;
}

uint32_t deviceGetRamSize(void)
{
	return 16UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 0;	//S3C2410
}

static void z22nandPrvReady(void *userData, bool ready)
{
	struct SocGpio *gpio = (struct SocGpio*)userData;
	
	socGpioSetState(gpio, 161, ready);
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	static const struct NandSpecs nandSpecs = {
		.bytesPerPage = 528,
		.blocksPerDevice = 2048,
		.pagesPerBlockLg2 = 5,
		.flags = NAND_FLAG_SAMSUNG_ADDRESSED_VIA_AREAS,
		.devIdLen = 2,
		.devId = {0xec, 0x75},
	};

	mNand = nandInit(nandFile, &nandSpecs, z22nandPrvReady, sp->gpio);
	sp->nand = mNand;
	
	mWeirdBusAccess = ramInit(sp->mem, 0xa0000000, 0x800, malloc(0x800));
	if (!mWeirdBusAccess)
		ERR("Cannot init RAM4");
	
	//GPG1 is usb plug detect. high if inserted
	socGpioSetState(sp->gpio, 113, false);
	
	//GPE0 and 1 are usb related
	
	//GPE9 is out and tells the charger what rate to charge at (500mA if high 100mA is low)
	
	if (!keypadAddGpioKey(kp, SDLK_F1, 98, false))
		ERR("Cannot init hardkey1 (datebook)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F2, 97, false))
		ERR("Cannot init hardkey2 (address)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_DOWN, 101, false))
		ERR("Cannot init down key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_UP, 100, false))
		ERR("Cannot init up key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_LEFT, 102, false))
		ERR("Cannot init left key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RIGHT, 103, false))
		ERR("Cannot init right key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RETURN, 99, false))
		ERR("Cannot init select key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 96, false))
		ERR("Cannot init power key\n");
	
	mAdc = (struct S3C24xxAdc*)sp->adc;
	
	//battery is nearly full. device uses AIN0, 3:4 scale
	s3c24xxAdcSetAuxAdc(mAdc, 0, 4100 * 3 / 4);	

	socBootload(sp->soc, 512, mNand);			//Z22 boots from NAND directly
}

void devicePeriodic(uint32_t cycles)
{
	
}

void deviceTouch(int x, int y)
{
	s3c24xxAdcSetPenPos(mAdc, x >= 0 ? 200 + 38 * x / 10 : x, y >= 0 ? 880 - 34 * y / 10 : y);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}
