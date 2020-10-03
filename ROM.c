//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include <string.h>
#include <stdlib.h>
#include <endian.h>
#include <stdio.h>
#include "util.h"
#include "mem.h"
#include "ROM.h"


struct ArmRomPiece {
	struct ArmRom *rom;
	uint32_t base, size;
	uint32_t* buf;
};

struct ArmRom {

	uint32_t start;
	enum RomChipType chipType;
	enum StrataFlashMode mode;
	uint16_t configReg;
};
	
static bool romAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* bufP)
{
	struct ArmRomPiece *piece = (struct ArmRomPiece*)userData;
	uint8_t *addr = (uint8_t*)piece->buf;
	struct ArmRom *rom = piece->rom;
	uint32_t cmd, fromStart;
	bool haveCmd = 0;
	
	fromStart = pa - rom->start;		//flashes care how far we are from start of flash, not of this arbitrary piece of it
	pa -= piece->base;
	if (pa >= piece->size)
		return false;
	
	addr += pa;
	
	if (write) {
		
		switch (rom->chipType) {
			case RomWriteIgnore:
				return true;
			case RomWriteError:
				return false;
			case RomStrataflash16x2x:
				if (size != 4) {
					fprintf(stderr, "StrataflashX2 command of improper size!\n");
					return false;
				}
				cmd = *(uint32_t*)bufP;
				if (cmd < 4) // STS commands are weird...
					cmd &= 0xffff;
				else if ((cmd & 0xffff) != (cmd >> 16)) {
					fprintf(stderr, "StrataflashX2 commands differ for flash halves: 0x%08x!\n", cmd);
					return false;
				}
				cmd &= 0xffff;
				haveCmd = true;
				break;
			case RomStrataFlash16x:
				if (size != 2) {
					fprintf(stderr, "Strataflash command of improper size!\n");
					return false;
				}
				cmd = *(uint16_t*)bufP;
				haveCmd = true;
				break;
			default:
				return false;
		}
		
		if (haveCmd) {
			
			if (rom->mode == StrataFlashSetReadConfigRegister) {
				
				rom->configReg = cmd;
				rom->mode = StrataFlashNormal;
				return true;
			}
			else switch (cmd & 0xff) {
				
				case 0x00 ... 0x03:	//STS Settings
					return true;
				
				case 0x50:
					//clear status register
					rom->mode = StrataFlashNormal;
					return true;
				
				case 0x60:
					//set read config reg
					rom->mode = StrataFlashSetReadConfigRegister;
					return true;
				
				case 0x70:
					//read sttaus register
					rom->mode = StrataFlashReadStatus;
					return true;
				
				case 0x90:
					//read identifier
					rom->mode = StrataFlashReadID;
					return true;
				
				case 0x98:
					//read query CFI
					rom->mode = StrataFlashReadCFI;
					return true;
				
				case 0xff:
					//read
					rom->mode = StrataFlashNormal;
					return true;
				
				default:
					fprintf(stderr, "Unknown strataflash command 0x%04x\n", cmd);
					return false;
			}
		}
		switch (size) {
			
			case 1:
		
				*((uint8_t*)addr) = *(uint8_t*)bufP;	//our memory system is little-endian
				break;
			
			case 2:
			
				*((uint16_t*)addr) = htole16(*(uint16_t*)bufP);	//our memory system is little-endian
				break;
			
			case 4:
			
				*((uint32_t*)addr) = htole32(*(uint32_t*)bufP);
				break;
			
			default:
			
				return false;
		}
	}
	else {
		
		//128mbit reply
		static const uint16_t qryReplies_from_0x10[] = {
			'Q', 'R', 'Y', 1, 0, 0x31, 0, 0, 0, 0, 0, 0x27, 0x36, 0, 0,
			8, 9, 10, 0, 1, 1, 2, 0, 0x18, 1, 0, 6, 0, 1, 0x7f, 0, 0,
			3,
			
			'P', 'R', 'I', '1', '1', 0xe6, 1, 0, 0, 1, 7, 0, 0x33, 0, 2, 0x80,
			0, 3, 3, 0x89, 0, 0, 0, 0, 0, 0, 0x10, 0, 4, 4, 2, 2,
			3
			
			};
		bool command = false;
		
		switch (rom->mode) {	//what modes expect a read of command size? which arent allowed at all
			
			case StrataFlashReadStatus:
			case StrataFlashReadID:
			case StrataFlashReadCFI:
				command = true;
				switch (rom->chipType) {
					case RomStrataFlash16x:
						if (size != 2) {
							fprintf(stderr, "Strataflash read of improper size!\n");
							return false;
						}
						fromStart /= 2;
						break;
					
					case RomStrataflash16x2x:
						if (size != 4) {
							fprintf(stderr, "StrataflashX2 read of improper size!\n");
							return false;
						}
						fromStart /= 4;
						break;
					
					default:
						return false;
				}
				break;
			
			case StrataFlashNormal:
			case StrataFlashSetReadConfigRegister:	//in this mode we can still fetch
				break;
			
			default:
				return false;
		}
		
		if (command) {
			
			bool skipdup = false;
			uint32_t reply;
			
			switch (rom->mode) {
				
				case StrataFlashReadStatus:
					rom->mode = StrataFlashNormal;
					reply = 0x0080;	//ready;
					break;
				
				case StrataFlashReadID:
					switch (fromStart) {
						case 0:
							reply = 0x0089;
							break;
						case 1:
							reply = 0x8802;
							break;
						case 5:
							reply = rom->configReg;
							break;
						case 0x80:	//protection register lock
							reply = 2;
							break;
						
						case 0x81:	//protection registers (uniq ID by intel and by manuf) copied frpom same chip as this rom
							reply = 0x001d0017;
							skipdup = true;
							break;
						
						case 0x82:
							reply = 0x000a0003;
							skipdup = true;
							break;
						
						case 0x83:
							reply = 0x3fb03fa6;
							skipdup = true;
							break;
						
						case 0x84:
							reply = 0x48d9c99a;
							skipdup = true;
							break;
						
						case 0x85 ... 0x88:
							reply = 0xffff;
							break;

						case 0x89:	//otp lock - all locked fo rus
							reply = 0;
							break;
						case 0x8a ... 0x109:	//otp data
							reply = 0;
							break;
						default:
							switch (fromStart & 0x7fff) {
								case 0:		//id?
									reply = 0x80;
									fprintf(stderr, "strataflash weird read of 0x%08x in ID mode returns 0x%04x\n", fromStart, reply);
									break;
								case 2:		//block lock/lockdown
									reply = 0;
									break;
								default:
									fprintf(stderr, "strataflash unknown read of 0x%08x in ID mode returns 0xffff\n", fromStart);
									reply = 0xffff;
									break;
							}
							break;
					}
					break;
				
				case StrataFlashReadCFI:
					fprintf(stderr, "CFI Read 0x%08x\n", fromStart);
					switch (fromStart) {
						case 0x00:
							reply = 0x0089;
							break;
						case 0x01:
							reply = 0x8802;
							break;
						case 0x10 ... sizeof(qryReplies_from_0x10) + 0x10:
							reply = qryReplies_from_0x10[fromStart - 0x10];
							break;
						default:
							switch (fromStart & 0xffff) {
								case 2:		//block status register
									reply = 0;
									break;
								default:
									return false;
							}
							break;
					}
					fprintf(stderr, "CFI Read 0x%08x -> 0x%04x\n", fromStart, reply);
					break;
				
				default:
					return false;
			}
			
			if (!skipdup)
				reply |= reply << 16;
			
			if (rom->chipType == RomStrataFlash16x)
				*(uint16_t*)bufP = reply;
			else
				*(uint32_t*)bufP = reply;
			
			return true;
		}
		switch (size) {
			
			case 1:
				
				*(uint8_t*)bufP = *((uint8_t*)addr);
				break;
			
			case 2:
			
				*(uint16_t*)bufP = le16toh(*((uint16_t*)addr));
				break;
			
			case 4:
			
				*(uint32_t*)bufP = le32toh(*((uint32_t*)addr));
				break;
			
			case 64:
				((uint32_t*)bufP)[ 8] = le32toh(*((uint32_t*)(addr + 32)));
				((uint32_t*)bufP)[ 9] = le32toh(*((uint32_t*)(addr + 36)));
				((uint32_t*)bufP)[10] = le32toh(*((uint32_t*)(addr + 40)));
				((uint32_t*)bufP)[11] = le32toh(*((uint32_t*)(addr + 44)));
				((uint32_t*)bufP)[12] = le32toh(*((uint32_t*)(addr + 48)));
				((uint32_t*)bufP)[13] = le32toh(*((uint32_t*)(addr + 52)));
				((uint32_t*)bufP)[14] = le32toh(*((uint32_t*)(addr + 56)));
				((uint32_t*)bufP)[15] = le32toh(*((uint32_t*)(addr + 60)));
				//fallthrough
			case 32:
			
				((uint32_t*)bufP)[4] = le32toh(*((uint32_t*)(addr + 16)));
				((uint32_t*)bufP)[5] = le32toh(*((uint32_t*)(addr + 20)));
				((uint32_t*)bufP)[6] = le32toh(*((uint32_t*)(addr + 24)));
				((uint32_t*)bufP)[7] = le32toh(*((uint32_t*)(addr + 28)));
				//fallthrough
			case 16:
				
				((uint32_t*)bufP)[2] = le32toh(*((uint32_t*)(addr +  8)));
				((uint32_t*)bufP)[3] = le32toh(*((uint32_t*)(addr + 12)));
				//fallthrough
			case 8:
				((uint32_t*)bufP)[0] = le32toh(*((uint32_t*)(addr +  0)));
				((uint32_t*)bufP)[1] = le32toh(*((uint32_t*)(addr +  4)));
				break;
			
			default:
			
				return false;
		}
	}
	
	return true;
}

struct ArmRom* romInit(struct ArmMem *mem, uint32_t adr, void **pieces, const uint32_t *pieceSizes, uint32_t numPieces, enum RomChipType chipType)
{
	struct ArmRom *rom = (struct ArmRom*)malloc(sizeof(*rom));
	uint32_t i;
	
	if (!rom)
		ERR("cannot alloc ROM at 0x%08x", adr);
	
	memset(rom, 0, sizeof (*rom));
	
	if (numPieces > 1 && chipType != RomWriteIgnore && chipType != RomWriteError)
		ERR("piecewise roms cannot be writeable\n");
	
	rom->start = adr;
	
	for (i = 0; i < numPieces; i++) {
		
		struct ArmRomPiece *piece = (struct ArmRomPiece*)malloc(sizeof(*piece));
		if (!piece)
			ERR("cannot alloc ROM piece at 0x%08x", adr);
		
		memset(piece, 0, sizeof (*piece));
		
		if (adr & 0x1f)
			ERR("rom piece cannot start at 0x%08x\n", adr);
		
		piece->base = adr;
		piece->size = *pieceSizes++;
		piece->buf = (uint32_t*)*pieces++;
		piece->rom = rom;
		
		adr += piece->size;
	
		if (!memRegionAdd(mem, piece->base, piece->size, romAccessF, piece))
			ERR("cannot add ROM piece at 0x%08x to MEM\n", adr);
	}
	
	rom->chipType = chipType;
	rom->mode = StrataFlashNormal;
	rom->configReg = 0xffc7;
	
	return rom;
}

