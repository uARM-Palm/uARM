//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "mmiodev_W86L488.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"

//MMIO info
#define W86L488_SIZE 		0x10


//direct regs
#define REG_NO_CMD_RSP				0x00
#define REG_NO_STAT_CTRL			0x01
#define REG_NO_RX_TX_FIFO			0x02
#define REG_NO_INT_STAT_CTRL		0x03
#define REG_NO_GPIO					0x04
#define REG_NO_GPIO_IRQ_CTRL		0x05
#define REG_NO_INDIRECT_ADDR		0x06
#define REG_NO_INDIRECT_DATA		0x07

//indirect regs (div 1)
#define IREG_NO_XTD_STA_AND_SETT	0x00
#define IREG_NO_SDIO_CTL			0x01
#define IREG_NO_MASTER_DATA_FMT		0x02
#define IREG_NO_MASTER_BLOCK_CT		0x03
#define IREG_NO_SLAVE_DATA_FMT		0x04
#define IREG_NO_SLAVE_BLOCK_CT		0x05
#define IREG_NO_NAK_TO				0x06
#define IREG_NO_ERR_STATUS			0x07
#define IREG_NO_HOST_IFACE			0x08
#define IREG_NO_TEST				0x09
#define IREG_NO_ID_CODE				0x0a




struct W86L488 {
	
	struct SocGpio *gpio;
	VSD *vsd;
	int8_t intGpio;
	
	//direct regs
	uint16_t sta;
	uint16_t ctrl;
	uint16_t ints;	//status and ctrl
	uint16_t indAddr;
	
	uint8_t gpioInts;
	uint8_t gpioPrevStates;
	uint8_t gpioIrqSta;
	
	uint8_t intCdIe			: 1;	//these occupy the same bit...ugh
	uint8_t intCwrRspIe		: 1;
	uint8_t intCdSta		: 1;	//these occupy the same bit...ugh
	uint8_t intCwrRspSta	: 1;
	
	
	//indirect regs
	uint16_t xtdStatus;
	uint16_t settings;
	uint16_t sdio;
	uint16_t mDataFmt;
	uint16_t mBlockCnt;
	uint16_t sDataFmt;
	uint16_t sBlockCnt;
	uint16_t nakTO;
	uint16_t errSta;
	uint16_t bufSvcLen;
	uint16_t hostIface;
	
	//command stuff
	uint16_t cmdWhi;
	uint16_t cmdWmid;
	uint8_t  cmdBitsCnt	:3;
	
	//resp stuff
	uint16_t resp[9];

	//GPIOS: inputs are INVERTED, outputs aren't!!!
	uint8_t gpiosInput;		//as provided in (NOT INVERTED)
	uint8_t gpioLatches;	//as requested for out
	uint8_t gpioDirs;		//as requesed, 1 = out

};





static void w86l488PrvRecalcInts(struct W86L488 *wl)
{
	bool haveIrq = false;
	
	if (wl->ints & 0x0001) {	//are ints on at all?
		
		if (wl->ints & (wl->ints >> 8) & 0xf4)								//some are in same positions
			haveIrq = true;
		else if ((wl->ints & 0x0300) && (wl->ints & 0x0002))				//BAE and TOE are enabled by the same bit
			haveIrq = true;
		else if (wl->intCdIe && wl->intCdSta && !(wl->ctrl & 0x10))			//CD only signalled is SIEN is off
			haveIrq = true;
		else if (wl->intCwrRspIe && wl->intCwrRspSta && wl->ctrl & 0x10)	//Cwr_RSP_IE only signalled is SIEN is on
			haveIrq = true;
	}
	
	socGpioSetState(wl->gpio, wl->intGpio, !haveIrq); //active low
}

static uint8_t w86l488PrvGetCurGpioState(struct W86L488 *wl)
{
	uint8_t val;
	
	//inputs are read inverted!
	val = (wl->gpioLatches & wl->gpioDirs) | ((~wl->gpiosInput) & (~wl->gpioDirs));
	val &= 0x1f;
	
	return val;
}

static void w86l488gpioRecalc(struct W86L488 *wl)
{
	uint8_t gpioState = w86l488PrvGetCurGpioState(wl);
	uint8_t gpioDiffs = gpioState ^ wl->gpioPrevStates;
	uint8_t gpiosWentLow = gpioDiffs & wl->gpioPrevStates;
	uint8_t gpiosCouldIrq = (gpiosWentLow & 0x1e) | (gpioDiffs & 0x01);
	
	wl->gpioPrevStates = gpioState;
	
	wl->gpioIrqSta |= gpiosCouldIrq & wl->gpioInts;
	
	if (wl->gpioIrqSta)
		wl->ints |= 0x1000;	//GIT
	
	w86l488PrvRecalcInts(wl);
}

void w86l488gpioSetVal(struct W86L488 *wl, unsigned gpioNum, bool hi)
{
	if (gpioNum >= 5)
		return;
	
	if (hi)
		wl->gpiosInput |= (1 << gpioNum);
	else
		wl->gpiosInput &=~ (1 << gpioNum);
	
	w86l488gpioRecalc(wl);
}

static bool w86l488PrvIndirectAccess(struct W86L488 *wl, bool write, uint16_t *buf)
{
	bool ret = true;
	uint32_t val = 0;
	
	if (write)
		val = *buf;
	
	switch (wl->indAddr) {
		
		case IREG_NO_XTD_STA_AND_SETT:
			if (write)
				wl->settings = val & 0x0bff;
			else
				val = wl->settings | wl->xtdStatus;
			break;
		
		case IREG_NO_SDIO_CTL:
			if (write)
				wl->sdio = (wl->sdio & 0xffc0) | (val & 0x003f);
			else
				val = wl->sdio;
			break;
		
		case IREG_NO_MASTER_DATA_FMT:
			if (write)
				wl->mDataFmt = val & 0xcfff;
			else
				val = wl->mDataFmt;
			break;
		
		case IREG_NO_MASTER_BLOCK_CT:
			if (write)
				wl->mBlockCnt = val & 0x81ff;
			else
				val = wl->mBlockCnt;
			break;
		
		case IREG_NO_SLAVE_DATA_FMT:
			if (write)
				wl->sDataFmt = val & 0x4fff;
			else
				val = wl->sDataFmt;
			break;
		
		case IREG_NO_SLAVE_BLOCK_CT:
			if (write)
				wl->sBlockCnt = val & 0x81ff;
			else
				val = wl->sBlockCnt;
			break;
		
		case IREG_NO_NAK_TO:
			if (write)
				wl->nakTO = val;
			else
				val = wl->nakTO;
			break;
		
		case IREG_NO_ERR_STATUS:
			if (write)
				ret = false;
			else
				val = wl->errSta;
			break;
		
		case IREG_NO_HOST_IFACE:
			if (write)
				wl->hostIface = val & 0x3f8e;
			else
				val = (wl->bufSvcLen & 0xff00) | (wl->hostIface & 0x008e);
			break;
		
		case IREG_NO_TEST:
			if (write && val)
				ret = false;
			else if (!write)
				val = 0;
			break;
		
		case IREG_NO_ID_CODE:
			if (write)
				ret = false;
			else
				val = 0x488c;
			break;
		
		default:
			return false;
	}
	
	static const char *strIndirect[] = {
		[IREG_NO_XTD_STA_AND_SETT] = "STA/SETT",
		[IREG_NO_SDIO_CTL] = "SDIO CTRL",
		[IREG_NO_MASTER_DATA_FMT] = "M DT FMT",
		[IREG_NO_MASTER_BLOCK_CT] = "M BLK CT",
		[IREG_NO_SLAVE_DATA_FMT] = "S DT FMT",
		[IREG_NO_SLAVE_BLOCK_CT] = "S BLK CT",
		[IREG_NO_NAK_TO] = "NAK TO",
		[IREG_NO_ERR_STATUS] = "ERR STA",
		[IREG_NO_HOST_IFACE] = "HOST IFACE",
		[IREG_NO_TEST] = "TEST",
		[IREG_NO_ID_CODE] = "ID CODE",
	};
	fprintf(stderr, "WL %c i[%02x %10s] == 0x%04lx\n", (char)(write ? 'W' : 'R'), wl->indAddr, strIndirect[wl->indAddr], (unsigned long)val);
	
	if (!write)
		*buf = val;
	
	return ret;
}

static void w86l488PrvExecCmd(struct W86L488 *wl, uint8_t cmd, uint32_t param)
{
	enum SdReplyType ret;
	uint8_t reply[17];
	
	fprintf(stderr, "cmd %u (0x%08lx)\n", cmd, (unsigned long)param);
	
	wl->sta &=~ 0x3000;
	
	if (wl->vsd) {
		fprintf(stderr, "sending cmd %u (0x%08lx)\n", cmd & 0x3f, (unsigned long)param);
		ret = vsdCommand(wl->vsd, cmd, param, reply);
	}
	else {
		fprintf(stderr, "MMC unit has no SD card - command ignored\n");
		ret = SdReplyNone;
	}
	fprintf(stderr, "SD says %d\n", ret);
	wl->xtdStatus &=~ 0x8000;	//not waiting for reply anymore
	wl->xtdStatus &=~ 0x1000;	//clock not running (cmd process is over)
	
	switch (ret) {
		case SdReplyNone:
			//????
			break;
		
		case SdReply48bitsAndBusy:
		case SdReply48bits:
			wl->sta |= 0x1000;
			break;
		
		case SdReply136bits:
			wl->sta |= 0x3000;
			break;
	}
	
	
}

static bool w86l488PrvCmdW(struct W86L488 *wl, uint16_t val)
{
	uint32_t param;
	uint8_t cmd;
	
	switch (wl->cmdBitsCnt++) {
		case 0:
			wl->cmdWhi = val;
			break;
		
		case 1:
			wl->cmdWmid = val;
			break;
		
		case 2:
			wl->cmdBitsCnt = 0;
			cmd = (wl->cmdWhi >> 8) & 0x3f;
			param = wl->cmdWhi & 0xff;
			param <<= 16;
			param |= wl->cmdWmid;
			param <<= 8;
			param |= val >> 8;
			
			w86l488PrvExecCmd(wl, cmd, param);
			break;
	}
	
	return true;
}

static bool w86l488PrvFifoW(struct W86L488 *wl, uint16_t val)
{
	(void)wl;
	fprintf(stderr, "FIFO W: 0x%04x\n", val);
	
	return false;
}

static bool w86l488PrvFifoR(struct W86L488 *wl, uint16_t *valP)
{
	(void)wl;
	fprintf(stderr, "FIFO R\n");
	
	*valP = 0;
	
	return false;
}

static bool w86l488PrvCtrlW(struct W86L488 *wl, uint16_t val)
{
	wl->ctrl = val & 0x009f;
	if (val & 0x0100) {		//S_RST
		
		wl->cmdBitsCnt = 0;
		memset(wl->resp, 0, sizeof(wl->resp));
	}
	if (val & 0x0040) {		//DRST_S/DS_RST_S
		
		//slave logic reset
	}
	if (val & 0x0020) {		//DRST_M
		
		//master logic reset
	}
	
	//irqs might be affected
	w86l488PrvRecalcInts(wl);
	return true;
}

static bool w86l488PrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct W86L488 *wl = (struct W86L488*)userData;
	bool ret = true;
	uint32_t val = 0;
	
	if(size != 2) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;
	}
	
	pa = (pa % W86L488_SIZE) >> 1;
	
	if (write)
		val = *(uint16_t*)buf;
	
	switch (pa) {
		
		case REG_NO_CMD_RSP:
			if (write)
				ret = w86l488PrvCmdW(wl, val);
			else {
				val = wl->resp[0];
				memmove(wl->resp + 0, wl->resp + 1, sizeof(wl->resp) - sizeof(*wl->resp));
			}
			break;
		
		case REG_NO_STAT_CTRL:
			if (write)
				ret = w86l488PrvCtrlW(wl, val & 0x1ff);
			else
				val = (wl->sta & 0xff00) | (wl->ctrl & 0x00ff);
			break;
		
		case REG_NO_RX_TX_FIFO:
			if (write)
				ret = w86l488PrvFifoW(wl, val);
			else
				ret = w86l488PrvFifoR(wl, (uint16_t*)buf);
			break;
		
		case REG_NO_INT_STAT_CTRL:
		
			if (write) {
				wl->ints = val &~ 8;	//CD_IE/Cwr_RSP_IE is special
				if (wl->ctrl & 0x10)	//SIEN set -> Cwr_RSP_IE programmed
					wl->intCwrRspIe = (val >> 3) & 1;
				else					//SIEN clear -> CD_IE programmed
					wl->intCdIe = (val >> 3) & 1;
				w86l488PrvRecalcInts(wl);
			}
			else {
				val = wl->ints & 0xfff7;	//CD_IE/Cwr_RSP_IE is special
				if (wl->ctrl & 0x10) {		//SIEN set -> Cwr_RSP_IE read
					if (wl->intCwrRspIe)
						val |= 0x0008;
					if (wl->intCwrRspSta)
						val |= 0x0800;
				}
				else {						//SIEN clear -> CD_IE read
					if (wl->intCdIe)
						val |= 0x0008;
					if (wl->intCdSta)
						val |= 0x0800;
					wl->intCdSta = 0;	//read-clear
				}
			}
			break;
		
		case REG_NO_GPIO:
		
			if (write) {
				
				wl->gpioDirs = val & 0x1f;
				wl->gpioLatches = (val >> 8) & 0x1f;
				w86l488gpioRecalc(wl);
			}
			else {
				val = w86l488PrvGetCurGpioState(wl);
				val |= 0x20;	//cause the real chip does this
				val <<= 8;
				val |= wl->gpioDirs;
			}
			break;
		
		case REG_NO_GPIO_IRQ_CTRL:
			if (write)
				wl->gpioInts = val & 0x1f;
			else {
				
				val = wl->gpioInts | (((uint32_t)wl->gpioIrqSta) << 8);
				wl->gpioIrqSta = 0;
			}
			break;
		
		case REG_NO_INDIRECT_ADDR:
			if (write)
				wl->indAddr = (val & 0x001e) >> 1;
			else
				val = (wl->indAddr << 1) & 0x001e;
			break;
		
		case REG_NO_INDIRECT_DATA:
			return w86l488PrvIndirectAccess(wl, write, (uint16_t*)buf);
			break;
		
		default:
			return false;
	}
	
	static const char *strDirect[] = {
		[REG_NO_CMD_RSP] = "CMD/RSP",
		[REG_NO_STAT_CTRL] = "STAT/CTRL",
		[REG_NO_RX_TX_FIFO] = "RX/TX FIFO",
		[REG_NO_INT_STAT_CTRL] = "INT/+CTRL",
		[REG_NO_GPIO] = "GPIO",
		[REG_NO_GPIO_IRQ_CTRL] = "GPIO IRQ",
		[REG_NO_INDIRECT_ADDR] = "IND ADDR",
		[REG_NO_INDIRECT_DATA] = "IND DATA",
	};
	fprintf(stderr, "WL %c d[%02lx %10s] == 0x%04lx\n", (char)(write ? 'W' : 'R'), (unsigned long)pa, strDirect[pa], (unsigned long)val);
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return ret;
}

struct W86L488* w86l488init(struct ArmMem *physMem, struct SocGpio *gpio, uint32_t base, VSD *vsd, int intPin /* negative for none */)
{
	struct W86L488 *wl = (struct W86L488*)malloc(sizeof(*wl));
	
	if (!wl)
		ERR("cannot alloc W86L488");
	
	memset(wl, 0, sizeof (*wl));
	wl->gpio = gpio;
	wl->vsd = vsd;
	wl->intGpio = intPin;
	
	wl->ctrl = 0x0081;
	wl->sta = 0x0800;
	wl->settings = 0x0041;
	wl->sdio = 0xff00;
	wl->mDataFmt = 0x0200;
	wl->mBlockCnt = 0x0001;
	wl->sDataFmt = 0x0200;
	wl->sBlockCnt = 0x0001;
	wl->nakTO = 0x7fff;
	wl->gpiosInput = 0x19;
	wl->cmdBitsCnt = 0;
	
	if (vsd)
		wl->xtdStatus |= 0x400;
	else
		wl->xtdStatus &=~ 0x400;
	
	if (vsd) {
		//GPIO 0: low when inserted
		wl->gpiosInput &=~ (1 << 0);
	
		//GPIO 1: high when read only
		wl->gpiosInput |= 1 << 1;
		
		//CD bit
		wl->intCdSta = 1;
	}
	
	w86l488PrvRecalcInts(wl);
	if (!memRegionAdd(physMem, base, W86L488_SIZE, w86l488PrvMemAccessF, wl))
		ERR("cannot add W86L488 to MEM\n");
	
	return wl;
}
