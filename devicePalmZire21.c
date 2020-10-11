//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "uwiredev_ADS7846.h"
#include <stdlib.h>
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
zire 21 has BB ADS7846E chip for touch

z21 accesses something at PA 08000000 which is flash CS2, which is part of EMIFS, this here are the configs:
	
	EMIFS_CONFIG_REG	0x0000001C
	EMIFS_CS0_CONFIG	0x00212668
	EMIFS_CS1_CONFIG	0x0010fffb
	EMIFS_CS2_CONFIG	0x0010fffb
	EMIFS_CS3_CONFIG	0x0000fffb

	so... nothing is there, fun...


mpuio: (not plugged in, no buttons pressed)
	08c4	0000	ffc4	xxxx
	ffff	fff0	ffe0	0008
	ffff	0000	fffe	fff3
	fe01	0000
	
	IO		DIR		VAL		INT		NOTES
	0		out		0		
	1		out		0		
	2		in		1		FE
	3		out		0		RE
	4		out		0		
	5		out		0		
	6		in		1		
	7		in		1		
	8		in		0		
	9		in		0		
	10		in		0		
	11		in		1		
	12		in		0		
	13		in		0		
	14		in		0		
	15		in		0		

gpio: (not plugged in, no buttons pressed)
	data in		0040
	data out	0880
	dir ctrl	0461
	int ctrl	ffbf
	int mask	ffbe	0, 6
	inst stat	0000
	pin ctrl	ffff	//all assigned to MPU
	
	GPIO	DIR		VAL		INT		NOTES
	0		in		0		FE		USB DETECT (high when plugged in)
	1		out		0		
	2		out		0		
	3		out		0		
	4		out		0		
	5		in		0		
	6		in		1		RE		likely pen detect
	7		out		1		
	8		out		0		
	9		out		0		
	10		in		0		
	11		out		1		
	12		out		0		
	13		out		0		
	14		out		0		
	15		out		0		


	button map (3 lowest outputs driven, only 3 used, all 5 inputs used):
	
	btn		output		input
	up		1			2
	down	1			1
	H1		0			1
	H2		0			2
	pwr		0			4
	


*/

struct Device {
	struct Ads7846 *ads7846;
};

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
	return 8UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 0;
}

struct Device* deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	uint32_t *mem = (uint32_t*)calloc(1,0x280);
	struct ArmRam *weirdBusAccess;
	struct Device *dev;
	uint_fast8_t i;
	
	if (!mem)
		ERR("Cannot alloc weird bus device\n");
	
	weirdBusAccess = ramInit(sp->mem, 0x08000000ul, 0x280, mem);
	if (!weirdBusAccess)
		ERR("Cannot init RAM4");
	
	dev = (struct Device*)malloc(sizeof(*dev));
	if (!dev)
		ERR("cannot alloc device");
	
	memset(dev, 0, sizeof(*dev));
	
	for (i = 0; i < 2; i++) {
		if (!keypadDefineCol(kp, i, 32 + i))
			ERR("Cannot init keypad col %u as gpio %u", i, 32 + i);
	}
	for (i = 0; i < 5; i++) {
		if (!keypadDefineRow(kp, i, 40 + i))
			ERR("Cannot init keypad row %u as gpio %u", i, 40 + i);
	}
	
	dev->ads7846 = ads7846init(sp->uw, 0, sp->gpio, 6);
	if (!dev->ads7846)
		ERR("Cannot init ADS7846");

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
	
	
	//keys
	if (!keypadAddMatrixKey(kp, SDLK_F1, 1, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_F2, 2, 0))
		ERR("Cannot init hardkey1\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_ESCAPE, 4, 0))
		ERR("Cannot init power key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_DOWN, 1, 1))
		ERR("Cannot init down key\n");
	
	if (!keypadAddMatrixKey(kp, SDLK_UP, 2, 1))
		ERR("Cannot init up key\n");
	
	//battery is full
	ads7846setAdc(dev->ads7846, Ads7846auxTypeBatt, 4100);
	
	return dev;
}

void devicePeriodic(struct Device *dev, uint32_t cycles)
{
	
}

void deviceTouch(struct Device *dev, int x, int y)
{
	uint16_t z = (x >= 0 && y >= 0) ? 2048 : 0;
	uint16_t adcX, adcY;
	
	adcX = 3570 - x * 175 / 10;
	adcY = 3750 - y * 158 / 10;
	
	ads7846penInput(dev->ads7846, adcX, adcY, z);
}

void deviceKey(struct Device *dev, uint32_t key, bool down)
{
	//nothing
}
