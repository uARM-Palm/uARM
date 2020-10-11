//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "ac97dev_WM9712L.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"

/*
	GPIO	DIR	VAL	AFR	EDGE		NOTES
	0		IN		0		
	1		IN		1				gpio reset
	2		IN		0	FE 			hard "power" (active low)
	3		OUT	LO	0		
	4		OUT	LO	0		
	5		OUT	LO	1				MMC CLK
	6		OUT	LO	0		
	7		IN		0	RE 			?? usb detect (active high)
	8		OUT	LO	1				MMC CS
	9		IN		0	RE 			?? external power detect (active high)
	10		IN		0	RE FE 		MMC card detect (low when inserted)
	11		IN		0	FE 			hard "address" (active low)
	12		IN		0	RE 			?? headphones decect (active high)
	13		IN		0	FE 			hard "datebook" (active low)
	14		IN		0	FE 			hard "5-way center" (active low)
	15		OUT	LO	0		
	16		OUT	HI	2				pwm0
	17		OUT	HI	2				pwm1
	18		OUT	LO	0		
	19		IN		0	FE 	 		hard "5-way left" (active low)
	20		IN		0	FE 	 		hard "5-way right" (active low)
	21		IN		0	FE 	 		hard "5-way down" (active low)
	22		IN		0	FE 	 		hard "5-way up" (active low)
	23		OUT	LO	0		
	24		OUT	LO	0		
	25		OUT	LO	0		
	26		OUT	LO	0		
	27		OUT	LO	0		
	28		IN		1				AC96 bit_clk
	29		IN		1				AC97 Sdata_in0
	30		OUT	LO	2				AC97 Sdata_out
	31		OUT	LO	2				AC97 sync out
	32		OUT	HI	0		
	33		OUT	LO	0		
	34		OUT	LO	1				FFRXD if configured for IN, which it is not
	35		OUT	LO	0		
	36		OUT	LO	0		
	37		OUT	LO	0		
	38		OUT	LO	0		
	39		OUT	LO	2				FFTXD
	40		IN		0		
	41		OUT	LO	0		
	42		OUT	LO	0		
	43		OUT	LO	0		
	44		OUT	LO	0		
	45		OUT	LO	0		
	46		IN		2				STUART RX
	47		OUT	LO	1				STUART TX
	48		OUT	LO	0		
	49		IN		0		
	50		IN		0	RE 			pen down from codec (active high)
	51		IN		0				MMC card write protect (high when protected)
	52		OUT	LO	0		
	53		IN		0		
	54		OUT	HI	0		
	55		OUT	LO	0				MMC POWER (active high)
	56		OUT	HI	0		
	57		OUT	LO	0		
	58		OUT	LO	2				LDD[0]
	59		OUT	LO	2				LDD[1]		
	60		OUT	LO	2				LDD[2]
	61		OUT	LO	2				LDD[3]
	62		OUT	LO	2				LDD[4]
	63		OUT	LO	2				LDD[5]
	64		OUT	LO	2				LDD[6]
	65		OUT	LO	2				LDD[7]
	66		OUT	LO	0		
	67		OUT	LO	0		
	68		OUT	LO	0		
	69		OUT	LO	0		
	70		OUT	LO	0		
	71		OUT	LO	0		
	72		OUT	LO	0		
	73		OUT	HI	0		
	74		OUT	LO	2				LCD FCLK
	75		OUT	LO	2				LCD LCLK
	76		OUT	LO	2				LCD PCLK
	77		OUT	LO	2				LCD AC BIAS
	78		OUT	LO	0		
	79		OUT	LO	0		
	80		OUT	LO	0		
*/

struct Device {

	struct WM9712L *wm9712L;
};

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
	return 16UL << 20;
}

uint_fast8_t deviceGetSocRev(void)
{
	return 0;	//PXA25x
}

struct Device* deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	struct Device *dev;
	
	dev = (struct Device*)malloc(sizeof(*dev));
	if (!dev)
		ERR("cannot alloc device");

	dev->wm9712L = wm9712LInit(sp->ac97, sp->gpio, 50);
	if (!dev->wm9712L)
		ERR("Cannot init WM9712L");
	
	if (!keypadAddGpioKey(kp, SDLK_F1, 13, false))
		ERR("Cannot init hardkey1 (datebook)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_F2, 11, false))
		ERR("Cannot init hardkey2 (address)\n");
	
	if (!keypadAddGpioKey(kp, SDLK_DOWN, 21, false))
		ERR("Cannot init down key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_UP, 22, false))
		ERR("Cannot init up key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_LEFT, 19, false))
		ERR("Cannot init left key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RIGHT, 20, false))
		ERR("Cannot init right key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_RETURN, 14, false))
		ERR("Cannot init select key\n");
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 2, false))
		ERR("Cannot init power key\n");
	
	wm9712LsetAuxVoltage(dev->wm9712L, WM9712LauxPinBmon, 4200 / 3);		//main battery is 4.2V
	
	socGpioSetState(sp->gpio, 1, true);	//reset button
	socGpioSetState(sp->gpio, 7, false);	//no usb
	socGpioSetState(sp->gpio, 9, false);	//no external power
	socGpioSetState(sp->gpio, 10, !vsd);	//sd card?
	socGpioSetState(sp->gpio, 12, false);	//no headphones
	socGpioSetState(sp->gpio, 40, true);	//no hotsync button pressed

	sp->dbgUart = sp->uarts[0];	//FFUART

	return dev;
}

void devicePeriodic(struct Device *dev, uint32_t cycles)
{
	if (!(cycles & 0x000007FFUL))
		wm9712Lperiodic(dev->wm9712L);
}

void deviceTouch(struct Device *dev, int x, int y)
{
	wm9712LsetPen(dev->wm9712L, (x >= 0 && y >= 0) ? 320 + 18 * x : -1, (x >= 0 && y >= 0) ? 3800 - 16 * y : y, 1000);
}

void deviceKey(struct Device *dev, uint32_t key, bool down)
{
	//nothing
}