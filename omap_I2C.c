//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_DMA.h"
#include "omap_IC.h"
#include "soc_I2C.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_I2C_BASE	0xFFFB3800UL
#define OMAP_I2C_SIZE	0x00000040UL


struct SocI2c {
	
	//emu state
	struct SocDma *dma;
	struct SocIc *ic;
	struct DevI2C {
		I2cDeviceActionF actF;
		void *userData;
	} devs[8];
	
	//regs
	uint16_t stat, buf, dcount, con, oa, sa, systest;
	uint8_t ie, psc, scll, sclh;
	
	//device state
	uint8_t haveTxDataShift	:1;
	uint8_t txHasSingleByte	:1;
	uint8_t rxFifoFull		:1;
	uint16_t dcountOrig, fifoVal, prevStat;
};

bool socI2cDeviceAdd(struct SocI2c *i2c, I2cDeviceActionF actF, void *userData)
{
	uint_fast8_t i;
	
	for (i = 0; i < sizeof(i2c->devs) / sizeof(*i2c->devs); i++) {
		
		if (i2c->devs[i].actF)
			continue;
		
		i2c->devs[i].actF = actF;
		i2c->devs[i].userData = userData;
		
		return true;
	}
	
	return false;
}
					
static void socI2cPrvRecalcIrq(struct SocI2c *i2c)
{
	uint16_t diff = i2c->stat & i2c->prevStat, newHighs = diff & i2c->stat;
	uint_fast8_t effectiveIe = i2c->ie;
	
	if (i2c->buf & 0x8000)
		effectiveIe &=~ 0x0008;
	if (i2c->buf & 0x0080)
		effectiveIe &=~ 0x0010;
	
	if (newHighs & effectiveIe & 0x001f) {
		//edge
		socIcInt(i2c->ic, OMAP_I_I2C, true);
		socIcInt(i2c->ic, OMAP_I_I2C, false);
	}
	
	i2c->prevStat = i2c->stat;
	
	socDmaExternalReq(i2c->dma, DMA_REQ_I2C_RX, (i2c->buf & 0x8000) && (i2c->stat & 0x0008));
	socDmaExternalReq(i2c->dma, DMA_REQ_I2C_TX, (i2c->buf & 0x0080) && (i2c->stat & 0x0010));
}

static void socI2cPrvReset(struct SocI2c *i2c)
{
	i2c->stat = 0;
	i2c->buf = 0;
	i2c->dcount = 0;
	i2c->con = 0;
	i2c->oa = 0;
	i2c->sa = 0;
	i2c->systest = 0;
	i2c->ie = 0;
	i2c->psc = 0;
	i2c->scll = 0;
	i2c->sclh = 0;
	i2c->haveTxDataShift = 0;
	i2c->rxFifoFull = 0;
	i2c->txHasSingleByte = 0;
	
	socI2cPrvRecalcIrq(i2c);
}

static uint_fast8_t socI2cPrvAction(struct SocI2c *i2c, enum ActionI2C action, uint8_t param)
{
	static const char *acts[] = {
		[i2cStart] = "start",
		[i2cRestart] = "restart",
		[i2cTx] = "tx",
		[i2cRx] = "rx",
		[i2cStop] = "stop"
	};
	uint_fast8_t ret = 0, i;
	
	for (i = 0; i < sizeof(i2c->devs) / sizeof(*i2c->devs); i++) {
		
		if (!i2c->devs[i].actF)
			continue;
		
		ret |= i2c->devs[i].actF(i2c->devs[i].userData, action, param);
	}
	
	fprintf(stderr, "I2C: %s 0x%02x -> 0x%02x\n", acts[action], param, ret);
	
	return ret;
}

static void socI2cPrvRun(struct SocI2c *i2c)
{
	while (1) {
		bool repeatMode = !!(i2c->con & 0x0004);
		uint_fast8_t ret;
		
		if (i2c->con & 0x0001) {	//start condition
			
			(void)socI2cPrvAction(i2c, (i2c->con & 0x1000) ? i2cRestart : i2cStart, 0); //bus was busy? restart, else start
			i2c->con &=~ 0x0001;	//start has been done
			i2c->stat |= 0x1000;	//bus became busy
			
			//send address
			ret = socI2cPrvAction(i2c, i2cTx, i2c->sa * 2 + ((i2c->con & 0x200) ? 0 : 1));
			if (!ret) {
				i2c->stat |= 0x0002;	//NAK
				break;
			}
		}
	
		if (!(i2c->stat & 0x1000))	//bus is not busy? we're done
			break;
		
		if (i2c->con & 0x200) {	//write mode
			
			if (repeatMode || i2c->dcount) {		//more data to send
				
				//we are or will be out soon...
				i2c->stat |= 0x0410;
				if (i2c->haveTxDataShift) {
					
					ret = socI2cPrvAction(i2c, i2cTx, i2c->fifoVal & 0xff);
					if (!ret) {
						i2c->stat |= 0x0002;	//NAK
						break;
					}
					if (!repeatMode)
						i2c->dcount--;
					
					if (!i2c->txHasSingleByte) {
						if (repeatMode || i2c->dcount) {
							ret = socI2cPrvAction(i2c, i2cTx, i2c->fifoVal >> 8);
							if (!ret) {
								i2c->stat |= 0x0002;	//NAK
								break;
							}
						}
						if (!repeatMode)
							i2c->dcount--;
					}
					i2c->haveTxDataShift = 0;
				}
				else
					break;
			}
			else
				break;
		}
		else {				//read mode
			
			if (repeatMode || i2c->dcount) {		//more data to rx
				
				bool stopNow = !repeatMode;
				
				if (i2c->rxFifoFull) {
					i2c->stat |= 0x0008;	//data ready
					break;
				}
				
				repeatMode = !repeatMode && i2c->dcount != 1;
				i2c->fifoVal = socI2cPrvAction(i2c, i2cRx, !repeatMode && i2c->dcount != 1);
				i2c->rxFifoFull = 1;
				
				if (!repeatMode)
					i2c->dcount--;
					
				if (repeatMode || i2c->dcount) {
					ret = socI2cPrvAction(i2c, i2cRx, !repeatMode && i2c->dcount != 1);
					
					i2c->fifoVal |= ((uint_fast16_t)ret) << 8;
					if (!repeatMode)
						i2c->dcount--;
				}
			}
			else
				break;
		}
		
		if (i2c->con & 0x0002) {	//stop mode
			
			if (repeatMode || !i2c->dcount) {
				i2c->stat &=~ 0x1000;	//bus became busy
				break;
			}
		}
	}
	socI2cPrvRecalcIrq(i2c);
}

static bool socI2cPrvFifoW(struct SocI2c *i2c, uint_fast16_t val, bool singleByte)
{
	if (i2c->haveTxDataShift)
		ERR("write while fifo is full\n");
	i2c->haveTxDataShift = 1;
	i2c->txHasSingleByte = singleByte;
	i2c->fifoVal = val;
	
	socI2cPrvRun(i2c);
	
	return true;
}

static bool socI2cPrvFifoR(struct SocI2c *i2c, uint_fast16_t *valP,  bool singleByte)
{
	if (!i2c->rxFifoFull)
		ERR("read while fifo is empty\n");
	i2c->rxFifoFull = 0;
	*valP = i2c->fifoVal;
	
	socI2cPrvRun(i2c);
	
	return true;
}


static void socI2cPrvConW(struct SocI2c *i2c)
{
	if (!(i2c->con & 0x0400))	//master mode off - nothing to do
		return;
	
	socI2cPrvRun(i2c);
}

static bool socI2cPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocI2c *i2c = (struct SocI2c*)userData;
	uint_fast16_t val = 0;
	bool ret = true;
	
	if ((size != 4 && size != 2 && size != 1) | (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_I2C_BASE) >> 2;
	
	if (write) switch (size) {
		case 1:
			val = *(uint8_t*)buf;
			break;
		case 2:
			val = *(uint16_t*)buf;
			break;
		case 4:
			val = *(uint32_t*)buf;
			break;
	}
		
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				ret = false;
			else
				val = 0x0010;
			break;
		
		case 0x04 / 4:
			if (write) {
				i2c->ie = val & 0x1e;
				socI2cPrvRecalcIrq(i2c);
			}
			else
				val = i2c->ie;
			break;
			
		case 0x08 / 4:
			if (write)
				ret = false;
			else
				val = i2c->stat;
			break;
		
		case 0x0c / 4:
			if (write)
				ret = false;
			else {
				if (i2c->stat & 0x1f) {
					val = __builtin_ctz(i2c->stat) + 1;
					i2c->stat &=~ (1 << (val - 1));
					if (val == 3)
						i2c->stat &=~ 0x8000;
					socI2cPrvRecalcIrq(i2c);
				}
				else
					val = 0;
			}
			break;
		
		case 0x14 / 4:
			if (write) {
				i2c->buf = val & 0x8080;
				socI2cPrvRecalcIrq(i2c);
			}
			else
				val = i2c->buf;
			break;
		
		case 0x18 / 4:
			if (write)
				i2c->dcount = val;
			else
				val = i2c->dcount;
			break;
		
		case 0x1c / 4:
			if (write) {
				if (size != 1 && (i2c->con & 0x4000))
					val = __builtin_bswap16(val);
				ret = socI2cPrvFifoW(i2c, val, size == 1);
			}
			else {
				ret = socI2cPrvFifoR(i2c, &val, size == 1);
				if (size != 1 && (i2c->con & 0x4000))
					val = __builtin_bswap16(val);
			}
			break;
		
		case 0x24 / 4:
			if (write) {
				i2c->con = val & 0xcf07;
				if (!(val & 0x8000))
					socI2cPrvReset(i2c);
				if (val & 0x0800)
					ERR("start byte mode not supported (who needs it anyways?)\n");
				if (val & 0x0100)
					ERR("XA mode not supported (who needs it anyways?)\n");
				socI2cPrvConW(i2c);
			}
			else
				val = i2c->con;
			break;
		
		case 0x28 / 4:
			if (write)
				i2c->oa = val & 0x03ff;
			else
				val = i2c->oa;
			break;
		
		case 0x2c / 4:
			if (write)
				i2c->sa = val & 0x03ff;
			else
				val = i2c->sa;
			break;
		
		case 0x30 / 4:
			if (write)
				i2c->psc = val & 0x00ff;
			else
				val = i2c->psc;
			break;
		
		case 0x34 / 4:
			if (write)
				i2c->scll = val & 0x00ff;
			else
				val = i2c->scll;
			break;
		
		case 0x38 / 4:
			if (write)
				i2c->sclh = val & 0x00ff;
			else
				val = i2c->sclh;
			break;
		
		case 0x3c / 4:
			if (write)
				i2c->systest = val & 0xf00f;
			else
				val = i2c->systest;
			break;
		
		default:
			ret = false;
			break;
	}
	
	if (!write) switch (size) {
		case 1:
			*(uint8_t*)buf = val;
			break;
		case 2:
			*(uint16_t*)buf = val;
			break;
		case 4:
			*(uint32_t*)buf = val;
			break;
	}
	return ret;
}

struct SocI2c* socI2cInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma)
{
	struct SocI2c *i2c = (struct SocI2c*)malloc(sizeof(*i2c));
	
	if (!i2c)
		ERR("cannot alloc I2C");
	
	memset(i2c, 0, sizeof (*i2c));
	i2c->dma = dma;
	i2c->ic = ic;
	socI2cPrvReset(i2c);
	
	if (!memRegionAdd(physMem, OMAP_I2C_BASE, OMAP_I2C_SIZE, socI2cPrvMemAccessF, i2c))
		ERR("cannot add I2C to MEM\n");
	
	return i2c;
}
