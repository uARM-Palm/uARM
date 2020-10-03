//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "ac97dev_WM9705.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"

enum WM9705REG {
	RESET = 0x00,
	VOLMASTER = 0x02,
	VOLHPHONE = 0x04,
	VOLMASTERMONO = 0x06,
	VOLPCBEEP = 0x0a,
	VOLPHONE = 0x0c,
	VOLMIC = 0x0e,
	VOLLINEIN = 0x10,
	VOLCD = 0x12,
	VOLVIDEO = 0x14,
	VOLAUX = 0x16,
	VOLPCMOUT = 0x18,
	RECSELECT = 0x1a,
	VOLRECGAIN = 0x1c,
	
	GENERALPURPOSE = 0x20,
	
	POWERDOWN = 0x26,
	EXTDAUDIO = 0x2a,
	DACRATE = 0x2c,
	ADCRATE = 0x32,
	
	MIXERPATHMUTE = 0x5a,
	ADDFUNCCTL = 0x5c,
	ADDFUNC = 0x74,
	
	DIGI1 = 0x76,
	DIGI2 = 0x78,
	DIGI_RES = 0x7A,
	VID1 = 0x7c,
	VID2 = 0x7e,
};

enum WM9705sampleIdx {
	WM9705sampleIdxNone = 0,
	WM9705sampleIdxX,
	WM9705sampleIdxY,
	WM9705sampleIdxPressure,
	WM9705sampleIdxBmon,
	WM9705sampleIdxAuxAdc,
	WM9705sampleIdxPhone,
	WM9705sampleIdxPcBeep,
};

struct WM9705 {
	
	struct SocAC97 *ac97;
	uint16_t digiRegs[3];
	uint16_t volumes[12];
	uint16_t powerdownReg, extdAudio, dacrate, adcrate, generalPurpose, addtlFuncCtl, recSelect, addFunc, mixerPathMute;
	
	//external stimuli
	uint16_t vAux[4];	//indexed by enum WM9705auxPin
	uint16_t penX, penY, penZ;
	bool penDown;
	
	//for state machine
	bool haveUnreadPenData;
	uint8_t cooIdx, numUnreadDatas;
	uint16_t otherTwo[2];
	
};

static uint16_t* wm9705prvGetVolReg(struct WM9705 *wm, uint32_t regAddr)
{
	static const uint8_t volmap[] = {
		[VOLMASTER] = 0x80 + 0,
		[VOLHPHONE] = 0x80 + 1,
		[VOLMASTERMONO] = 0x80 + 2,
		[VOLPCBEEP] = 0x80 + 3,
		[VOLPHONE] = 0x80 + 4,
		[VOLMIC] = 0x80 + 5,
		[VOLLINEIN] = 0x80 + 6,
		[VOLCD] = 0x80 + 7,
		[VOLVIDEO] = 0x80 + 8,
		[VOLAUX] = 0x80 + 9,
		[VOLPCMOUT] = 0x80 + 10,
		[VOLRECGAIN] = 0x80 + 11,
	};
	
	if (regAddr > sizeof(volmap) / sizeof(*volmap) || !volmap[regAddr])
		return NULL;
	
	return &wm->volumes[volmap[regAddr] - 0x80];
}

static bool wm9705prvCodecRegR(void *userData, uint32_t regAddr, uint16_t *regValP)
{
	enum WM9705REG which = (enum WM9705REG)regAddr;
	struct WM9705 *wm = (struct WM9705*)userData;
	uint16_t val;
	
	fprintf(stderr, "codec read [0x%04x]\n", (unsigned)regAddr);
	
	switch (which) {
		case RESET:
			val = 0x6150;
			break;
		
		case VOLMASTER:
		case VOLHPHONE:
		case VOLMASTERMONO:
		case VOLPCBEEP:
		case VOLPHONE:
		case VOLMIC:
		case VOLLINEIN:
		case VOLCD:
		case VOLVIDEO:
		case VOLAUX:
		case VOLPCMOUT:
		case VOLRECGAIN:
			val = *wm9705prvGetVolReg(wm, which);
			break;
		
		case RECSELECT:
			val = wm->recSelect;
			break;
		
		case GENERALPURPOSE:
			val = wm->generalPurpose;
			break;
		
		case POWERDOWN:
			val = wm->powerdownReg;
			break;
		
		case EXTDAUDIO:
			val = wm->extdAudio;
			break;
		
		case DACRATE:
			val = wm->dacrate;
			break;
		
		case ADCRATE:
			val = wm->adcrate;
			break;
		
		case MIXERPATHMUTE:
			val = wm->mixerPathMute;
			break;
		
		case ADDFUNCCTL:
			val = wm->addtlFuncCtl;
			break;
		
		case ADDFUNC:
			val = wm->addFunc;
			break;
		
		case DIGI1:
			val = wm->digiRegs[0];
			break;
		
		case DIGI2:
			val = wm->digiRegs[1];
			break;
		
		case DIGI_RES:
			val = wm->digiRegs[2];
			wm->haveUnreadPenData = false;
			break;
		
		case VID1:
			val = 0x574D;
			break;
			
		case VID2:
			val = 0x4C05;
			break;
		
		default:
			fprintf(stderr, "unknown reg\n");
			return false;
	}
	
	*regValP = val;
	return true;
}

static bool wm9705prvCodecRegW(void *userData, uint32_t regAddr, uint16_t val)
{
	enum WM9705REG which = (enum WM9705REG)regAddr;
	struct WM9705 *wm = (struct WM9705*)userData;
	
	fprintf(stderr, "codec wri 0x%04x -> [0x%04x]\n", (unsigned)val, (unsigned)regAddr);
		
	switch (regAddr) {
		case VOLMASTER:
		case VOLHPHONE:
		case VOLMASTERMONO:
		case VOLPCBEEP:
		case VOLPHONE:
		case VOLMIC:
		case VOLLINEIN:
		case VOLCD:
		case VOLVIDEO:
		case VOLAUX:
		case VOLPCMOUT:
		case VOLRECGAIN:
			*wm9705prvGetVolReg(wm, which) = val;
			break;
		
		case RECSELECT:
			wm->recSelect = val;
			break;
		
		case GENERALPURPOSE:
			wm->generalPurpose = val;
			break;
		
		case POWERDOWN:
			wm->powerdownReg = val;
			break;
		
		case EXTDAUDIO:
			wm->extdAudio = val;
			break;
			
		case DACRATE:
			wm->dacrate = val;
			break;
		
		case ADCRATE:
			wm->adcrate = val;
			break;
			
		case MIXERPATHMUTE:
			wm->mixerPathMute = val;
			break;
		
		case ADDFUNCCTL:
			wm->addtlFuncCtl = val;
			break;
		
		case ADDFUNC:
			wm->addFunc = val;
			break;
		
		case DIGI1:
			wm->digiRegs[0] = val;
			break;
		
		case DIGI2:
			wm->digiRegs[1] = val;
			break;
		
		default:
			fprintf(stderr, "unknown reg\n");
			return false;
	}

	return true;
}

static bool wm9705prvCodecUnusedRegW(void *userData, uint32_t regAddr, uint16_t val)		//no regs in modem part of this codec - only uses modem slot for touch data
{
	return false;
}

static bool wm9705prvCodecUnusedRegR(void *userData, uint32_t regAddr, uint16_t *regValP)		//no regs in modem part of this codec - only uses modem slot for touch data
{
	return false;
}

struct WM9705* wm9705Init(struct SocAC97* ac97)
{
	struct WM9705 *wm = (struct WM9705*)malloc(sizeof(*wm));
	unsigned i;
	
	if (!wm)
		ERR("cannot alloc WM9705");
	
	memset(wm, 0, sizeof (*wm));
	
	wm->ac97 = ac97;
	wm->powerdownReg = 0x000f;
	
	for (i = 0; i < 4; i++)
		wm->volumes[i] = 0x8000;
	for (; i < 6; i++)
		wm->volumes[i] = 0x8008;
	for (; i < 11; i++)
		wm->volumes[i] = 0x8808;
	wm->volumes[i] = 0x8000;
	
	wm->dacrate = 0xbb80;
	wm->adcrate = 0xbb80;
	
	socAC97clientAdd(ac97, Ac97PrimaryAudio, wm9705prvCodecRegR, wm9705prvCodecRegW, wm);
	socAC97clientAdd(ac97, Ac97SecondaryAudio, wm9705prvCodecUnusedRegR, wm9705prvCodecUnusedRegW, wm);
	socAC97clientAdd(ac97, Ac97PrimaryModem, wm9705prvCodecUnusedRegR, wm9705prvCodecUnusedRegW, wm);
	
	return wm;
}

static void wm9705prvNewAudioPlaybackSample(struct WM9705 *wm, uint32_t samp)
{
	//nothing for now
}

static bool wm9705prvHaveAudioOutSample(struct WM9705 *wm, uint32_t *sampP)
{
	*sampP = 0;
	return true;
}

static bool wm9705prvHaveMicOutSample(struct WM9705 *wm, uint32_t *sampP)
{
	*sampP = 0;
	return true;
}

static uint_fast16_t wm9705prvGetSample(struct WM9705 *wm, enum WM9705sampleIdx which)
{
	uint_fast16_t ret;
	
	switch (which) {
		case WM9705sampleIdxNone:
			ret = 0;
			break;
		
		case WM9705sampleIdxX:
			ret = wm->penX;
			break;
		
		case WM9705sampleIdxY:
			ret = wm->penY;
			break;
		
		case WM9705sampleIdxPressure:
			ret = wm->penZ;
			break;
		
		case WM9705sampleIdxBmon:
			ret = wm->vAux[WM9705auxPinBmon];
			break;
		
		case WM9705sampleIdxAuxAdc:
			ret = wm->vAux[WM9705auxPinAux];
			break;
		
		case WM9705sampleIdxPhone:
			ret = wm->vAux[WM9705auxPinPhone];
			break;
		
		case WM9705sampleIdxPcBeep:
			ret = wm->vAux[WM9705auxPinPcBeep];
			break;
		
		default:
			ret = 0;
			break;
	}
	
	ret &= 0x0fff;
	if (wm->penDown)
		ret |= 0x8000;
	
	ret |= ((uint16_t)which) << 12;
	
	//fprintf(stderr, "%d -> 0x%04x\n", which, ret);
	
	return ret;
}

static bool wm9705prvHaveModemOutSample(struct WM9705 *wm, uint32_t *sampP)
{
	uint_fast16_t retVal;
	
	//if we are not in slot mode, and we have unread data, we cannot proceed and bail out early
	if (!(wm->digiRegs[0] & 0x08) && wm->haveUnreadPenData && (wm->digiRegs[1] & 0x0100))
		return false;
	
	//if we are in slot mode but any slot except 5 is selected, provide no data - pxa255 cnanot get it anyways, plus this is callback for modem data (slot5)
	if ((wm->digiRegs[0] & 0x08) && (wm->digiRegs[0] & 0x07))
		return false;
	
	//if we have unread data, send it and do nothing else
	if (wm->numUnreadDatas)
		retVal = wm->otherTwo[2 - wm->numUnreadDatas--];
	//else, if needed do a sampling (poll is set or continuous is set and pdet is off or pen is down
	else if (wm->digiRegs[0] & 0x8000 || wm->penDown || !(wm->digiRegs[1] & 0x1000)) {
		
		enum WM9705sampleIdx addrIdx = (enum WM9705sampleIdx)((wm->digiRegs[0] >> 12) & 7);
		
		//clear poll immediately
		wm->digiRegs[0] &=~ 0x8000;
		
		//see if we need a set
		if (wm->digiRegs[0] & 0x0800) {
			
			uint_fast16_t y;
			
			//get x
			retVal = wm9705prvGetSample(wm, WM9705sampleIdxX);
			
			//get y
			y = wm9705prvGetSample(wm, WM9705sampleIdxY);
			
			//get third if needed
			if (addrIdx != WM9705sampleIdxNone) {
				
				wm->otherTwo[0] = y;
				wm->otherTwo[1] = wm9705prvGetSample(wm, addrIdx);
				wm->numUnreadDatas = 2;
			}
			else {
				wm->otherTwo[1] = y;
				wm->numUnreadDatas = 1;
			}
		}
		//else read one thing (even if "none")
		else {
			
			retVal = wm9705prvGetSample(wm, addrIdx);
		}
	}
	//else no data
	else
		return false;
	
	
	//provide ret val (not necessarily in the slot)
	if (wm->digiRegs[0] & 0x08) {
		//fprintf(stderr, "touch reply 0x%04x (slot)\n", retVal);
		*sampP = retVal;
		return true;
	}
	
	wm->digiRegs[2] = retVal;
	return false;
}

void wm9705periodic(struct WM9705 *wm)
{
	uint32_t val;
	
	if (socAC97clientClientWantData(wm->ac97, Ac97PrimaryAudio, &val))
		wm9705prvNewAudioPlaybackSample(wm, val);
	
	if (wm9705prvHaveAudioOutSample(wm, &val))
		socAC97clientClientHaveData(wm->ac97, Ac97PrimaryAudio, val);
	
	if (wm9705prvHaveMicOutSample(wm, &val))
		socAC97clientClientHaveData(wm->ac97, Ac97SecondaryAudio, val);
	
	if (wm9705prvHaveModemOutSample(wm, &val))
		socAC97clientClientHaveData(wm->ac97, Ac97PrimaryModem, val);
}

void wm9705setAuxVoltage(struct WM9705 *wm, enum WM9705auxPin which, uint32_t mV)
{
	//vref is 3.3V
	
	if (mV > 3300)
		mV = 0xfff;
	else
		mV = (mV * 4095 + 3300 / 2) / 3300;
	
	wm->vAux[which] = mV;
}

void wm9705setPen(struct WM9705 *wm, int16_t x, int16_t y, int16_t press)		//raw ADC values, negative for pen up
{
	wm->penDown = x >= 0 && y >= 0 && press >= 0;
	if (wm->penDown) {
		wm->penX = x;
		wm->penY = y;
		wm->penZ = press;
	}
}
