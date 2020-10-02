//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_SDIO_H_
#define _S3C24XX_SDIO_H_

#include "soc_DMA.h"
#include "soc_IC.h"
#include "nand.h"
#include "mem.h"
#include "vSD.h"

struct S3C24xxSdio;


struct S3C24xxSdio* s3c24xxSdioInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma, uint_fast8_t socRev);

void s3c24xxSdioInsert(struct S3C24xxSdio *sdio, struct VSD* vsd);	//NULL also acceptable



#endif
