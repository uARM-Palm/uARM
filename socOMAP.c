//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_32kTMR.h"
#include "omap_Camera.h"
#include "omap_McBSP.h"
#include "omap_Misc.h"
#include "omap_ULPD.h"
#include "omap_UART.h"
#include "omap_MMC.h"
#include "omap_DMA.h"
#include "omap_USB.h"
#include "omap_PWL.h"
#include "omap_PWT.h"
#include "omap_TMR.h"
#include "omap_WDT.h"
#include "omap_LCD.h"
#include "omap_RTC.h"
#include "omap_IC.h"

#include "soc_uWire.h"
#include "soc_GPIO.h"

#include "SoC.h"
#include "CPU.h"
#include "MMU.h"
#include "mem.h"
#include "RAM.h"
#include "ROM.h"


#include "device.h"
#include "keys.h"
#include "vSD.h"
#include <stdlib.h>
#include "SDL2/SDL.h"
#include "util.h"



#define ROM_BASE	0x00000000UL
#define RAM_BASE	0x10000000UL
#define SRAM_BASE	0x20000000UL
#define SRAM_SIZE	0x00030000UL


struct SoC {

	struct OmapMcBsp *mcbsp[3];
	struct Omap32kTmr *tmr32k;
	struct OmapCamera *cam;
	struct OmapTmr *tmr[3];
	struct OmapMisc *misc;
	struct OmapUlpd *ulpd;
	struct OmapWdt *wdt;
	struct OmapLcd *lcd;
	struct OmapRtc *rtc;
	struct OmapPwl *pwl;
	struct OmapPwt *pwt;
	struct OmapUsb *usb;
	struct OmapMmc *mmc;
	
	struct SocUart *uart1, *uart2, *uart3;
	struct SocUwire *uWire;
	struct SocGpio *gpio;
	struct SocDma *dma;
	struct SocI2c *i2c; 
	struct SocIc *ic;

	struct ArmRam *sram;
	struct ArmRam *ram;
	struct ArmRam *ramMirror;	//mirror
	struct ArmRom *rom;
	struct ArmMem *mem;
	struct ArmCpu *cpu;
	struct Keypad *kp;
	struct VSD *vSD;
};

/*
	addr				periph			sz		access
DSP shared bus 0:
	E100 0000			DSP TIPB		2K		16
	E100 8000			DSP CLKM		2K		16
DSP shared bus 1:
	E101 0000			UART1			2K		8
	E101 0800			UART2			2K		8
	E101 1800			McBSP1			2K		16
	E101 2000			MCSI2			2K		16
	E101 2800			MCSI1			2K		16
	E101 7000			McBSP3			2K		16
	E101 9800			UART3			2K		8
	E101 E000			GPIO			2K		16
MPU public bus 0:
	FFFB 0000			UART1 (bt)		2K		8
	FFFB 0800			UART2 (comms)	2K		8
	FFFB 1000			McBSP2			2K		16
	FFFB 3000			uWire			2K		16
	FFFB 3800			i2c				2K		16
	FFFB 4000			usb				2K		16
	FFFB 4800			rtc				2K		8
	FFFB 5000			MPUIO			2K		16	??
	FFFB 5800			PWL				2K		8	???
	FFFB 6000			PWT				2K		8	???
	FFFB 6800			camera IF		2K		32
	FFFB 7800			MMC				2K		16
	FFFB 9000			32khz tmr		2K		32
	FFFB 9800			UART3			2K		8
	FFFB A000			usb host		2K		32
	FFFB A800			FAC				2K		16
	FFFB C000			1-wire			2K		8
	FFFB C800			tibp swtches	2K		16	???
	FFFB D000			LED1			2K		8
	FFFB D800			LED2			2K		8
MPU public bus 1:
	FFFC E000			GPIO			2K		32	???
	FFFC F000			mailbox			2K		16	???
MPU private bus 1:
	FFFE 0000			level2 ic		2K		32
	FFFE 0800			ULPD pwrMgmnt	2K		16
	FFFE 1000			omap cfg		2K		32	???
	FFFE 1800			die id			2K		32	???
	FFFE C000			LCD ctrlr		256		32
	FFFE C100			local bus IF	256		32	???
	FFFE C200			local bus MMU	256		32	???
	FFFE C500			MPU timer 1		256		32
	FFFE C600			MPU timer 2		256		32
	FFFE C700			MPU timer 3		256		32
	FFFE C800			MPU WDT			256		32
	FFFE C900			MPUI			256		32	???
	FFFE CA00			mpupriv tibp br	256		32	???
	FFFE CB00			level1 ic		256		32
	FFFE CC00			traffic cntrlr	256		32	???
	FFFE CE00			mpu clk ctrl	256		32
	FFFE CF00			DPLL1			256		32
	FFFE D200			dsp mmu			256		32
	FFFE D300			mpu-pub tibp br	256		16
	FFFE D400			jtad id code	256		32
	FFFE D800			dma channels	768		16
	FFFE DB00			lcd dma			256		16
	FFFE DC00			dma config		256		16
	
	

*/

static uint_fast16_t socUartPrvRead(void* userData)
{
	uint_fast16_t v;
	int r;

	r = socExtSerialReadChar();
	
	if(r == CHAR_CTL_C)
		v = UART_CHAR_BREAK;
	else if(r == CHAR_NONE)
		v = UART_CHAR_NONE;
	else if(r >= 0x100)
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
	struct SoC *soc = malloc(sizeof(struct SoC));
	struct SocPeriphs sp;
	void *ramBuffer;
	uint32_t i;
	
	memset(soc, 0, sizeof(*soc));
	
	soc->mem = memInit();
	if (!soc->mem)
		ERR("Cannot init physical memory manager");

	soc->cpu = cpuInit(ROM_BASE, soc->mem, false /* xscale */, true /* omap */, gdbPort, 0x54029152, 0x0514a1da);
	if (!soc->cpu)
		ERR("Cannot init CPU");
	
	ramBuffer = malloc(SRAM_SIZE);
	if (!ramBuffer)
		ERR("cannot alloc SRAM space\n");
	
	soc->sram = ramInit(soc->mem, SRAM_BASE, SRAM_SIZE, ramBuffer);
	if(!soc->sram)
		ERR("Cannot init SRAM");
	
	ramBuffer = malloc(deviceGetRamSize());
	if (!ramBuffer)
		ERR("cannot alloc RAM space\n");
	
	soc->ram = ramInit(soc->mem, RAM_BASE, deviceGetRamSize(), ramBuffer);
	if(!soc->ram)
		ERR("Cannot init RAM");
	
	//ram mirror for rom probe code
	soc->ramMirror = ramInit(soc->mem, RAM_BASE + deviceGetRamSize(), deviceGetRamSize(), ramBuffer);
	if(!soc->ramMirror)
		ERR("Cannot init RAM mirror");
	
	soc->rom = romInit(soc->mem, ROM_BASE, romPieces, romPieceSizes, romNumPieces, deviceGetRomMemType());
	if(!soc->rom)
		ERR("Cannot init ROM");
	
	soc->ic = socIcInit(soc->cpu, soc->mem, socRev);
	if (!soc->ic)
		ERR("Cannot init OMAP's IC");
	
	soc->dma = socDmaInit(soc->mem, soc->ic);
	if (!soc->dma)
		ERR("Cannot init OMAP's DMA");
	
	soc->gpio = socGpioInit(soc->mem, soc->ic, socRev);
	if (!soc->gpio)
		ERR("Cannot init OMAP's GPIO");
	
	soc->i2c = socI2cInit(soc->mem, soc->ic, soc->dma);
	if (!soc->i2c)
		ERR("Cannot init OMAP's I2C");
	
	soc->misc = omapMiscInit(soc->mem);
	if (!soc->gpio)
		ERR("Cannot init OMAP's MISC stuff");

	soc->ulpd = omapUlpdInit(soc->mem, soc->ic);
	if (!soc->ulpd)
		ERR("Cannot init OMAP's ULPD");

	soc->wdt = omapWdtInit(soc->mem, soc->ic);
	if (!soc->wdt)
		ERR("Cannot init OMAP's WDT");
	
	soc->uart1 = socUartInit(soc->mem, soc->ic, OMAP_UART1_BASE, OMAP_I_UART1);
	if (!soc->uart1)
		ERR("Cannot init OMAP's UART1");
	
	soc->uart2 = socUartInit(soc->mem, soc->ic, OMAP_UART2_BASE, OMAP_I_UART2);
	if (!soc->uart2)
		ERR("Cannot init OMAP's UART2");
	
	soc->uart3 = socUartInit(soc->mem, soc->ic, OMAP_UART3_BASE, OMAP_I_UART3);
	if (!soc->uart3)
		ERR("Cannot init OMAP's UART3");
	
	for (i = 0; i < 3; i++) {
		
		static const uint8_t irqs[] = {OMAP_I_TIMER_1, OMAP_I_TIMER_2, OMAP_I_TIMER_3};
		
		soc->tmr[i] = omapTmrInit(soc->mem, soc->ic, soc->misc, OMAP_TMRS_BASE + OMAP_TMRS_INCREMENT * i, irqs[i]);
		if (!soc->tmr[i])
			ERR("Cannot init OMAP's TMR%u", i + 1);
	}
	
	for (i = 0; i < 3; i++) {
		
		static const uint8_t irqs[] = {OMAP_I_McBSP_1_TX, OMAP_I_McBSP_2_TX, OMAP_I_McBSP_3_TX};
		static const uint8_t dmas[] = {DMA_REQ_McBSP_1_TX, DMA_REQ_McBSP_2_TX, DMA_REQ_McBSP_3_TX};
		static const uint32_t bases[] = {0xE1011800, 0xFFFB1000, 0xE1017000};
		
		soc->mcbsp[i] = omapMcBspInit(soc->mem, soc->ic, soc->dma, bases[i], irqs[i], irqs[i] + 1, dmas[i], dmas[i] + 1);
		if (!soc->mcbsp[i])
			ERR("Cannot init OMAP's McBSP%u", i + 1);
	}

	soc->tmr32k = omap32kTmrInit(soc->mem, soc->ic);
	if (!soc->tmr32k)
		ERR("Cannot init OMAP's 32kTMR");
	
	soc->rtc = omapRtcInit(soc->mem, soc->ic);
	if (!soc->rtc)
		ERR("Cannot init OMAP's RTC");

	soc->pwl = omapPwlInit(soc->mem);
	if (!soc->pwl)
		ERR("Cannot init OMAP's PWL");

	soc->pwt = omapPwtInit(soc->mem);
	if (!soc->pwt)
		ERR("Cannot init OMAP's PWT");

	soc->lcd = omapLcdInit(soc->mem, soc->ic, deviceHasGrafArea());
	if (!soc->lcd)
		ERR("Cannot init OMAP's LCD");

	soc->usb = omapUsbInit(soc->mem, soc->ic, soc->dma);
	if (!soc->usb)
		ERR("Cannot init OMAP's USB");

	soc->mmc = omapMmcInit(soc->mem, soc->ic, soc->dma);
	if (!soc->mmc)
		ERR("Cannot init OMAP's MMC");

	soc->cam = omapCameraInit(soc->mem, soc->ic, soc->dma);
	if (!soc->cam)
		ERR("Cannot init OMAP's CameraIF");

	soc->uWire = socUwireInit(soc->mem, soc->ic, soc->dma);
	if (!soc->uWire)
		ERR("Cannot init OMAP's uWire");

	soc->kp = keypadInit(soc->gpio, true);
	if (!soc->kp)
		ERR("Cannot init keypad controller");
	
	if (sdNumSectors) {
		
		soc->vSD = vsdInit(sdR, sdW, sdNumSectors);
		if (!soc->vSD)
			ERR("Cannot init vSD");
		
		omapMmcInsert(soc->mmc, soc->vSD);
		fprintf(stderr, "SD inited\n");
	}
	
	sp.mem = soc->mem;
	sp.gpio = soc->gpio;
	sp.uw = soc->uWire;
	sp.i2c = soc->i2c;
	deviceSetup(&sp, soc->kp, soc->vSD, nandFile);
	
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);
	
	return soc;
}

void socRun(struct SoC* soc)
{
	uint32_t cycles = 0;
	uint_fast8_t i;
	
	while (1)
	{	
		cycles++;
		
		if (!(cycles & 0x000003FFUL))
			socDmaPeriodic(soc->dma);
		if (!(cycles & 0x000003FFUL))
			socUwirePeriodic(soc->uWire);
		if (!(cycles & 0x000003FFFUL)) {
			for (i = 0; i < 3; i++)
				omapMcBspPeriodic(soc->mcbsp[i]);
		}
		for (i = 0; i < 3; i++)
			omapTmrPeriodic(soc->tmr[i]);
		if (!(cycles & 0x00000FFFUL))
			omapWdtPeriodic(soc->wdt);
		if (!(cycles & 0x00000fFFUL))	//4x fatser than normal - for speed
			omap32kTmrPeriodic(soc->tmr32k);
		if (!(cycles & 0x000FFFFFUL))
			omapLcdPeriodic(soc->lcd);
		if (!(cycles & 0x000FFFFFUL)) 
			omapRtcPeriodic(soc->rtc);
		if (!(cycles & 0x000000FFUL)) {
			socUartProcess(soc->uart1);
			socUartProcess(soc->uart2);
			socUartProcess(soc->uart3);
		}
		devicePeriodic(cycles);
	
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
					deviceTouch(event.button.x, event.button.y);
					break;
				
				case SDL_MOUSEBUTTONUP:
					if (event.button.button != SDL_BUTTON_LEFT)
						break;
					mouseDown = false;
					deviceTouch(-1, -1);
					break;
				
				case SDL_MOUSEMOTION:
					if (!mouseDown)
						break;
					deviceTouch(event.motion.x, event.motion.y);
					break;
				
				case SDL_KEYDOWN:
				
					deviceKey(event.key.keysym.sym, true);
					keypadSdlKeyEvt(soc->kp, event.key.keysym.sym, true);
					break;
				
				case SDL_KEYUP:
				
					deviceKey(event.key.keysym.sym, false);
					keypadSdlKeyEvt(soc->kp, event.key.keysym.sym, false);
					break;
			}
		}
		
		cpuCycle(soc->cpu);
	}
}


