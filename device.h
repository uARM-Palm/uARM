//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "soc_uWire.h"
#include "soc_GPIO.h"
#include "soc_UART.h"
#include "soc_AC97.h"
#include "soc_SSP.h"
#include "soc_I2C.h"
#include "soc_I2S.h"
#include <stdbool.h>
#include <stdio.h>
#include "keys.h"
#include "nand.h"
#include "SoC.h"
#include "ROM.h"
#include "vSD.h"
#include "mem.h"

struct SocPeriphs {
	//in to deviceSetup
	struct SocAC97 *ac97;
	struct SocGpio *gpio;
	struct SocUwire *uw;
	struct SocI2c *i2c;
	struct SocI2s *i2s;
	struct SocSsp *ssp;
	struct SocSsp *ssp2;	//assp for xscale
	struct SocSsp *ssp3;	//nssp for scale
	struct ArmMem *mem;
	struct SoC *soc;
	
	//PXA order: ffUart, hwUart, stUart, btUart
	struct SocUart *uarts[4];
	
	void *adc;		//some cases need this
	void *kpc;		//some cases need this
	
	//out from deviceSetup
	struct NAND *nand;
	struct SocUart *dbgUart;
};




bool deviceHasGrafArea(void);

uint32_t deviceGetRamSize(void);

enum RomChipType deviceGetRomMemType(void);

uint_fast8_t deviceGetSocRev(void);

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile);
void deviceKey(uint32_t key, bool down);
void devicePeriodic(uint32_t cycles);
void deviceTouch(int x, int y);



#endif
