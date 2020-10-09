//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_LCD.h"
#include "s3c24xx_IC.h"
#include "SDL2/SDL.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define S3C24XX_LCD_BASE			0x4d000000UL
#define S3C24XX_LCD_SIZE			0x800


struct S3C24xxLcd {

	SDL_Surface *mScreen;
	SDL_Window *mWindow;
	struct ArmMem *mem;
	struct SocIc *ic;
	
	uint32_t lcdcon1, lcdcon2, lcdcon3, lcdcon5, lcdsaddr1, lcdsaddr2, lcdsaddr3, redlut, greenlut, dithmode, tpal;
	uint16_t lcdcon4, bluelut, pal[256];
	uint8_t lcdintpnd, lcdsrcpnd, lcdintmsk, lpcsel;
	bool hardGrafArea;
};


static void s3c24xxLcdPrvUpdateInts(struct S3C24xxLcd *lcd)
{
	lcd->lcdintpnd |= lcd->lcdsrcpnd &~ lcd->lcdintmsk;
	
	socIcInt(lcd->ic, S3C24XX_I_LCD, !!lcd->lcdintpnd);
}

static bool s3c24xxLcdPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxLcd *lcd = (struct S3C24xxLcd*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa - S3C24XX_LCD_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	if (pa >= 0x400 / 4) {
		if (write)
			lcd->pal[pa - 0x400 / 4] = val;
		else
			val = lcd->pal[pa - 0x400 / 4];
	}
	else switch (pa) {
		
		case 0x00 / 4:
			if (write)
				lcd->lcdcon1 = (lcd->lcdcon1 & 0x0ffc0000ul) | (val & 0x0003fffful);
			else
				val = lcd->lcdcon1;
			break;
		
		case 0x04 / 4:
			if (write)
				lcd->lcdcon2 = val;
			else
				val = lcd->lcdcon2;
			break;
		
		case 0x08 / 4:
			if (write)
				lcd->lcdcon3 = val;
			else
				val = lcd->lcdcon3;
			break;
		
		case 0x0c / 4:
			if (write)
				lcd->lcdcon4 = val;
			else
				val = lcd->lcdcon4;
			break;
		
		case 0x10 / 4:
			if (write)
				lcd->lcdcon5 = (lcd->lcdcon5 & 0x0001f000ul) | (val & 0x00000ffful);
			else
				val = lcd->lcdcon4;
			break;
		
		case 0x14 / 4:
			if (write)
				lcd->lcdsaddr1 = val & 0x3ffffffful;
			else
				val = lcd->lcdsaddr1;
			break;
		
		case 0x18 / 4:
			if (write)
				lcd->lcdsaddr2 = val & 0x001ffffful;
			else
				val = lcd->lcdsaddr2;
			break;
		
		case 0x1c / 4:
			if (write)
				lcd->lcdsaddr3 = val & 0x003ffffful;
			else
				val = lcd->lcdsaddr3;
			break;
		
		case 0x20 / 4:
			if (write)
				lcd->redlut = val;
			else
				val = lcd->redlut;
			break;
		
		case 0x24 / 4:
			if (write)
				lcd->greenlut = val;
			else
				val = lcd->greenlut;
			break;
		
		case 0x28 / 4:
			if (write)
				lcd->bluelut = val;
			else
				val = lcd->bluelut;
			break;
		
		case 0x4c / 4:
			if (write)
				lcd->dithmode = val & 0x0007fffful;
			else
				val = lcd->dithmode;
			break;
		
		case 0x50 / 4:
			if (write)
				lcd->tpal = val & 0x01fffffful;
			else
				val = lcd->tpal;
			break;
		
		case 0x54 / 4:
			if (write) {
				lcd->lcdintpnd &=~ (val & 0x03);		//XXX: is this accurate?
				s3c24xxLcdPrvUpdateInts(lcd);
			}
			else
				val = lcd->lcdintpnd;
			break;
		
		case 0x58 / 4:
			if (write) {
				lcd->lcdsrcpnd &=~ (val & 0x03);		//XXX: is this accurate?
				s3c24xxLcdPrvUpdateInts(lcd);
			}
			else
				val = lcd->lcdsrcpnd;
			break;
		
		case 0x5c / 4:
			if (write) {
				lcd->lcdintmsk = val & 0x07;
				s3c24xxLcdPrvUpdateInts(lcd);
			}
			else
				val = lcd->lcdintmsk;
			break;
		
		case 0x60 / 4:
			if (write)
				lcd->lpcsel = (val & 0x03) | 0x04;
			else
				val = lcd->lpcsel;
			break;
		
		case 0x68 / 4:
			if (write)
				return false;
			else
				val = 0;	//not documented but Z22 bootlaoder expects zero here
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

//XXX: implement TPAL

static uint16_t* s3c24xxLcdPrvGetFb(struct S3C24xxLcd *lcd, uint32_t w, uint32_t h)
{
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

static void s3c24xxLcdPrvReleaseFb(struct S3C24xxLcd *lcd)
{
	SDL_UnlockSurface(lcd->mScreen);
	SDL_BlitSurface(lcd->mScreen, NULL, SDL_GetWindowSurface(lcd->mWindow), NULL);
	SDL_UpdateWindowSurface(lcd->mWindow);
}

static uint32_t s3c24xxLcdPrvReadWord(struct S3C24xxLcd *lcd, uint32_t addr)	//swaps as needed
{
	uint32_t val;
	
	if (!memAccess(lcd->mem, addr, sizeof(uint32_t), false, &val))
		val = 0xaaaaaaaaul;
	
	switch (lcd->lcdcon5 & 3) {	//swapping
		case 0:	//none
			break;
		case 1:	//halfwords
			val = (val >> 16) | (val << 16);
			break;
		case 2:	//bytes
			val = __builtin_bswap32(val);
			break;
		case 3:	//bytes & halwords (meaning byte sin halfwords)
			val = ((val & 0xff00ff00ul) >> 8) | ((val << 8) & 0xff00ff00ul);
			break;
	}
	
	return val;
}

void s3c24xxLcdPeriodic(struct S3C24xxLcd *lcd)
{
	uint32_t i, w, h;
	uint16_t *px;
	
	if (!(lcd->lcdcon1 & 1))	//if lcd is off, nothing to do
		return;
	
	lcd->lcdsrcpnd &=~ 2;		//usually this is off
	
	//pretend like we render each line and each frame:
	
	//check where we are vertically
	switch ((lcd->lcdcon5 >> 15) & 3) {
		case 1:	//we are in back porch - interupt that frame is about to start
			lcd->lcdsrcpnd |= 2;
			//fallthrough
		case 0:	//we are in vsync
			lcd->lcdcon5 += 0x8000;
			goto lcd_done;
		case 3:	//we are in front porch
			lcd->lcdcon5 -= 0x18000ul;
			goto lcd_done;
		case 2:	//active area
			break;
	}
	
	//we are in an active screen area - let's see where we ar ehorizontally
	switch ((lcd->lcdcon5 >> 13) & 3) {
		case 1:	//we are in back porch - interupt that frame is about to start
		case 0:	//we are in hsync
			lcd->lcdcon5 += 0x2000;
			goto lcd_done;
		case 3:	//we are in front porch
			lcd->lcdcon5 -= 0x6000;
			lcd->lcdcon5 += 0x8000;
			goto lcd_done;
		case 2:	//active area
			lcd->lcdcon5 += 0x2000;
			break;
	}
	
	//we are in active line area - count the line and go on - we render entire screen a tline 0
	if (lcd->lcdcon1 & 0x0ffc0000ul) {
		lcd->lcdcon1 -= 0x00040000ul;
		goto lcd_done;
	}
	
	//RENDER
	
	//get dimensions (horizontally this get complex VERY fast
	h = ((lcd->lcdcon2 >> 14) & 0x3ff) + 1;	//LINEVAL, not actual height yet
	w = ((lcd->lcdcon3 >> 8) & 0x7ff) + 1;	//HOZVAL, not actual width yet
	switch (((lcd->lcdcon1) >> 5) & 3) {
		case 0:	//4-bit dual scan STN
			h *= 2;
			//fallthrough
		case 1:	//4 bit single scan STN
			w = w * 4 / 3;	//"/ 3" assumes color display .for B&W, it woudl not be needed
			break;
		
		case 2:	//8-bit single scan STN
			w = w * 8 / 3;	//"/ 3" assumes color display .for B&W, it woudl not be needed
			break;
		
		case 3:	//TFT
			//w is accurate
			break;
	}
	
	//set LINECNT again
	lcd->lcdcon1 |= (lcd->lcdcon2 << 4) & 0x0ffc0000ul;
	
	px = s3c24xxLcdPrvGetFb(lcd, w, h);
	
	if (lcd->tpal & 0x01000000ul) {	//TPAL
		
		uint32_t v = ((lcd->tpal >> 8) & 0xf800) | ((lcd->tpal >> 5) & 0x07e0) | ((lcd->tpal >> 3) * 0x001f);
		
		for (i = 0; i < w * h; i++)
			*px++ = v;
	}
	else {
		static const uint8_t gamma5[] = {0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f, };
		static const uint8_t gamma6[] = {0x0, 0x4, 0x8, 0xd, 0x11, 0x15, 0x19, 0x1d, 0x22, 0x26, 0x2a, 0x2e, 0x32, 0x37, 0x3b, 0x3f, };
		uint32_t r, c, t, v, pa = (lcd->lcdsaddr1 << 1) & 0x7ffffffeul, strideExtra = 2 * ((lcd->lcdsaddr3 >> 11) & 0x7ff);
		
		for (r = 0; r < h; r++, pa += strideExtra) {
			
			switch ((lcd->lcdcon1 >> 1) & 0x0f) {
				case 0x00:	//STN 1bpp
				case 0x08:	//LCD 1bpp
					for (c = 0; c < w; c += 32, pa += 4) {
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 32; i++, v <<= 1)
							*px++ = (v >> 31) ? 0 : 0xffff;
					}
					break;
				
				case 0x01:	//STN 2bpp
				case 0x09:	//TFT 2bpp
					for (c = 0; c < w; c += 16, pa += 4) {
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 16; i++, v <<= 2) {
							static const uint16_t shades[] = {0xffff, 0xA554, 0x52AA, 0x0000};
							
							*px++ = shades[v >> 30];
						}
					}
					break;
				
				case 0x02:	//STN 4bpp
				case 0x0a:	//TFT 4bpp
					for (c = 0; c < w; c += 8, pa += 4) {
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 8; i++, v <<= 4) {
							static const uint16_t shades[] = {0xffff, 0xef7d, 0xdefb, 0xce59, 0xbdd7, 0xad55, 0x9cd3, 0x8c51, 0x73ae, 0x632c, 0x52aa, 0x4228, 0x31a6, 0x2104, 0x1082, 0x0000, };
							
							*px++ = shades[v >> 28];
						}
					}
					break;
				
				case 0x03:	//STN 8bpp - direct color
					for (c = 0; c < w; c += 4, pa += 4) {
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 4; i++, v <<= 8) {
							
							static const uint8_t gamma5[] = {0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f, };
							static const uint8_t gamma6[] = {0x0, 0x4, 0x8, 0xd, 0x11, 0x15, 0x19, 0x1d, 0x22, 0x26, 0x2a, 0x2e, 0x32, 0x37, 0x3b, 0x3f, };
							
							
							//get components
							uint_fast8_t pv = v >> 24;
							uint_fast8_t rv = (pv >> 5) & 7;
							uint_fast8_t gv = (pv >> 2) & 7;
							uint_fast8_t bv = (pv & 3);
							
							//lookup
							rv = (lcd->redlut >> (rv * 4)) & 0x0f;
							gv = (lcd->greenlut >> (gv * 4)) & 0x0f;
							bv = (lcd->bluelut >> (bv * 4)) & 0x0f;
							
							//expand to our bit withs
							rv = gamma5[rv];
							gv = gamma6[gv];
							bv = gamma5[bv];
							
							//to one pixel
							t = rv;
							t <<= 5;
							t += gv;
							t <<= 6;
							t += bv;
							
							*px++ = t;
						}
					}
					break;
				
				case 0x04:	//STN 12bpp - direct color
					for (c = 0; c < w; c += 8) {
						
						uint_fast8_t rv = 0, gv = 0, bv = 0, cc, bc;
						
						for (bc = 0, cc = 0, i = 0; i < 24; i++, v <<= 4, bc--) {
							
							if (!bc) {
								bc = 8;
								v = s3c24xxLcdPrvReadWord(lcd, pa);
								pa += 4;
							}
							
							switch (cc++) {
								case 0:
									rv = v >> 28;
									break;
								case 1:
									gv = v >> 28;
									break;
								case 2:
									bv = v >> 28;
									
									//expand to our bit withs
									rv = gamma5[rv];
									gv = gamma6[gv];
									bv = gamma5[bv];
									
									//to one pixel
									t = rv;
									t <<= 6;
									t += gv;
									t <<= 5;
									t += bv;
									
									*px++ = t;
									cc = 0;
							}
						}
					}
					break;
				
				case 0x05:		//STN 12bit unpacked - S3C2440 only
					for (c = 0; c < w; c += 2, pa += 4) {
						uint_fast8_t rv = 0, gv = 0, bv = 0;
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 2; i++, v <<= 16) {
							rv = (v >> 24) & 0x0f;
							gv = (v >> 20) & 0x0f;
							bv = (v >> 16) & 0x0f;
							
							//expand to our bit withs
							rv = gamma5[rv];
							gv = gamma6[gv];
							bv = gamma5[bv];
							
							//to one pixel
							t = rv;
							t <<= 6;
							t += gv;
							t <<= 5;
							t += bv;
							
							*px++ = t;
							
						}
					}
					break;
				
				case 0x06:		//STN 16bit - S3C2440 only
					for (c = 0; c < w; c += 2, pa += 4) {
						uint_fast8_t rv = 0, gv = 0, bv = 0;
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 2; i++, v <<= 16) {
							rv = (v >> 28) & 0x0f;	//collapse to 4-bit data
							gv = (v >> 23) & 0x0f;
							bv = (v >> 17) & 0x0f;
							
							//expand to our bit withs
							rv = gamma5[rv];
							gv = gamma6[gv];
							bv = gamma5[bv];
							
							//to one pixel
							t = rv;
							t <<= 6;
							t += gv;
							t <<= 5;
							t += bv;
							
							*px++ = t;
							
						}
					}
					break;
				
				case 0x0b:		//TFT 8bpp
					for (c = 0; c < w; c += 4, pa += 4) {
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						for (i = 0; i < 4; i++, v <<= 8) {
							
							*px++ = lcd->pal[v >> 24];
						}
					}
					break;
				
				case 0x0c:		//TFT 16bpp
					for (c = 0; c < w; c += 2, pa += 4) {
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						*px++ = v >> 16;
						*px++ = v;
					}
					break;
				
				case 0x0d:		//TFT 24bpp
					for (c = 0; c < w; c++, pa += 4) {
						uint_fast8_t rv, gv, bv;
						
						v = s3c24xxLcdPrvReadWord(lcd, pa);
						
						rv = (v >> 16) & 0xff;
						gv = (v >> 8) & 0xff;
						bv = (v >> 0) & 0xff;
						
						*px++ = (((uint32_t)(rv & 0xf8)) << 8) | (((uint32_t)(gv & 0xfc)) << 3) | (bv >> 3);
					}
					break;
				
				default:
					ERR("LCD color mode 0x0%x\n", (unsigned)((lcd->lcdcon1 >> 1) & 0x0f));
					break;
			}
		}
	}
	s3c24xxLcdPrvReleaseFb(lcd);

lcd_done:
	s3c24xxLcdPrvUpdateInts(lcd);
}

struct S3C24xxLcd* s3c24xxLcdInit(struct ArmMem *physMem, struct SocIc *ic, bool hardGrafArea)
{
	struct S3C24xxLcd *lcd = (struct S3C24xxLcd*)malloc(sizeof(*lcd));
	
	if (!lcd)
		ERR("cannot alloc LCD");
	
	memset(lcd, 0, sizeof (*lcd));
	lcd->mem = physMem;
	lcd->ic = ic;
	lcd->hardGrafArea = hardGrafArea;
	lcd->lcdintmsk = 3;
	lcd->lpcsel = 4;
	
	if (!memRegionAdd(physMem, S3C24XX_LCD_BASE, S3C24XX_LCD_SIZE, s3c24xxLcdPrvMemAccessF, lcd))
		ERR("cannot add LCD to MEM\n");
	
	return lcd;
}





