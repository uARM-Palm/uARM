//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "soc_GPIO.h"

#include "s3c24xx_MemCtrl.h"
#include "s3c24xx_PwrClk.h"
#include "s3c24xx_SDIO.h"
#include "s3c24xx_UART.h"
#include "s3c24xx_NAND.h"
#include "s3c24xx_ADC.h"
#include "s3c24xx_RTC.h"
#include "s3c24xx_USB.h"
#include "s3c24xx_TMR.h"
#include "s3c24xx_WDT.h"
#include "s3c24xx_LCD.h"
#include "s3c24xx_IC.h"

#include "nand.h"
#include "SoC.h"
#include "CPU.h"
#include "MMU.h"
#include "mem.h"
#include "RAM.h"
#include "ROM.h"


#include "device.h"
#include "keys.h"
#include "vSD.h"
#include <string.h>
#include <stdlib.h>
#include "SDL2/SDL.h"
#include "util.h"



#define ROM_BASE	0x00000000UL

#define RAM_BASE	0x30000000UL
#define SRAM_SIZE	0x00001000UL


struct SoC {
	
	struct S3C24xxMemCtrl *memCtrl;
	struct S3C24xxTimers *timers;
	struct S3C24xxPwrClk *pwrClk;
	struct S3C24xxSdio *sdio;
	struct S3C24xxNand *nand;
	struct S3C24xxRtc *rtc;
	struct S3C24xxAdc *adc;
	struct S3C24xxUsb *usb;
	struct S3C24xxWdt *wdt;
	struct S3C24xxLcd *lcd;
	bool soc40;

	
	struct SocUart *uart0, *uart1, *uart2;
	struct SocGpio *gpio;
	struct SocDma *dma;
	struct SocI2c *i2c; 
	struct SocIc *ic;

	uint8_t *sramBuffer;
	struct ArmRam *sram;
	struct ArmRam *ram;
	struct ArmRam *ramMirror;	//mirror
	struct ArmRom *rom;
	struct ArmMem *mem;
	struct ArmCpu *cpu;
	struct Keypad *kp;
	struct VSD *vSD;
	
	struct Device *dev;
};

/*
	addr		 len		periph

	48000000	0x34		memory control
	49000000	0x5c		usb host
	4a000000	0x20		interrupt controller
	4b000000	0xe4		DMA
	4c000000	0x18		clocks and power
	4d000000	0x64		LCD
	4e000000	0x18		NAND
	50000000	0x2c		UART0
	50004000	0x2c		UART1
	50008000	0x2c		UART2
	51000000	0x44		PWM/timer
	52000140	0x130		USB (byte accessible)
	53000000	0x0c		WDT
	54000000	0x10		i2c
	55000000	0x14		i2s (halfword accessible)
	56000000	0xb4		gpio, input clocks, external irqs, general statys
	57000040	0x48		RTC (byte accessible)
	58000000	0x14		ADC
	59000000	0x18		SPI0
	59000020	0x18		SPI1
	5a000000	0x44		SDIO
*/

static uint_fast16_t socUartPrvRead(void* userData)
{
	uint_fast16_t v;
	int r;

	r = socExtSerialReadChar();
	
	if (r == CHAR_CTL_C)
		v = UART_CHAR_BREAK;
	else if (r == CHAR_NONE)
		v = UART_CHAR_NONE;
	else if (r >= 0x100)
		v = UART_CHAR_NONE;		//we canot send this char!!!
	else
		v = r;
	
	return v;
}

static void socUartPrvWrite(uint_fast16_t chr, void* userData)
{
	if (chr == UART_CHAR_NONE)
		return;

	socExtSerialWriteChar(chr);
}

struct SoC* socInit(void **romPieces, const uint32_t *romPieceSizes, uint32_t romNumPieces, uint32_t sdNumSectors, SdSectorR sdR, SdSectorW sdW, FILE *nandFile, int gdbPort, uint_fast8_t socRev)
{
	struct SoC *soc = (struct SoC*)malloc(sizeof(struct SoC));
	struct SocPeriphs sp;
	uint32_t *ramBuffer;
	uint32_t i;
	
	memset(soc, 0, sizeof(*soc));
	soc->soc40 = !!socRev;
	
	soc->mem = memInit();
	if (!soc->mem)
		ERR("Cannot init physical memory manager");

	soc->cpu = cpuInit(0, soc->mem, false /* xscale */, false /* omap */, gdbPort, 0x41129200ul, 0x0d172172ul);
	if (!soc->cpu)
		ERR("Cannot init CPU");
	
	if (romNumPieces) {
		soc->rom = romInit(soc->mem, ROM_BASE, romPieces, romPieceSizes, romNumPieces, deviceGetRomMemType());
		if (!soc->rom)
			ERR("Cannot init ROM");
	}
	
	soc->sramBuffer = (uint8_t*)malloc(SRAM_SIZE);
	if (!soc->sramBuffer)
		ERR("cannot alloc SRAM space\n");
	
	soc->sram = ramInit(soc->mem, romNumPieces ? 0x40000000ul : 0x00000000ul, SRAM_SIZE, (uint32_t*)soc->sramBuffer);
	if (!soc->sram)
		ERR("Cannot init SRAM");
	
	ramBuffer = (uint32_t*)malloc(deviceGetRamSize());
	if (!ramBuffer)
		ERR("cannot alloc RAM space\n");
	
	soc->ram = ramInit(soc->mem, RAM_BASE, deviceGetRamSize(), ramBuffer);
	if (!soc->ram)
		ERR("Cannot init RAM");
	
	//ram mirror for rom probe code
	soc->ramMirror = ramInit(soc->mem, RAM_BASE + deviceGetRamSize(), deviceGetRamSize(), ramBuffer);
	if (!soc->ramMirror)
		ERR("Cannot init RAM mirror");
	
	soc->ic = socIcInit(soc->cpu, soc->mem, socRev);
	if (!soc->ic)
		ERR("Cannot init S3C24xx's IC");
	
	//soc->dma = socDmaInit(soc->mem, soc->ic);
	//if (!soc->dma)
	//	ERR("Cannot init S3C24xx's DMA");
	
	soc->gpio = socGpioInit(soc->mem, soc->ic, socRev);
	if (!soc->gpio)
		ERR("Cannot init S3C24xx's GPIO");
	
	//soc->i2c = socI2cInit(soc->mem, soc->ic, soc->dma);
	//if (!soc->i2c)
	//	ERR("Cannot init S3C24xx's I2C");
	
	soc->uart0 = socUartInit(soc->mem, soc->ic, S3C24XX_UART0_BASE, S3C24XX_I_UART2_ERR);
	if (!soc->uart0)
		ERR("Cannot init S3C24xx's UART0");
	
	soc->uart1 = socUartInit(soc->mem, soc->ic, S3C24XX_UART1_BASE, S3C24XX_I_UART2_ERR);
	if (!soc->uart1)
		ERR("Cannot init S3C24xx's UART1");
	
	soc->uart2 = socUartInit(soc->mem, soc->ic, S3C24XX_UART2_BASE, S3C24XX_I_UART2_ERR);
	if (!soc->uart2)
		ERR("Cannot init S3C24xx's UART2");
	
	soc->timers = s3c24xxTimersInit(soc->mem, soc->ic, soc->dma);
	if (!soc->timers)
		ERR("Cannot init S3C24xx's Timers");
	
	soc->lcd = s3c24xxLcdInit(soc->mem, soc->ic, deviceHasGrafArea());
	if (!soc->lcd)
		ERR("Cannot init S3C24xx's LCD unit");
	
	soc->adc = s3c24xxAdcInit(soc->mem, soc->ic);
	if (!soc->adc)
		ERR("Cannot init S3C24xx's ADC unit");
	
	soc->rtc = s3c24xxRtcInit(soc->mem, soc->ic);
	if (!soc->rtc)
		ERR("Cannot init S3C24xx's RTC unit");
	
	soc->wdt = s3c24xxWdtInit(soc->mem, soc->ic, socRev);
	if (!soc->wdt)
		ERR("Cannot init S3C24xx's WDT");
	
	soc->pwrClk = s3c24xxPwrClkInit(soc->mem);
	if (!soc->pwrClk)
		ERR("Cannot init S3C24xx's Clocks & Power Manager");
	
	soc->memCtrl = s3c24xxMemCtrlInit(soc->mem);
	if (!soc->memCtrl)
		ERR("Cannot init S3C24xx's Memory Controller");
	
	soc->kp = keypadInit(soc->gpio, true);
	if (!soc->kp)
		ERR("Cannot init keypad controller");
	
	soc->usb = s3c24xxUsbInit(soc->mem, soc->ic, soc->dma);
	if (!soc->usb)
		ERR("Cannot init S3C24xx's USB unit");
	
	soc->sdio = s3c24xxSdioInit(soc->mem, soc->ic, soc->dma, socRev);
	if (!soc->sdio)
		ERR("Cannot init S3C24xx's SDIO unit");
	
	sp.mem = soc->mem;
	sp.gpio = soc->gpio;
	sp.i2c = soc->i2c;
	sp.soc = soc;
	sp.adc = soc->adc;
	soc->dev = deviceSetup(&sp, soc->kp, soc->vSD, nandFile);
	if (!soc->dev)
		ERR("Cannot init device\n");
	
	soc->nand = s3c24xxNandInit(soc->mem, sp.nand, soc->ic, soc->gpio);
	if (!soc->nand)
		ERR("Cannot init S3C24xx's NAND unit");
	
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);
	
	return soc;
}

static void socNandWait(struct SoC* soc, struct NAND *nand)
{
	uint_fast8_t i;
	
	//wait for busy signal
	for (i = 0; i < 10; i++)
		nandPeriodic(nand);

	//wait for ready signal
	while (!nandIsReady(nand))
		nandPeriodic(nand);
}

void socBootload(struct SoC* soc, uint32_t nandPageSzAndFlags, void *param)
{
	uint32_t i, j, nandPageSz = nandPageSzAndFlags & 0xffffful;
	bool extraColAddr = !!(nandPageSzAndFlags & 0x80000000ul);
	bool extraReadCmd = !!(nandPageSzAndFlags & 0x40000000ul);
	struct NAND *nand = (struct NAND*)param;
	
	if (soc->rom)
		ERR("cannot bootload with ROM present\n");
	
	//read is without ecc
	for (i = 0; i < SRAM_SIZE / nandPageSz; i++) {
		
		socNandWait(soc, nand);
		
		if (!nandWrite(nand, true, false, 0x00))
			ERR("cannot tell NAND to read\n");
		
		//column address
		if (!nandWrite(nand, false, true, 0))
			ERR("cannot tell NAND col addr byte 1\n");
		
		if (extraColAddr) {
			if (!nandWrite(nand, false, true, 0))
				ERR("cannot tell NAND col addr byte 2\n");
		}
		
		if (!nandWrite(nand, false, true, i))
			ERR("cannot tell NAND row addr byte 1\n");
		
		if (!nandWrite(nand, false, true, i >> 8))
			ERR("cannot tell NAND row addr byte 2\n");
		
		if (!nandWrite(nand, false, true, i >> 16))
			ERR("cannot tell NAND row addr byte 3\n");

		if (extraReadCmd) {
			if (!nandWrite(nand, true, false, 0x30))
				ERR("cannot tell NAND to read again\n");
		}

		socNandWait(soc, nand);

		for (j = 0; j < nandPageSz; j++) {
			if (!nandRead(nand, false, false, soc->sramBuffer + i * nandPageSz + j))
				ERR("cannot read nand page %u byte %u\n", i, j);
		}
	}
}


void socRun(struct SoC* soc)
{
	uint32_t cycles = 0;
	uint_fast8_t i;
	
	while (1)
	{	
		cycles++;
		
		if (!(cycles & 0x00000001UL)) {
			s3c24xxWdtPeriodic(soc->wdt);
			s3c24xxTimersPeriodic(soc->timers);
		}
		if (!(cycles & 0x000000FFUL))
			s3c24xxNandPeriodic(soc->nand);
		
		if (!(cycles & 0x00000FFFUL))
			s3c24xxLcdPeriodic(soc->lcd);
		
		if (!(cycles & 0x0000000FUL))
			s3c24xxAdcPeriodic(soc->adc);

		if (!(cycles & 0x00000FFFUL))
			s3c24xxRtcPeriodic(soc->rtc);
		
		//if (!(cycles & 0x000003FFUL))
		//	socDmaPeriodic(soc->dma);
		if (!(cycles & 0x000000FFUL)) {
			socUartProcess(soc->uart0);
			socUartProcess(soc->uart1);
			socUartProcess(soc->uart2);
		}
		devicePeriodic(soc->dev, cycles);
	
		if (!(cycles & 0x00FFFFUL)) {
			
			static bool mouseDown = false;
			SDL_Event event;
			
			if (SDL_PollEvent(&event)) switch (event.type) {
				
				case SDL_QUIT:
					fprintf(stderr, "quit reqested\n");
					exit(0);
					break;
				
				case SDL_MOUSEBUTTONDOWN:
					if (event.button.button != SDL_BUTTON_LEFT)
						break;
					mouseDown = true;
					deviceTouch(soc->dev, event.button.x, event.button.y);
					break;
				
				case SDL_MOUSEBUTTONUP:
					if (event.button.button != SDL_BUTTON_LEFT)
						break;
					mouseDown = false;
					deviceTouch(soc->dev, -1, -1);
					break;
				
				case SDL_MOUSEMOTION:
					if (!mouseDown)
						break;
					deviceTouch(soc->dev, event.motion.x, event.motion.y);
					break;
				
				case SDL_KEYDOWN:
				
					deviceKey(soc->dev, event.key.keysym.sym, true);
					keypadSdlKeyEvt(soc->kp, event.key.keysym.sym, true);
					break;
				
				case SDL_KEYUP:
				
					deviceKey(soc->dev, event.key.keysym.sym, false);
					keypadSdlKeyEvt(soc->kp, event.key.keysym.sym, false);
					break;
			}
		}
		
		cpuCycle(soc->cpu);
	}
}


