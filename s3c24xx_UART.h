//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_UART_H_
#define _S3C24XX_UART_H_

#include "mem.h"
#include "CPU.h"
#include "soc_UART.h"


/*
	S3CXX UARTs
	
	They are identical, but at diff base addresses. this implements one. instantiate more than one to make all 3 work if needed.

	by default we read nothing and write nowhere (buffer drains fast into nothingness)
	this can be changed by adding appropriate callbacks

*/

#define S3C24XX_UART0_BASE		0x50000000UL
#define S3C24XX_UART1_BASE		0x50004000UL
#define S3C24XX_UART2_BASE		0x50008000UL




#endif

