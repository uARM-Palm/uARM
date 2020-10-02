//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_UART_H_
#define _OMAP_UART_H_

#include "mem.h"
#include "CPU.h"
#include "soc_UART.h"


/*
	OMAP UARTs
	
	They are identical, but at diff base addresses. this implements one. instantiate more than one to make all 3 work if needed.

	by default we read nothing and write nowhere (buffer drains fast into nothingness)
	this can be changed by adding appropriate callbacks

*/

#define OMAP_UART1_BASE		0xFFFB0000UL
#define OMAP_UART2_BASE		0xFFFB0800UL
#define OMAP_UART3_BASE		0xFFFB9800UL		//supports IR




#endif

