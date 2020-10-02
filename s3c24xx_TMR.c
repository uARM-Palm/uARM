//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_TMR.h"
#include "s3c24xx_DMA.h"
#include "s3c24xx_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"


#define S3C24XX_TMR_BASE	0x51000000UL
#define S3C24XX_TMR_SIZE	0x44

struct TimerUnit {
	uint16_t reloadVal, curVal, compareVal;
	uint8_t psc;
};

struct S3C24xxTimers {
	
	struct SocDma *dma;
	struct SocIc *ic;
	
	uint32_t tcfg1, tcon;
	uint8_t dzl, psc01, psc234, cnt01, cnt234;
	
	struct TimerUnit unit[5];
};

static bool s3c24xxTimersPrvClockMgrMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct S3C24xxTimers *tmr = (struct S3C24xxTimers*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - S3C24XX_TMR_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch(pa){
		
		case 0x00 / 4:
			if (write) {
				tmr->dzl = val >> 16;
				tmr->psc234 = val >> 8;
				tmr->psc01 = val;
			}
			else
				val = (((uint32_t)tmr->dzl) << 16) + (((uint32_t)tmr->psc234) << 8) + tmr->psc01;
			break;
		
		case 0x04 / 4:
			if (write)
				tmr->tcfg1 = 0x00fffffful;
			else
				val = tmr->tcfg1;
			break;
			
		case 0x08 / 4:
			if (write)
				tmr->tcon = 0x007fff1ful;
			else
				val = tmr->tcon;
			break;
		
		case 0x0c / 4:
		case 0x18 / 4:
		case 0x24 / 4:
		case 0x30 / 4:
		case 0x3c / 4:
			if (write)
				tmr->unit[(pa - 0x0c / 4) / 3].reloadVal = val;
			else
				val = tmr->unit[(pa - 0x0c / 4) / 3].reloadVal;
			break;
		
		case 0x10 / 4:
		case 0x1c / 4:
		case 0x28 / 4:
		case 0x34 / 4:
			if (write)
				tmr->unit[(pa - 0x0c / 4) / 3].compareVal = val;
			else
				val = tmr->unit[(pa - 0x0c / 4) / 3].compareVal;
			break;
		
		case 0x14 / 4:
		case 0x20 / 4:
		case 0x2c / 4:
		case 0x38 / 4:
		case 0x40 / 4:
			if (write)
				return false;	//read only reg
			else
				val = tmr->unit[(pa - 0x0c / 4) / 3].curVal;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

struct S3C24xxTimers* s3c24xxTimersInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma)
{
	struct S3C24xxTimers *tmr = (struct S3C24xxTimers*)malloc(sizeof(*tmr));
	
	if (!tmr)
		ERR("cannot alloc Timers unit");
	
	memset(tmr, 0, sizeof (*tmr));
	
	tmr->dma = dma;
	tmr->ic = ic;
	
	if (!memRegionAdd(physMem, S3C24XX_TMR_BASE, S3C24XX_TMR_SIZE, s3c24xxTimersPrvClockMgrMemAccessF, tmr))
		ERR("cannot add Timers to MEM\n");
	
	return tmr;
}

static void s3c24xxTimersPrvTimerTick(struct S3C24xxTimers* tmr, uint_fast8_t idx)
{
	struct TimerUnit *unit = &tmr->unit[idx];
	uint_fast8_t iss = idx ? 4 + 4 * idx : 0;
	uint_fast8_t imu = iss + 1;
	uint_fast8_t iar = imu + ((idx == 4) ? 1 : 2);
	bool ss = (tmr->tcon >> iss) & 1, mu = (tmr->tcon >> imu) & 1, ar = (tmr->tcon >> iar) & 1;
	uint_fast8_t dmaSel = (tmr->tcfg1 >> 20) & 0x0f;
	
	if (mu) {	//docs say not to leave it on, but some do so we must clear it when we do the deed
		
		unit->curVal = unit->reloadVal;
		tmr->tcon &=~ (1UL << imu);
	}
	
	if (ss) {	//running?
		
		if (unit->curVal)
			unit->curVal--;
		else {
			
			if (ar)
				unit->curVal = unit->reloadVal;
			else
				tmr->tcon &=~ (1UL << iss);		//turn it off
			
			if (idx + 1 == dmaSel) {
				//this is quite wrong since we never deassert the DMA request. we need to work on that. we need to assert it for just one burst
		//		socDmaExternalReq(tmr->dma, DMA_REQ_TIMER, true);
				ERR("dma not yet implemented for timers\n");
			}
			else
				socIcInt(tmr->ic, S3C24XX_I_TIMER0 + idx, true);
		}
	}
}

void s3c24xxTimersPeriodic(struct S3C24xxTimers* tmr)
{
	uint_fast8_t i, pscLim, tmrAct = 0;
	
	//first level of prescalers
	if (tmr->cnt01++ >= tmr->psc01) {
		tmr->cnt01 = 0;
		tmrAct |= 0x03;
	}
	if (tmr->cnt234++ >= tmr->psc234) {
		tmr->cnt234 = 0;
		tmrAct |= 0x1c;
	}
	
	//second level of prescalers
	for (i = 0; i < 5; i++) {
		
		if (tmrAct & (1 << i)) {
			
			pscLim = 2 << ((tmr->tcfg1 >> (i * 4)) & 0x0f);
			
			if (tmr->unit[i].psc++ >= pscLim) {
				
				tmr->unit[i].psc = 0;
				
				s3c24xxTimersPrvTimerTick(tmr, i);
			}
		}
	}
}

