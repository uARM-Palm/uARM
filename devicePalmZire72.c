//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "ac97dev_WM9712L.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
	GPIO	DIR	VAL	AFR	EDGE	NOTES
	0		IN		0	FE 	
	1		IN		0		
	2		OUT	LO	0		
	3		OUT	HI	0		
	4		OUT	HI	0		
	5		IN		0		
	6		IN		0		
	7		IN		0		
	8		IN		0		
	9		IN		0		
	10		IN		0		
	11		IN		0	RE 	
	12		IN		0		
	13		IN		2			KP_DKIN<7>
	14		IN		0	RE FE 	
	15		IN		0	RE FE 	
	16		OUT	LO	2			PWMOUT<0>
	17		OUT	HI	0		
	18		OUT	LO	0		
	19		OUT	HI	0		
	20		OUT	HI	0		
	21		OUT	LO	0		
	22		OUT	LO	0		
	23		OUT	LO	2		
	24		OUT	LO	2		
	25		OUT	LO	2		
	26		IN		1			SSPRXD
	27		IN		0	RE 	
	28		IN		1			AC97_BITCLK
	29		IN		1			AC97_SDATA_IN_0
	30		OUT	HI	2			AC97_SDATA_OUT
	31		OUT	HI	2			AC97_SYNC
	32		OUT	LO	0		
	33		OUT	LO	0		
	34		IN		0		
	35		IN		1			FFCTS
	36		OUT	HI	0		
	37		IN		0		
	38		OUT	LO	0		
	39		OUT	LO	0		
	40		OUT	LO	0		
	41		OUT	LO	2			FFRTS
	42		IN		1			BTRXD
	43		OUT	LO	2			BTTXD
	44		IN		1			BTCTS
	45		OUT	LO	2			BTRTS
	46		IN		2			STUARTRX
	47		OUT	LO	1			STUARTTX
	48		IN		1			CIF_DD<5>
	49		OUT	HI	0		
	50		IN		1			CIF_DD<3>
	51		IN		1			CIF_DD<2>
	52		IN		1			CIF_DD<4>
	53		OUT	LO	2		
	54		IN		3		
	55		IN		1			CIF_DD<1>
	56		OUT	HI	0		
	57		OUT	LO	0		
	58		OUT	LO	2			LDD<0>
	59		OUT	LO	2			LDD<1>
	60		OUT	LO	2			LDD<2>
	61		OUT	LO	2			LDD<3>
	62		OUT	LO	2			LDD<4>
	63		OUT	LO	2			LDD<5>
	64		OUT	LO	2			LDD<6>
	65		OUT	LO	2			LDD<7>
	66		OUT	LO	2			LDD<8>
	67		OUT	LO	2			LDD<9>
	68		OUT	LO	2			LDD<10>
	69		OUT	LO	2			LDD<11>
	70		OUT	LO	2			LDD<12>
	71		OUT	LO	2			LDD<13>
	72		OUT	LO	2			LDD<14>
	73		OUT	LO	2			LDD<15>
	74		OUT	LO	2			L_FCLK_RD
	75		OUT	LO	2			L_LCLK_A0
	76		OUT	LO	2			L_PCLK_WR
	77		OUT	LO	2			L_BIAS
	78		OUT	LO	0		
	79		OUT	LO	0		
	80		IN		0		
	81		IN		2			CIF_DD<0>
	82		OUT	LO	0		
	83		OUT	LO	0		
	84		IN		3			CIF_FV
	85		IN		3			CIF_LV
	86		IN		0		
	87		IN		0		
	88		OUT	LO	0		
	89		OUT	LO	1			AC97_SYSCLK
	90		OUT	LO	0		
	91		OUT	LO	0		
	92		OUT	LO	0		
	93		IN		2			CIF_DD<6>
	94		IN		0		
	95		OUT	LO	0		
	96		OUT	HI	0		
	97		IN		3			KP_MKIN<3>
	98		OUT	HI	0		
	99		IN		0		
	100		IN		1			KP_MKIN<0>
	101		IN		1			KP_MKIN<1>
	102		IN		1			KP_MKIN<2>
	103		OUT	HI	2			KP_MKOUT<0>
	104		OUT	HI	2			KP_MKOUT<1>
	105		OUT	HI	2			KP_MKOUT<2>
	106		OUT	LO	0		
	107		IN		0		
	108		IN		1			CIF_DD<7>
	109		OUT	LO	0		
	110		OUT	LO	0		
	111		OUT	LO	0		
	112		OUT	LO	0		
	113		OUT	HI	0		
	114		OUT	LO	0		
	115		IN		0		
	116		OUT	LO	0		
	117		OUT	HI	0		
	118		OUT	HI	0		
	119		OUT	LO	0		
	120		OUT	LO	0		
	121		IN		0		
	
	

*/

static struct ArmRam *mWeirdBusAccess;			//likely for d-cache cleaning
static struct WM9712L *mWM9712L;

bool deviceHasGrafArea(void)
{
	return true;
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

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	mWeirdBusAccess = ramInit(sp->mem, 0x04090000ul, 0x40, (uint32_t*)malloc(0x40));
	if (!mWeirdBusAccess)
		ERR("Cannot init RAM4");
		
	mWM9712L = wm9712LInit(sp->ac97, sp->gpio, 27);
	if (!mWM9712L)
		ERR("Cannot init WM9712L");
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 0, false))
		ERR("Cannot init power key\n");
	
	wm9712LsetAuxVoltage(mWM9712L, WM9712LauxPinBmon, 4200 / 3);		//main battery is 4.2V
	
	socGpioSetState(sp->gpio, 1, true);		//reset button
	socGpioSetState(sp->gpio, 14, !vsd);	//sd card?
	socGpioSetState(sp->gpio, 37, true);	//tell MfgTestExt that this is not a factory test rig
}

void devicePeriodic(uint32_t cycles)
{
	if (!(cycles & 0x000007FFUL))
		wm9712Lperiodic(mWM9712L);
}

void deviceTouch(int x, int y)
{
	wm9712LsetPen(mWM9712L, (x >= 0) ? 320 + 9 * x : -1, (y >= 0) ? 3800 - 8 * y : y, 1000);
}

void deviceKey(uint32_t key, bool down)
{
	//nothing
}