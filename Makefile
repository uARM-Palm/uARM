APP	= uARM
CC	= gcc
LD	= gcc


#PGO: first compile with --profile-generate, then run to completion, then compile with --profile-use


OPT			= -fomit-frame-pointer -momit-leaf-frame-pointer -Ofast -flto #--profile-use #--profile-generate
#OPT			= -O0 -ffunction-sections -Wl,--gc-sections


COMMON		= $(OPT) -g -ggdb -ggdb3 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
CCFLAGS		= $(COMMON) -D_FILE_OFFSET_BITS=64 -DGDB_STUB_ENABLED -DSDL_ENABLED
LDFLAGS		= $(COMMON) -lSDL2

#main
PROGRAM		+= main_pc.o CPU.o MMU.o cp15.o mem.o RAM.o ROM.o icache.o gdbstub.o vSD.o keys.o palmoscalls.o

#PXA2xx
PXA2XX		+= socPXA.o pxa_IC.o pxa_MMC.o
PXA2XX		+= pxa_TIMR.o pxa_RTC.o pxa_UART.o pxa_PwrClk.o pxa_I2S.o
PXA2XX		+= pxa_GPIO.o pxa_DMA.o pxa_LCD.o pxa_PWM.o pxa_AC97.o
PXA2XX		+= pxa_MemCtrl.o pxa_I2C.o pxa_SSP.o pxa255_UDC.o pxa270_UDC.o
PXA2XX		+= pxa255_DSP.o pxa270_IMC.o pxa270_KPC.o pxa270_WMMX.o

#omap
OMAP		+= socOMAP.o omap_Camera.o
OMAP		+= omap_GPIO.o omap_IC.o omap_ULPD.o omap_Misc.o omap_WDT.o omap_LCD.o
OMAP		+= omap_uWire.o omap_TMR.o omap_RTC.o omap_32kTMR.o omap_DMA.o omap_PWL.o
OMAP		+= omap_USB.o omap_UART.o omap_PWT.o omap_McBSP.o omap_MMC.o omap_I2C.o

#S3C24xx
S3C24XX		+= socS3C24xx.o s3c24xx_GPIO.o s3c24xx_IC.o s3c24xx_WDT.o s3c24xx_PwrClk.o
S3C24XX		+= s3c24xx_MemCtrl.o s3c24xx_TMR.o s3c24xx_LCD.o s3c24xx_UART.o s3c24xx_USB.o
S3C24XX		+= s3c24xx_ADC.o s3c24xx_RTC.o s3c24xx_SDIO.o

#S3C2410
S3C2410		+= $(S3C24XX) s3c2410_NAND.o

#S3C2440
S3C2440		+= $(S3C24XX) s3c2440_NAND.o

#entirely broken devices
#DEVICE		+= $(OMAP) devicePalmTungstenT.o uwiredev_ADS7846.o

#nonworking touch
#DEVICE		+= $(PXA2XX) devicePalmZire72.o ac97dev_WM9712L.o -DSUPPORT_Z72_PRINTF
#DEVICE		+= $(PXA2XX) devicePalmTX.o mmiodev_DirectNAND.o nand.o ac97dev_WM9712L.o mmiodev_TxNoramMarker.o

#mostly working devices
#DEVICE		+= $(OMAP) devicePalmTungstenE.o sspdev_TSC210x.o
#DEVICE		+= $(PXA2XX) deviceDellAximX3.o mmiodev_AximX3cpld.o mmiodev_W86L488.o ac97dev_WM9705.o -DSUPPORT_AXIM_PRINTF
#DEVICE		+= $(OMAP) devicePalmZireXYZ.o sspdev_TSC210x.o
#DEVICE		+= $(S3C2440) deviceAceecaPDA32.o nand.o
#DEVICE		+= $(PXA2XX) devicePalmTungstenC.o ac97dev_UCB1400.o

#working devices
DEVICE		+= $(PXA2XX) devicePalmTungstenT3.o sspdev_TSC210x.o i2cdev_TPS65010.o mmiodev_W86L488.o
#DEVICE		+= $(PXA2XX) devicePalmTungstenE2.o mmiodev_DirectNAND.o nand.o ac97dev_WM9712L.o
#DEVICE		+= $(PXA2XX) devicePalmZire31.o ac97dev_WM9712L.o
#DEVICE		+= $(PXA2XX) deviceSonyTG50.o mmiodev_TG50uc.o sspdev_AD7873.o mmiodev_MemoryStickController.o i2cdev_AN32502A.o i2sdev_AK4534.o
#DEVICE		+= $(OMAP) devicePalmZire21.o uwiredev_ADS7846.o
#DEVICE		+= $(OMAP) devicePalmZire71.o uwiredev_ADS7846.o
#DEVICE		+= $(S3C2410) devicePalmZ22.o nand.o



OBJS		= $(patsubst -D%,,$(DEVICE)) $(PROGRAM)
DFLAGS		= $(patsubst %.o,,$(DEVICE))

HFILES		= $(wildcard *.h)

$(APP): $(OBJS) $(HFILES)
	$(LD) -o $(APP) $(OBJS) $(LDFLAGS) $(DFLAGS)
	$(EXTRA)

%.o: %.c $(HFILES) Makefile
	$(CC) $(CCFLAGS) $(DFLAGS) -o $@ -c $<

clean:
	rm -f $(APP) $(OBJS)


