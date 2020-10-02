//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "omap_Misc.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"


#define OMAP_DSP_CLKM_BASE			0xE1008000UL
#define OMAP_DSP_CLKM_SIZE			2048

#define OMAP_OMAP_CFG_BASE			0xFFFE1000UL
#define OMAP_OMAP_CFG_SIZE			256

#define OMAP_MPUI_BASE				0xFFFEC900UL	//access to the dsp from mpu
#define OMAP_MPUI_SIZE				256

#define OMAP_TRAFFIC_CTRL_BASE		0xFFFECC00UL
#define OMAP_TRAFFIC_CTRL_SIZE		256

#define OMAP_MPU_CLK_CTRL_BASE		0xFFFECE00UL
#define OMAP_MPU_CLK_CTRL_SIZE		256

#define OMAP_DPLL_1_BASE			0xFFFECF00UL
#define OMAP_DPLL_1_SIZE			256

#define OMAP_IDCODE_BASE			0xFFFED404UL
#define OMAP_IDCODE_SIZE			4


struct OmapMisc {
	
	//MPU CLOCK CONTROL
	struct {
		uint16_t CKCTL, IDLECT1, IDLECT2, EWUPCT, RSTCT1, RSTCT2, SYSST;
	} mpu;
	
	//DPLL1
	struct {
		uint16_t CTL_REG;
	} dpll;
	
	//OMAP config
	struct {
		uint32_t FUNC_MUX_CTRL[14], COMP_MODE_CTRL, PULL_DWN_CTRL[4], GATE_INH_CTRL, VOLTAGE_CTRL, TEST_DBG_CTRL, MOD_CONF_CTRL;
	} omapCfg;
	
	//TRAFFIC CONTROLLER
	struct {
		uint32_t EMIFS_CSx_CONFIG[4], EMIFF_SDRAM_CONFIG, TIMEOUT[2];
		uint16_t EMIFF_MRS;
		uint8_t EMIFS_CONFIG_REG, ENDIANISM, EMIFF_SDRAM_CONFIG_2, EMIFS_CFG_DYN_WAIT;
	} trafficCfg;
	
	//DSP CLKM
	struct {
		uint8_t RSTCT2;
		uint16_t IDLECT[2], SYSST;
		
		//guessed, uncodumented
		uint16_t CKCTL;
	} dspClkm;
	
	//MPUI
	struct {
		uint32_t CTRL_REG;
		uint16_t DSP_API_CONFIG;
		uint8_t DSP_BOOT_CONFIG;
	} mpui;
};

static bool omapMiscPrvDpllMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMisc* misc = (struct OmapMisc*)userData;
	uint32_t val = 0;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_DPLL_1_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				misc->dpll.CTL_REG = val | 0x0001;	//always locked
			else
				val = misc->dpll.CTL_REG;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

bool omapMiscIsTimerAtCpuSpeed(struct OmapMisc *misc)
{
	return (misc->mpu.CKCTL >> 12) & 1;
}

static bool omapMiscPrvMpuClkCtrlMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMisc* misc = (struct OmapMisc*)userData;
	uint32_t val = 0;
	
	if ((size != 2 && size != 4) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_MPU_CLK_CTRL_BASE) >> 2;
	
	if (write)
		val = (size == 2) ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				misc->mpu.CKCTL = val;
			else
				val = misc->mpu.CKCTL;
			break;
		
		case 0x04 / 4:
			if (write)
				misc->mpu.IDLECT1 = val;
			else
				val = misc->mpu.IDLECT1;
			break;
		
		case 0x08 / 4:
			if (write)
				misc->mpu.IDLECT2 = val;
			else
				val = misc->mpu.IDLECT2;
			break;
		
		case 0x0c / 4:
			if (write)
				misc->mpu.EWUPCT = val;
			else
				val = misc->mpu.EWUPCT;
			break;
		
		case 0x10 / 4:
			if (write) {
				
				misc->mpu.RSTCT1 = val;
				if (val & 0x0009)
					ERR("MPU has reset itself\n");
			}
			else
				val = misc->mpu.RSTCT1;
			break;
		
		case 0x14 / 4:
			if (write)
				misc->mpu.RSTCT2 = val;
			else
				val = misc->mpu.RSTCT2;
			break;
		
		case 0x18 / 4:
			if (write)
				misc->mpu.SYSST = val;
			else
				val = misc->mpu.SYSST;
			break;
		
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	
	return true;
}

static bool omapMiscPrvOmapCfgMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMisc* misc = (struct OmapMisc*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_OMAP_CFG_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
		case 0x04 / 4:
		case 0x08 / 4:
			if (write)
				misc->omapCfg.FUNC_MUX_CTRL[pa - 0x00 / 4] = val;
			else
				val = misc->omapCfg.FUNC_MUX_CTRL[pa - 0x00 / 4];
			break;
		
		case 0x0c / 4:
			if (write)
				misc->omapCfg.COMP_MODE_CTRL = val;
			else
				val = misc->omapCfg.COMP_MODE_CTRL;
			break;
		
		case 0x10 / 4:
		case 0x14 / 4:
		case 0x18 / 4:
		case 0x1c / 4:
		case 0x20 / 4:
		case 0x24 / 4:
		case 0x28 / 4:
		case 0x2c / 4:
		case 0x30 / 4:
		case 0x34 / 4:
		case 0x38 / 4:
			if (write)
				misc->omapCfg.FUNC_MUX_CTRL[pa - 0x00 / 4 - 1] = val;
			else
				val = misc->omapCfg.FUNC_MUX_CTRL[pa - 0x00 / 4 - 1];
			break;
		
		case 0x40 / 4:
		case 0x44 / 4:
		case 0x48 / 4:
		case 0x4c / 4:
			if (write)
				misc->omapCfg.PULL_DWN_CTRL[pa - 0x00 / 4 - 1] = val;
			else
				val = misc->omapCfg.PULL_DWN_CTRL[pa - 0x00 / 4 - 1];
			break;
		
		case 0x50 / 4:
			if (write)
				misc->omapCfg.GATE_INH_CTRL = val;
			else
				val = misc->omapCfg.GATE_INH_CTRL;
			break;
		
		case 0x60 / 4:
			if (write)
				misc->omapCfg.VOLTAGE_CTRL = val;
			else
				val = misc->omapCfg.VOLTAGE_CTRL;
			break;
		
		case 0x70 / 4:
			if (write)
				misc->omapCfg.TEST_DBG_CTRL = val;
			else
				val = misc->omapCfg.TEST_DBG_CTRL;
			break;
		
		case 0x80 / 4:
			if (write)
				misc->omapCfg.MOD_CONF_CTRL = val;
			else
				val = misc->omapCfg.MOD_CONF_CTRL;
			break;
		
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

static bool omapMiscPrvDspClkmMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMisc* misc = (struct OmapMisc*)userData;
	uint_fast16_t val = 0;
	
	if (size != 2) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_DSP_CLKM_BASE) >> 2;
	
	if (write)
		val = *(uint16_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				misc->dspClkm.CKCTL = val;
			else
				val = misc->dspClkm.CKCTL;
			break;
		
		case 0x04 / 4:
			if (write)
				misc->dspClkm.IDLECT[0] = val & 0x01e0;
			else
				val = misc->dspClkm.IDLECT[0];
			break;
		
		case 0x08 / 4:
			if (write)
				misc->dspClkm.IDLECT[1] = val & 0x0103;
			else
				val = misc->dspClkm.IDLECT[1];
			break;
		
		case 0x14 / 4:
			if (write)
				misc->dspClkm.RSTCT2 = val & 1;
			else
				val = misc->dspClkm.RSTCT2;
			break;
		
		case 0x18 / 4:
			if (write)
				misc->dspClkm.SYSST = val & 0x38ff;
			else
				val = misc->dspClkm.SYSST;
			break;
			
		default:
			return false;
	}
	
	if (!write)
		*(uint16_t*)buf = val;
	
	return true;
}

static bool omapMiscPrvMpuiMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMisc* misc = (struct OmapMisc*)userData;
	uint32_t val = 0;
	
	if ((size != 4 && size != 2) || (pa & 3)) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_MPUI_BASE) >> 2;
	
	if (write)
		val = size == 2 ? *(uint16_t*)buf : *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
			if (write)
				misc->mpui.CTRL_REG = val & 0x007fffff;
			else
				val = misc->mpui.CTRL_REG;
			break;
			
		case 0x04 / 4:	//DEBUG_ADDR
		case 0x08 / 4:	//DEBUG_DATA
		case 0x0c / 4:	//DEBUG_FLAG
			if (write)
				return false;
			else
				val = 0x00ffffff;
			break;
			
		case 0x10 / 4:	//STATUS_REG
			if (write)
				return false;
			else
				val = 0x00001fff;
			break;
		
		case 0x14 / 4:
			if (write)
				return false;
			else
				val = 0x00000fff;
			break;
		
		case 0x18 / 4:
			if (write)
				misc->mpui.DSP_BOOT_CONFIG = val & 0x0f;
			else
				val = misc->mpui.DSP_BOOT_CONFIG;
			break;
			
		case 0x1c / 4:
			if (write)
				misc->mpui.DSP_API_CONFIG = val;
			else
				val = misc->mpui.DSP_API_CONFIG;
			break;
			
		default:
			return false;
	}
	
	if (!write) {
		if (size == 2)
			*(uint16_t*)buf = val;
		else
			*(uint32_t*)buf = val;
	}
	return true;
}

static bool omapMiscPrvTrafficCtrlMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct OmapMisc* misc = (struct OmapMisc*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08x\n", __func__, write ? "write" : "read", size, pa);
		return false;
	}
	
	pa = (pa - OMAP_TRAFFIC_CTRL_BASE) >> 2;
	
	if (write)
		val = *(uint32_t*)buf;
	
	switch (pa) {
		
		case 0x00 / 4:
		case 0x04 / 4:
		case 0x08 / 4:
			if (write)
				;	//ignored
			else
				val = 0;
			break;
		
		case 0x0C / 4:
			if (write)
				misc->trafficCfg.EMIFS_CONFIG_REG = (val & 0x07) | (misc->trafficCfg.EMIFS_CONFIG_REG & 8);		//flahs is always ready
			else
				val = misc->trafficCfg.EMIFS_CONFIG_REG;
			break;
		
		case 0x10 / 4:
		case 0x14 / 4:
		case 0x18 / 4:
		case 0x1C / 4:
			if (write)
				misc->trafficCfg.EMIFS_CSx_CONFIG[pa - 0x10 / 4] = val & 0x0037fff7;
			else
				val = misc->trafficCfg.EMIFS_CSx_CONFIG[pa - 0x10 / 4];
			break;
		
		case 0x20 / 4:
			if (write)
				misc->trafficCfg.EMIFF_SDRAM_CONFIG = val & 0x0fffffff;
			else
				val = misc->trafficCfg.EMIFF_SDRAM_CONFIG;
			break;
		
		case 0x24 / 4:
			if (write)
				misc->trafficCfg.EMIFF_MRS = val & 0x027f;
			else
				val = misc->trafficCfg.EMIFF_MRS;
			break;
		
		case 0x28 / 4:
		case 0x2C / 4:
			if (write)
				misc->trafficCfg.TIMEOUT[pa - 0x28 / 4] = val & 0x00ff00ff;
			else
				val = misc->trafficCfg.TIMEOUT[pa - 0x28 / 4];
			break;
		
		//case 0x30 / 4:		//TIMEOUT3 reg
		//	return false;
			
		case 0x34 / 4:
			if (write)
				misc->trafficCfg.ENDIANISM = val & 0x03;
			else
				val = misc->trafficCfg.ENDIANISM;
			break;
		
		case 0x3C / 4:
			if (write)
				misc->trafficCfg.EMIFF_SDRAM_CONFIG_2 = val & 0x03;
			else
				val = misc->trafficCfg.EMIFF_SDRAM_CONFIG_2;
			break;
		
		case 0x40 / 4:
			if (write)
				misc->trafficCfg.EMIFS_CFG_DYN_WAIT = val & 0x0f;
			else
				val = misc->trafficCfg.EMIFS_CFG_DYN_WAIT;
			break;
			
		default:
			return false;
	}
	
	if (!write)
		*(uint32_t*)buf = val;
	
	return true;
}

static bool omapMiscPrvIdcodeMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	if (pa == OMAP_IDCODE_BASE && size == 4 && !write) {
		*(uint32_t*)buf = 0x1b47002f;
		return true;
	}
	
	return false;
}

struct OmapMisc* omapMiscInit(struct ArmMem *physMem)
{
	struct OmapMisc *misc = (struct OmapMisc*)malloc(sizeof(*misc));
	
	if (!misc)
		ERR("cannot alloc MISC stuff\n");
	
	memset(misc, 0, sizeof (*misc));
	
	//MPU CLK CTRL
	misc->mpu.CKCTL = 0x3000;
	misc->mpu.IDLECT1 = 0x0400;
	misc->mpu.IDLECT2 = 0x0100;
	misc->mpu.EWUPCT = 0x003f;
	misc->mpu.SYSST = 0x0038;
	
	//DPLL
	misc->dpll.CTL_REG = 0x2002;
	
	//TRAFFIC CONTROLLER
	misc->trafficCfg.EMIFS_CONFIG_REG = 0x08;
	misc->trafficCfg.EMIFS_CSx_CONFIG[0] = 0x0000fffbul;
	misc->trafficCfg.EMIFS_CSx_CONFIG[1] = 0x0010fffbul;
	misc->trafficCfg.EMIFS_CSx_CONFIG[2] = 0x0010fffbul;
	misc->trafficCfg.EMIFS_CSx_CONFIG[3] = 0x0000fffbul;
	misc->trafficCfg.EMIFF_SDRAM_CONFIG = 0x00618800ul;
	misc->trafficCfg.EMIFF_MRS = 0x0037;
	misc->trafficCfg.EMIFF_SDRAM_CONFIG_2 = 0x03;
	
	//MPUI
	misc->mpui.CTRL_REG = 0x0003ff1f;
	misc->mpui.DSP_API_CONFIG = 0xffff;

	
	if (!memRegionAdd(physMem, OMAP_MPU_CLK_CTRL_BASE, OMAP_MPU_CLK_CTRL_SIZE, omapMiscPrvMpuClkCtrlMemAccessF, misc))
		ERR("cannot add MPU CLOCK CTRL to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_DPLL_1_BASE, OMAP_DPLL_1_SIZE, omapMiscPrvDpllMemAccessF, misc))
		ERR("cannot add DPLL1 to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_OMAP_CFG_BASE, OMAP_OMAP_CFG_SIZE, omapMiscPrvOmapCfgMemAccessF, misc))
		ERR("cannot add OMAP CFG to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_TRAFFIC_CTRL_BASE, OMAP_TRAFFIC_CTRL_SIZE, omapMiscPrvTrafficCtrlMemAccessF, misc))
		ERR("cannot add TRAFFIC CTRL to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_IDCODE_BASE, OMAP_IDCODE_SIZE, omapMiscPrvIdcodeMemAccessF, misc))
		ERR("cannot add IDCODE to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_DSP_CLKM_BASE, OMAP_DSP_CLKM_SIZE, omapMiscPrvDspClkmMemAccessF, misc))
		ERR("cannot add DSK CLKM to MEM\n");
	
	if (!memRegionAdd(physMem, OMAP_MPUI_BASE, OMAP_MPUI_SIZE, omapMiscPrvMpuiMemAccessF, misc))
		ERR("cannot add MPUI to MEM\n");
	
	return misc;
}

