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

	all active low, except as noted
	power		F0	(active high)
	hard1		F1
	up			F2
	down		F3
	left		F6
	right		F7
	center		G0
	resetBTN	G1	(active high)  //YES!

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
	return false;
}

enum RomChipType deviceGetRomMemType(void)
{
	return RomWriteIgnore;
}

uint32_t deviceGetRamSize(void)
{
	return 64UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 1;	//S3C2440
}

static void pda32nandPrvReady(void *userData, bool ready)
{
	struct SocGpio *gpio = (struct SocGpio*)userData;
	
	socGpioSetState(gpio, 161, ready);
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	static const struct NandSpecs nandSpecs = {
		.bytesPerPage = 2112,
		.blocksPerDevice = 1024,
		.pagesPerBlockLg2 = 6,
		.flags = NAND_HAS_SECOND_READ_CMD,
		.devIdLen = 5,
		.devId = {0xec, 0xf1, 0x00, 0x95, 0x40},
	};
	
	mNand = nandInit(nandFile, &nandSpecs, pda32nandPrvReady, sp->gpio);
	sp->nand = mNand;
	
	mWeirdBusAccess = ramInit(sp->mem, 0xa0000000, 0x800, malloc(0x800));
	if (!mWeirdBusAccess)
		ERR("Cannot init RAM4");
	
	//NAND config
	socGpioSetState(sp->gpio, 162, true);		//NCON= 1 (large page NAND)
	socGpioSetState(sp->gpio, 125, true);		//G13 = 1 (2Kbyte page)
	socGpioSetState(sp->gpio, 126, false);		//G14 = 0 (4 address bytes)
	socGpioSetState(sp->gpio, 127, false);		//G15 = 0 (8-bit bus)
	
	//GPB7 is charge detect. low if charging
	socGpioSetState(sp->gpio, 39, true);
	
	//GPG8 is SD detect (active low)
	socGpioSetState(sp->gpio, 120, true);
	
	//GPJ9 is SD write protect (high if protected)
	socGpioSetState(sp->gpio, 153, true);
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 96, true))
		ERR("Cannot init power key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F1, 97, false))
		ERR("Cannot init hardkey1 (brightness)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_UP, 98, false))
		ERR("Cannot init up key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_DOWN, 99, false))
		ERR("Cannot init down key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_LEFT, 102, false))
		ERR("Cannot init left key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RIGHT, 103, false))
		ERR("Cannot init right key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RETURN, 112, false))
		ERR("Cannot init select key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F12, 113, true))	//reset button
		ERR("Cannot init reset key\n");
	
	mAdc = (struct S3C24xxAdc*)sp->adc;
	
	//battery is nearly full. device uses AIN0, 1:3.2 scale
	s3c24xxAdcSetAuxAdc(mAdc, 0, 4100 * 10 / 32);

	socBootload(sp->soc, 2048 | 0xc0000000ul, mNand);			//PDA32 boots from NAND directly
}

void devicePeriodic(uint32_t cycles)
{
	
}

void deviceTouch(int x, int y)
{
	s3c24xxAdcSetPenPos(mAdc, x >= 0 ? 85 + 2 * x : x, y >= 0 ? 780 - 14 * y / 10 : y);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}
