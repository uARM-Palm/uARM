//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_LCD.h"
#include "omap_IC.h"
#include "SDL2/SDL.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_LCD_BASE			0xFFFEC000UL
#define OMAP_LCD_SIZE			256

#define OMAP_LCD_DMA_BASE		0xFFFEDB00UL	///part of DMA controller but we drive it locally
#define OMAP_LCD_DMA_SIZE		0x12

enum OmapLcdState {
	OmapLcdOff,
	OmapLcdFetchingPalette,
	OmapLcdFetchingData,
	OmapLcdResting,
};

struct OmapLcd {
	SDL_Surface *mScreen;
	SDL_Window *mWindow;
	struct ArmMem *mem;
	struct SocIc *ic;
	
	//dma configs
	uint32_t dmaStart[2], dmaEnd[2];
	uint8_t dmaCtrl;
	bool hardGrafArea;

	//lcd configs
	uint8_t status, curDepth;
	uint32_t ctrl, timing[3], subpanel;
	
	//state
	enum OmapLcdState state;
	uint8_t curEna		: 1;
	uint8_t whichFrame	: 1; // which dma frame
	uint8_t fetchData	: 1;
	uint16_t pal[256];
	uint32_t curAddr;
};

static void omapLcdPrvIrqsRecalc(struct OmapLcd *lcd)
{
	bool irq;
	
	//LCD irq
	irq = false;
	if ((lcd->status & 0x01) && (lcd->ctrl & 0x08))				//done
		irq = true;
	else if ((lcd->status & 0x40) && (lcd->ctrl & 0x10))		//pallete loaded
		irq = true;
	socIcInt(lcd->ic, OMAP_I_LCD, irq);
	
	//LCD DMA irq
	irq = false;
	if ((lcd->dmaCtrl & 0x40) && (lcd->ctrl & 0x04))			//bus error
		irq = true;
	else if ((lcd->dmaCtrl & 0x30) && (lcd->ctrl & 0x02))		//frame done
		irq = true;
	socIcInt(lcd->ic, OMAP_I_DMA_CH_LCD, irq);
}

static uint16_t* omapLcdPrvGetFb(struct OmapLcd *lcd)
{
	uint32_t w = (lcd->timing[0] & 0x3ff) + 1;
	uint32_t h = (lcd->timing[1] & 0x3ff) + 1;
	
	if (!lcd->mWindow) {
		
		uint32_t winH = h;
		
		if (lcd->hardGrafArea && w == h)
			winH += 3 * w / 8;
		
		fprintf(stderr, "SCREEN configured for %u x %u\n", (unsigned)w, (unsigned)h);
		lcd->mWindow = SDL_CreateWindow("uARM", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, winH, 0);
		if (lcd->mWindow == NULL)
			ERR("Couldn't create window: %s\n", SDL_GetError());
		
		lcd->mScreen = SDL_CreateRGBSurface(0, w, h, 16, 0xf800, 0x07e0, 0x001f, 0x0000);
		if (lcd->mScreen == NULL)
			ERR("Couldn't create screen surface: %s\n", SDL_GetError());
	}
	
	SDL_LockSurface(lcd->mScreen);
	return (uint16_t*)lcd->mScreen->pixels;
}

static void omapLcdPrvReleaseFb(struct OmapLcd *lcd)
{
	SDL_UnlockSurface(lcd->mScreen);
	SDL_BlitSurface(lcd->mScreen, NULL, SDL_GetWindowSurface(lcd->mWindow), NULL);
	SDL_UpdateWindowSurface(lcd->mWindow);
}

static bool omapLcdPrvReadWord(struct OmapLcd *lcd, uint32_t addr, uint16_t *wordP)
{
	if (!memAccess(lcd->mem, addr, sizeof(uint16_t), false, wordP)) {
		lcd->dmaCtrl |= 0x20;
		lcd->state = OmapLcdOff;
		lcd->status |= 0x01;
		omapLcdPrvIrqsRecalc(lcd);
		return false;
	}
	
	return true;
}

void omapLcdPeriodic(struct OmapLcd *lcd)
{
	uint32_t i, num, addr = lcd->curAddr, r, c, v;
	uint32_t w = (lcd->timing[0] & 0x3ff) + 1;
	uint32_t h = (lcd->timing[1] & 0x3ff) + 1;
	uint_fast8_t j;
	uint16_t *dst;
	
	switch (lcd->state) {			//on and doing things
		
		case OmapLcdOff:
			if (lcd->ctrl & 1) 		//just got enabled
				lcd->state = OmapLcdResting;
			break;
		
		case OmapLcdResting:
			if (!(lcd->ctrl & 1)){	//just got disabled
				lcd->state = OmapLcdOff;
				lcd->status |= 0x01;
			}
			else {					//next frame plz
				
				lcd->status &=~ 0x41;	//clear: pal loaded, done
				if (lcd->dmaCtrl & 0x0001)
					lcd->whichFrame = 1 - lcd->whichFrame;
				else
					lcd->whichFrame = 0;
				addr = lcd->dmaStart[lcd->whichFrame];
				lcd->fetchData = (((lcd->ctrl >> 20) & 3) != 1);
				lcd->state = (((lcd->ctrl >> 20) & 3) == 2) ? OmapLcdFetchingData : OmapLcdFetchingPalette;
			}
			break;
		
		case OmapLcdFetchingPalette:
			
			num = 16;
			for (i = 0; i < num; i++, addr += sizeof(uint16_t)) {
				
				if (!omapLcdPrvReadWord(lcd, addr, lcd->pal + i))
					return;
				
				//we need to get the first entry to see how many more we'll have
				if (!i)  {
					lcd->curDepth = (lcd->pal[0] >> 12) & 7;
				
					//only 8bpp mode uses 256 CLUT entries	, lal other modes use 16 entries
					if (lcd->curDepth == 3)
						num = 256;
				}
			}
			
			if (lcd->ctrl & 2) {	//monochrome palette needs extra work
				static const uint8_t expandTo6[] = {63, 58, 54, 49, 45, 40, 36, 31, 27, 22, 18, 13, 9, 4, 0, 0};
				static const uint8_t expandTo5[] = {31, 29, 27, 24, 22, 20, 18, 15, 13, 11, 9, 7, 4, 2, 0, 0};
				
				for (i = 0; i < num; i++) {
					v = lcd->pal[i] & 0x0f;
					v = (((uint32_t)expandTo5[v]) << 11) | 
						(((uint32_t)expandTo6[v]) << 5) | 
						(((uint32_t)expandTo5[v]) << 0);
					lcd->pal[i] = v;
				}
			}
			else {				//color pal needs love too
				
				static const uint8_t expandTo6[] = {0, 4, 8, 13, 17, 21, 25, 29, 34, 38, 42, 46, 50, 55, 59, 63};
				static const uint8_t expandTo5[] = {0, 2, 4, 6, 8, 10, 12, 14, 17, 19, 21, 23, 25, 27, 29, 31};
				
				for (i = 0; i < num; i++) {
					v = lcd->pal[i];
					v = (((uint32_t)expandTo5[(v >> 8) & 0x0f]) << 11) | 
						(((uint32_t)expandTo6[(v >> 4) & 0x0f]) << 5) | 
						(((uint32_t)expandTo5[(v >> 0) & 0x0f]) << 0);
					lcd->pal[i] = v;
				}
			}
			
			lcd->status |= 0x40;	//pal loaded
			lcd->state = OmapLcdFetchingData;
			break;
		
		case OmapLcdFetchingData:
		
			num = w * h;
			dst = omapLcdPrvGetFb(lcd);
			
			switch (lcd->curDepth) {
				case 0:	//1bpp (maybe unsupported)
					num /= 2;
					//fallthrough
				case 1:	//2bpp
					num /= 2;
					//fallthrough
				case 2:	//4bpp
					num /= 2;
					//fallthrough
				case 3:	//8bpp
					num /= 2;
					break;
				
				default:
					break;
			}
			for (i = 0; i < num; i++, addr += sizeof(uint16_t)) {
				
				uint16_t data;
				
				if (!omapLcdPrvReadWord(lcd, addr, &data))
					return;
				
				switch (lcd->curDepth) {
					case 0:			//1bpp
						data = __builtin_bswap16(data);
						for (j = 0; j < 16; j++, data <<= 1)
							*dst++ = lcd->pal[data >> 15];
						break;
					
					case 1:			//2bpp
						data = __builtin_bswap16(data);
						for (j = 0; j < 16; j += 2, data <<= 2)
							*dst++ = lcd->pal[data >> 14];
						break;
					
					case 2:			//4bpp
						data = __builtin_bswap16(data);
						for (j = 0; j < 16; j += 4, data <<= 4)
							*dst++ = lcd->pal[data >> 12];
						break;
					
					case 3:			//8bpp
						*dst++ = lcd->pal[data & 0xff];
						*dst++ = lcd->pal[data >> 8];
						break;
					
					default:
						*dst++ = data;
						break;
			}
		}
		lcd->state = OmapLcdResting;
		lcd->status |= 0x01;	//done
		lcd->dmaCtrl |= 1 << (lcd->whichFrame ? 4 : 3);
		omapLcdPrvReleaseFb(lcd);
		break;
	}
	lcd->curAddr = addr;
	omapLcdPrvIrqsRecalc(lcd);
}

static bool omapLcdPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapLcd *lcd = (struct OmapLcd*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - OMAP_LCD_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch (pa) {
		case 0x00 / 4:
			if (write) {
				
				//if we were off and are now also off, we still need to set "done" flag in SR
				if (!((lcd->ctrl | val) & 1))
					lcd->status |= 0x01;
				
				lcd->ctrl = val & 0x01fff39b;
			}
			else
				val = (lcd->ctrl & 0xfffffffe) | (lcd->curEna ? 1 : 0);
			break;
		
		case 0x04 / 4:
		case 0x08 / 4:
		case 0x0c / 4:
			if (write)
				lcd->timing[pa - 0x04 / 4] = val;
			else
				val = lcd->timing[pa - 0x04 / 4];
			break;
		
		case 0x10 / 4:
			if (write) {
				lcd->status &=~ (val & 0x08);
				omapLcdPrvIrqsRecalc(lcd);
			}
			else
				val = lcd->status;
			break;
		
		case 0x14 / 4:
			if (write)
				lcd->subpanel = val & 0xa3ffffff;
			else
				val = lcd->subpanel;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

static bool omapLcdPrvDmaMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapLcd *lcd = (struct OmapLcd*)userData;
	uint_fast16_t val = 0;
	
	if (size != 2) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - OMAP_LCD_DMA_BASE) >> 1;
	
	if (write)
		val = *(uint32_t*)buf;
	
	if (!pa) {
		
		if (write)
			lcd->dmaCtrl = val & 0x7f;
		else
			val = lcd->dmaCtrl;
	}
	else if (pa < 9){
		
		uint32_t *ptr;
		bool hi;
		
		pa--;
		hi = pa & 1;
		ptr = ((pa & 2) ? lcd->dmaEnd : lcd->dmaStart) + (pa >> 2);
		
		if (write) {
			if (hi)
				*ptr = ((*ptr) & 0x0000fffful) | (((uint32_t)val) << 16);
			else
				*ptr = ((*ptr) & 0xffff0000ul) | val;
		}
		else {
			if (hi)
				val = *ptr >> 16;
			else
				val = *ptr;
		}
	}
	else
		return false;

	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct OmapLcd* omapLcdInit(struct ArmMem *physMem, struct SocIc *ic, bool hardGrafArea)
{
	struct OmapLcd *lcd = (struct OmapLcd*)malloc(sizeof(*lcd));
	
	if (!lcd)
		ERR("cannot alloc LCD");
	
	memset(lcd, 0, sizeof (*lcd));
	lcd->hardGrafArea = hardGrafArea;
	lcd->mem = physMem;
	lcd->ic = ic;
	
	if (!memRegionAdd(physMem, OMAP_LCD_BASE, OMAP_LCD_SIZE, omapLcdPrvMemAccessF, lcd))
		ERR("cannot add LCD to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_LCD_DMA_BASE, OMAP_LCD_DMA_SIZE, omapLcdPrvDmaMemAccessF, lcd))
		ERR("cannot add LCD DMA to MEM\n");
	
	omapLcdPrvIrqsRecalc(lcd);
	
	return lcd;
}
