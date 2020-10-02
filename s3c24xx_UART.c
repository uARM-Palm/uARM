//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "s3c24xx_UART.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define S3C24XX_UART_SIZE			0x30

#define IRQ_ADD_ERROR				0
#define IRQ_ADD_TX					1
#define IRQ_ADD_RX					2

#define FIFO_SIZE					16

#define FLAG_FRAME_ERR				0x04
#define FLAG_OVERRUN_RR				0x01


struct FifoItem {
	uint8_t val;
	uint8_t flags;
};

struct Fifo {
	struct FifoItem item[FIFO_SIZE];
	uint8_t nItems;
};

struct SocUart {

	struct SocIc *ic;
	uint32_t baseAddr;
	uint8_t irqBase;
	
	SocUartReadF readF;
	SocUartWriteF writeF;
	void* accessFuncsData;
	
	struct Fifo rxFifo, txFifo;
	
	uint16_t ucon, ubrdiv;
	uint8_t ulcon, ufcon, umcon, utrstat, uerstat;
};


static void socUartPrvRecalc(struct SocUart* uart)
{
	//todo
	
	//XXX: DMA
	
	socIcInt(uart->ic, uart->irqBase + IRQ_ADD_ERROR, false);
	socIcInt(uart->ic, uart->irqBase + IRQ_ADD_RX, false);
	socIcInt(uart->ic, uart->irqBase + IRQ_ADD_TX, false);
}


static bool socUartPrvTx(struct SocUart *uart, uint_fast8_t val)
{
	return true;
}

static bool socUartPrvRx(struct SocUart *uart, uint32_t *valOutP)
{
	*valOutP = 0;
	
	return true;
}

static bool socUartPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct SocUart *uart = (struct SocUart*)userData;
	uint32_t val = 0;
	
	if ((size != 4 && size != 1) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - uart->baseAddr) >> 2;

	if (write)
		val = (size == 4) ? *(uint32_t*)buf : *(uint8_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				uart->ulcon = val & 0x7f;
			else
				val = uart->ulcon;
			break;
		
		case 0x04 / 4:
			if (write)
				uart->ucon = val & 0x07ef;
			else
				val = uart->ucon;
			break;
		
		case 0x08 / 4:
			if (write) {
				uart->ufcon = val & 0xf1;
				if (val & 2)
					uart->rxFifo.nItems = 0;
				if (val & 4)
					uart->txFifo.nItems = 0;
			}
			else
				val = uart->ufcon;
			break;
		
		case 0x0c / 4:
			if (write)
				uart->umcon = val & 0x11;
			else
				val = uart->umcon;
			break;
		
		case 0x10 / 4:
			if (write)
				return false;
			else
				val = uart->utrstat;
			break;
		
		case 0x14 / 4:
			if (write)
				return false;
			else
				val = uart->uerstat;
			break;
		
		case 0x18 / 4:
			if (write)
				return false;
			else
				val = (uart->txFifo.nItems == FIFO_SIZE ? 0x200 : 0) |
						(uart->rxFifo.nItems == FIFO_SIZE ? 0x100 : 0) |
						((uart->txFifo.nItems & 0x0f) << 4) |
						(uart->rxFifo.nItems & 0x0f);
			break;
		
		case 0x1c / 4:
			if (write)
				return false;
			else
				val = 0;	//CTS always low
			break;
		
		case 0x20 / 4:
			if (write)
				return socUartPrvTx(uart, val);
			else
				return false;
			break;
		
		case 0x24 / 4:
			if (write)
				return false;
			else
				return socUartPrvRx(uart, &val);
			break;
		
		case 0x28 / 4:
			if (write)
				uart->ubrdiv = val & 0xffff;
			else
				val = uart->ubrdiv;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 4)
			*(uint32_t*)buf = val;
		else
			*(uint8_t*)buf = val;
	}
	
	socUartPrvRecalc(uart);
	
	return true;
}


struct SocUart* socUartInit(struct ArmMem *physMem, struct SocIc *ic, uint32_t baseAddr, uint8_t irqBase)
{
	struct SocUart *uart = (struct SocUart*)malloc(sizeof(*uart));
	
	if (!uart)
		ERR("cannot alloc UART at 0x%08x", baseAddr);
	
	memset(uart, 0, sizeof (*uart));
	
	uart->baseAddr = baseAddr;
	uart->irqBase = irqBase;
	uart->ic = ic;
	
	uart->utrstat = 0x06;
	
	if (!memRegionAdd(physMem, baseAddr, S3C24XX_UART_SIZE, socUartPrvMemAccessF, uart))
		ERR("cannot add UART at 0x%08x to MEM\n", baseAddr);
	
	socUartPrvRecalc(uart);
	
	return uart;
}

static uint_fast16_t socUartPrvDefaultRead(void *userData)							//these are special funcs since they always get their own userData - the uart pointer :)
{
	(void)userData;
	
	return UART_CHAR_NONE;	//we read nothing..as always
}

static void socUartPrvDefaultWrite(uint_fast16_t chr, void *userData)				//these are special funcs since they always get their own userData - the uart pointer :)
{
	(void)chr;
	(void)userData;
	
	//nothing to do here
}

void socUartSetFuncs(struct SocUart *uart, SocUartReadF readF, SocUartWriteF writeF, void *userData)
{
	if (!readF)
		readF = socUartPrvDefaultRead;		//these are special funcs since they get their own private data - the uart :)
	if (!writeF)
		writeF = socUartPrvDefaultWrite;
	
	uart->readF = readF;
	uart->writeF = writeF;
	uart->accessFuncsData = userData;
}

void socUartProcess(struct SocUart *uart)
{
	//todo
}


