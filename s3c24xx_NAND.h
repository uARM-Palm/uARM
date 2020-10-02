//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_NAND_H_
#define _S3C24XX_NAND_H_

#include "soc_GPIO.h"
#include "soc_IC.h"
#include "nand.h"
#include "mem.h"

struct S3C24xxNand;


struct S3C24xxNand* s3c24xxNandInit(struct ArmMem *physMem, struct NAND *nandChip, struct SocIc *ic, struct SocGpio *gpio);
void s3c24xxNandPeriodic(struct S3C24xxNand* nand);





#endif
