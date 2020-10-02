//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA255_IC_H_
#define _PXA255_IC_H_

#include "soc_IC.h"

#define OMAP_LV1_MIN			0
#define OMAP_LV1_MAX			31

#define OMAP_LV2_MIN			32
#define OMAP_LV2_MAX			63


#define OMAP_LV1_IRQ(x)			(OMAP_LV1_MIN + (x))
#define OMAP_LV2_IRQ(x)			(OMAP_LV2_MIN + (x))

//level1 irq #s (0-based)
#define OMAP_I_LEVEL_2			OMAP_LV1_IRQ(0)
#define OMAP_I_CAMERA			OMAP_LV1_IRQ(1)
#define OMAP_I_EXT_FIQ			OMAP_LV1_IRQ(3)	//edge
#define OMAP_I_McBSP_2_TX		OMAP_LV1_IRQ(4)	//edge
#define OMAP_I_McBSP_2_RX		OMAP_LV1_IRQ(5)	//edge
#define OMAP_I_DSP_MMU_ABT		OMAP_LV1_IRQ(7)
#define OMAP_I_HOST_INT			OMAP_LV1_IRQ(8)
#define OMAP_I_ABORT			OMAP_LV1_IRQ(9)
#define OMAP_I_DSP_MAILBOX_1	OMAP_LV1_IRQ(10)
#define OMAP_I_DSP_MAILBOX_2	OMAP_LV1_IRQ(11)
#define OMAP_I_TIBP_BR_PRIV		OMAP_LV1_IRQ(13)
#define OMAP_I_GPIO				OMAP_LV1_IRQ(14)
#define OMAP_I_UART3			OMAP_LV1_IRQ(15)
#define OMAP_I_TIMER_3			OMAP_LV1_IRQ(16)
#define OMAP_I_LB_MMU			OMAP_LV1_IRQ(17)
#define OMAP_I_DMA_CH0_CH6		OMAP_LV1_IRQ(19)
#define OMAP_I_DMA_CH1_CH7		OMAP_LV1_IRQ(20)
#define OMAP_I_DMA_CH2_CH8		OMAP_LV1_IRQ(21)
#define OMAP_I_DMA_CH3			OMAP_LV1_IRQ(22)
#define OMAP_I_DMA_CH4			OMAP_LV1_IRQ(23)
#define OMAP_I_DMA_CH5			OMAP_LV1_IRQ(24)
#define OMAP_I_DMA_CH_LCD		OMAP_LV1_IRQ(25)
#define OMAP_I_TIMER_1			OMAP_LV1_IRQ(26)	//edge
#define OMAP_I_WDT				OMAP_LV1_IRQ(27)
#define OMAP_I_TIBP_BR_PUB		OMAP_LV1_IRQ(28)
#define OMAP_I_LOLCA_BUS_IF		OMAP_LV1_IRQ(29)
#define OMAP_I_TIMER_2			OMAP_LV1_IRQ(30)
#define OMAP_I_LCD				OMAP_LV1_IRQ(31)

//level 2 irqs (32-based)
#define OMAP_I_FAC				OMAP_LV2_IRQ(0)
#define OMAP_I_KEYBOARD			OMAP_LV2_IRQ(1)		//edge
#define OMAP_I_uWIRE_TX			OMAP_LV2_IRQ(2)		//edge
#define OMAP_I_uWIRE_RX			OMAP_LV2_IRQ(3)		//edge
#define OMAP_I_I2C				OMAP_LV2_IRQ(4)		//edge
#define OMAP_I_MPUIO			OMAP_LV2_IRQ(5)
#define OMAP_I_US_HHC_1			OMAP_LV2_IRQ(6)
#define OMAP_I_McBSP_3_TX		OMAP_LV2_IRQ(10)	//edge
#define OMAP_I_McBSP_3_RX		OMAP_LV2_IRQ(11)	//edge
#define OMAP_I_McBSP_1_TX		OMAP_LV2_IRQ(12)	//edge
#define OMAP_I_McBSP_1_RX		OMAP_LV2_IRQ(13)	//edge
#define OMAP_I_UART1			OMAP_LV2_IRQ(14)
#define OMAP_I_UART2			OMAP_LV2_IRQ(15)
#define OMAP_I_MCSI1			OMAP_LV2_IRQ(16)
#define OMAP_I_MCSI2			OMAP_LV2_IRQ(17)
#define OMAP_I_USB_FUNC_GEN		OMAP_LV2_IRQ(20)
#define OMAP_I_1_WIRE			OMAP_LV2_IRQ(21)
#define OMAP_I_TIMER32K			OMAP_LV2_IRQ(22)	//edge
#define OMAP_I_MMC				OMAP_LV2_IRQ(23)
#define OMAP_I_ULPD				OMAP_LV2_IRQ(24)
#define OMAP_I_RTC_TICK			OMAP_LV2_IRQ(25)	//edge
#define OMAP_I_RTC_ALM			OMAP_LV2_IRQ(26)
#define OMAP_I_DSP_MMU			OMAP_LV2_IRQ(28)
#define OMAP_I_USB_FUNC_ISO		OMAP_LV2_IRQ(29)
#define OMAP_I_USB_FUNC_NONISO	OMAP_LV2_IRQ(30)
#define OMAP_I_McBSP2_OVERFLOW	OMAP_LV2_IRQ(31)



#endif

