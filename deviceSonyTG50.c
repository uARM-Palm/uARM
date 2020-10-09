//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_MemoryStickController.h"
#include "i2cdev_AN32502A.h"
#include "mmiodev_TG50uc.h"
#include "sspdev_AD7873.h"
#include "i2sdev_AK4534.h"
#include "SDL2/SDL.h"
#include "device.h"
#include "util.h"
#include "RAM.h"

/*
///palmos irqs from most to least important:

	irqno	source			notes
	0		gpio0			pwr btn?
	15		DMA
	7		LCD
	40						IrcExtn interrupt
	16		OS timer 0		?
	3		USB
	11		BTUART
	12		FFUART
	21		RTC Alarm
	14		SSP
	10		STUART
	9		ICP				??WHY??
	8		I2C
	5		I2S
	23		gpio5			??
	24		gpio6			??
	27		gpio10			MS ip chip ?
	26		gpio9			pen detect ?
	22		gpio3			cradle button
	29		gpio14			keyboard/hardkey int ???
	30		gpio21			??
	35						gpioA.8 change? definitely: Pa1Lib interrupt
	34						gpioA.5 change? 
	39						gpioA.15 change	headphone insert/remove (low when inserted)
	25		gpio7			ms insert/remove ?
	32						gpioA.1 change?
	33						gpioA.4 change? thus hold switch?
	37						gpioA.11 change? 
	36						gpioA.10 change? 
	31						gpioA.4 change? definitely: BtTransportGn interrupt
	38						possibly key related (manipulated always near key mask). maybe hold?
	41
	28		gpio13			seems to be irq for all uC things as per dispatcher
unlisted in prio map but existing:
	1		gpio1 (rst)
	2		gpio2-84		dispatched separately
	4		PMU				unused?
	6		AC97			unused?
	13		MMC				unused?
	17		OS timer 1		unused?
	18		OS timer 2		unused?
	19		OS timer 3		unused?
	20		RTC HZ			unused?
*/


/*
	GPIO	AFR		DIR		STATE	EDGE	NOTES
	0		0		0		1		FE		palmos irq 0, ????
	1		1		0		0				gpio reset
	2		0		0		0				
	3		0		0		0		FE		palmos irq 22, active low "hotsync" cradle button detect
	4		0		1		0				
	5		0		1		0				palmos irq 23
	6		0		0		0				palmos irq 24
	7		0		0		1		RE FE	palmos irq 25, likely memory stick detect (active low)
	8		0		1		0				
	9		0		0		1		RE FE	palmos irq 26, touch interrupt from AD7873
	10		0		0		1		FE		palmos irq 27, memorystick IO interrupt ?
	11		1		1		0				3.6MHz out
	12		0		1		0		
	13		0		0		1		FE		palmos irq 28, related to keys
	14		0		0		0		FE		palmos irq 29, hard key pressed
	15		0		1		1				active low memory stick IP reset?
	16		2		1		0				PWM0 out
	17		2		1		0				PWM1 out
	18		1		0		0				RDY (external bus ready)
	19		1		0		0				DREQ[1] (used by memory stick IP chip)
	20		1		0		0				DREQ[0] (used by memory stick IP chip)
	21		0		0		0		FE		palmos irq 30, related to keys. irq when jog clicks up/down
	22		0		0		0				IFF irq21 interrupted, this is read to tell which it was. hi = up, lo = down
	23		2		1		0				SCLK (SSP)
	24		2		1		1				SFRM (SSP) (AD7873 chip select)
	25		2		1		0				TXD (SSP)
	26		1		0		0				RXD (SSP)
	27		0		1		0
	28		2		0		0				bit_clk IN (I2S)
	29		2		0		0				Sdata_in (I2S)
	30		1		1		0				Sdata_out (I2S)
	31		1		1		0				sync (I2S)
	32		0		1		0				controls memory stick ip somehow (power?)
	33		2		1		0				nCS5
	34		1		0		0				FFRXD
	35		1		0		0				FFCTS
	36		0		1		0		
	37		0		1		0		
	38		0		1		0		
	39		2		1		0				FFTXD
	40		2		1		0				FFDTR
	41		2		1		0				FFRTS
	42		0		0		0		
	43		0		0		0		
	44		0		0		0		
	45		0		0		0		
	46		0		1		0		
	47		0		1		0		
	48		0		1		0		
	49		2		1		0				nPWE (mem ctrlr)
	50		0		1		0		
	51		0		1		0		
	52		0		1		0		
	53		0		1		0		
	54		0		1		0		
	55		0		1		0		
	56		0		1		0		
	57		0		1		0		
	58		2		1		0				LDD[0] (LCD DATA)
	59		2		1		0				LDD[1] (LCD DATA)
	60		2		1		0				LDD[2] (LCD DATA)
	61		2		1		0				LDD[3] (LCD DATA)
	62		2		1		0				LDD[4] (LCD DATA)
	63		2		1		0				LDD[5] (LCD DATA)
	64		2		1		0				LDD[6] (LCD DATA)
	65		2		1		0				LDD[7] (LCD DATA)
	66		2		1		0				LDD[8] (LCD DATA)
	67		2		1		0				LDD[9] (LCD DATA)
	68		2		1		0				LDD[10] (LCD DATA)
	69		2		1		0				LDD[11] (LCD DATA)
	70		2		1		0				LDD[12] (LCD DATA)
	71		2		1		0				LDD[13] (LCD DATA)
	72		2		1		0				LDD[14] (LCD DATA)
	73		2		1		0				LDD[15] (LCD DATA)
	74		2		1		0				FCLK (LCD)
	75		2		1		0				LCLK (LCD)
	76		2		1		0				PCLK (LCD)
	77		0		1		0		
	78		2		1		0				nCS2
	79		2		1		0				nCS3
	80		2		1		0				nCS4


	UC GPIOS:
	
	GPIO	DIR		VAL		RE/FE	EIRQ	NOTES
	A0		0		1
	A1		0		0
	A2		0		1
	A3		0		1
	A4		0		1		rf		yes		hold slider (active low when in hold mode)
	A5		0		1
	A6		1		0						possibly bt chip power
	A7		0		0
	A8		0		1		r		yes
	A9		1		0
	A10		1		0						keyboard backlight (active low)
	A11		1		0
	A12		1		0
	A13		0		1		rf		yes		cradle detect (low when charging)
	A14		0		1						charge detect (low when in cradle)
	A15		0		1		rf		yes		headphone detect (low when inserted)

	B0		1		0						touch chip power (audio goes wrong when we toggle this)
	B1		1		0						audio chip power (goes wrong when toggled)
	B2		1		1						
	B3		1		1
	B4		0		0
	B5		1		1
	B6		1		1
	B7		1		1
	B8		1		0
	B9		1		1						lcd power
	B10		1		0
	B11		1		1						backglight power (active high)
	B12		1		0						power led (green led in power button) (active low)
	B13		0		1						attention LED (red led in power button) (active low)
	B14		0		0						BT LED ? (code says yes, but no effect)
	B15		1		1						rec led ? (code says yes, but no effect)

	C0		x		x		rf		yes		keypad colum read
	C1		x		x		rf		yes		keypad colum read
	C2		x		x		rf		yes		keypad colum read
	C3		x		x		rf		yes		keypad colum read
	C4		x		x		rf		yes		keypad colum read
	C5		x		x		rf		yes		keypad colum read
	C6		x		x		rf		yes		keypad colum read
	C7		x		x		rf		yes		keypad colum read
	C8		0		x						keypad row drivers
	C9		0		x						keypad row drivers
	C10		0		x						keypad row drivers
	C11		0		x						keypad row drivers
	C12		0		x						keypad row drivers
	C13		0		x						keypad row drivers
	C14		0		x						keypad row drivers
	C15		0		x						keypad row drivers

	D0		0		0						\
	D1		0		0						 | device revision
	D2		0		1						 | ID straps here
	D3		0		0						/
	
	keyboad matrix is on uc's portC
	low 8 bits are row drivers (out). drive low to drive a row
	high 8 bits are colomn reads. read low if key is down
	
	//as per american TG50 the map is:
	
	
	ROW/COL		0		1		2		3		4		5		6		7
	0			power	hard1	hard2	?		hard3	?		hard4	voice_rec
	1			pgUp	pgDn	home	jobBk	?		jogSel	grafiti	?
	2			q		caps	w		e		r		t		h		?
	3			y		tab		u		i		o		p		j		?
	4			a		shift	s		d		f		g		k		?
	5			z		ctrl	x		c		v		b		l		?
	6			n		blue	m		comma	period	up		down	?
	7			space	red		_@		left	right	enter	bksp	?
	
*/

static struct MemoryStickController *mMsc;
static struct ArmRam *mUnknownSram;
static struct An32502A *mAn32502A;
static struct Ad7873 *mAd7873;
static struct AK4534 *mAk4534;
static struct TG50uc *mUc;

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
	return 17UL << 20;	//it probes over 32M so we say 17M here so that our mirror covers the probe
}

uint_fast8_t deviceGetSocRev(void)
{
	return 0;	//PXA25x
}

void deviceSetup(struct SocPeriphs *sp, struct Keypad *kp, struct VSD *vsd, FILE* nandFile)
{
	static uint32_t keyMap[] = {
		SDLK_ESCAPE, SDLK_F1, SDLK_F2, 0, SDLK_F3, 0, SDLK_F4, SDLK_F5,
		SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_HOME, SDLK_END /* jog back */, 0, SDLK_F11/* jog sel */, SDLK_BACKQUOTE /* grafiti icon */, 0,
		SDLK_q, SDLK_CAPSLOCK, SDLK_w, SDLK_e, SDLK_r, SDLK_t, SDLK_h, 0,
		SDLK_y, SDLK_TAB, SDLK_u, SDLK_i, SDLK_o, SDLK_p, SDLK_j, 0,
		SDLK_a, SDLK_LSHIFT, SDLK_s, SDLK_d, SDLK_f, SDLK_g, SDLK_k, 0,
		SDLK_z, SDLK_LCTRL, SDLK_x, SDLK_c, SDLK_v, SDLK_b, SDLK_l, 0,
		SDLK_n, SDLK_F7 /* blue shift key */, SDLK_m, SDLK_COMMA, SDLK_PERIOD, SDLK_UP, SDLK_DOWN, 0,
		SDLK_SPACE, SDLK_F9 /* red shift key */, SDLK_UNDERSCORE, SDLK_LEFT, SDLK_RIGHT, SDLK_RETURN, SDLK_BACKSPACE, 0,
	};
	
	//gpio24 is AD7873 chip select (active low) in gpio mode (or SSPFRM mode)
	mAd7873 = ad7873Init(sp->ssp, sp->gpio, 9);
	if (!mAd7873)
		ERR("Cannot init AD7873");
	
	mAk4534 = ak4534Init(sp->i2c, sp->i2s, sp->gpio);
	if (!mAk4534)
		ERR("Cannot init AK4534");
	
	mAn32502A = an32502aInit(sp->i2c);
	if (!mAn32502A)
		ERR("Cannot init AN32502A");
	
	mUc = tg50ucInit(sp->mem, sp->gpio, 14, keyMap);
	if (!mUc)
		ERR("Cannot init TG50's UC");
	
	mMsc = msCtrlrInit(sp->mem, 0x14000000ul);
	if (!mMsc)
		ERR("Cannot init MSC");
	
	mUnknownSram = ramInit(sp->mem, 0xac000000ul, 1024, (uint32_t*)malloc(1024));
	if (!mUnknownSram)
		ERR("Cannot init RAM4");
	
	if (!keypadAddGpioKey(kp, SDLK_ESCAPE, 0, false))
		ERR("Cannot init power key\n");
	
	socGpioSetState(sp->gpio, 1, true);	//tg50 reset button (active low)
	
	socGpioSetState(sp->gpio, 3, true);	//cradle button not pressed
	
	socGpioSetState(sp->gpio, 7, true);	//tg50 MS not inserted
	socGpioSetState(sp->gpio, 10, true);	//tg50 MS IP not interrupting
	socGpioSetState(sp->gpio, 13, true);	//tg50 UC not interrupting
	
	ad7873setVbatt(mAd7873, 4200);			//battery is full
	
	sp->dbgUart = sp->uarts[2];	//FFUART
}

void devicePeriodic(uint32_t cycles)
{
	if (!(cycles & 0x00007FFFUL))
		ad7873Periodic(mAd7873);
}

void deviceTouch(int x, int y)
{
	ad7873PenInput(mAd7873, (x >= 0 && y >= 0) ? 3200 - 10 * x : -1, (x >= 0 && y >= 0) ? 3200 - 10 * y : -1);
}

void deviceKey(uint32_t key, bool down)
{
	tg50ucSetKeyPressed(mUc, key, down);
}