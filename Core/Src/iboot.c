#include "iboot.h"

static const struct {
	uint16_t devID;
	uint16_t sramSize;
	const char *family;	
} chipTable[] = {
//dev_ID  	RAM (Kbytes)	Family
	0x444, 		4, 					"STM32F030x4/STM32F030x6/STM32F03x",
	0x445, 		6, 					"STM32F070x6/STM32F04x",
	0x440, 		8, 					"STM32F030x8/STM32F05x",
	0x448, 		16, 				"STM32F070xB/STM32F07x",
	0x442, 		32, 				"STM32F030xC/STM32F09x",
};


/****************************************************************
	Copy vector table to RAM, reallocate RAM to 
	address 0 and then run application.
 ***************************************************************/
bool RunApp(uint32_t appStartAddr) {
	
	//check SP point to SRAM address range.
	if((*(__IO uint32_t *)appStartAddr >= SRAM_BASE) && \
		 (*(__IO uint32_t *)appStartAddr < (SRAM_BASE + ReadChipInfo()->sramSize))){
		//copy vectors table to SRAM.
	__IO uint32_t *src = (uint32_t *)appStartAddr;
	__IO uint32_t *dest = (uint32_t *)SRAM_BASE;
		for(uint8_t i=0; i<VECTOR_TBL_SIZE / sizeof(uint32_t); i++) {
			*dest++ = *src++;
		}
		//reallocate SRAM to address 0x00000000
		RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
		SYSCFG->CFGR1 |= SYSCFG_CFGR1_MEM_MODE;
		
		//update SP & PC value.
		__set_MSP(*(__IO uint32_t *)appStartAddr);
		((pfunc)(*((__IO uint32_t *)appStartAddr+1)))();
	}
	
	return false;
}

/******************************************************************
	Copy application code from appLoadAddr to appRunAddr.

	|---------------------------------------------------------------|
	|		app load address ( (flashsize - 2Kbyte) / 2 ~ flashsize )		|
	|---------------------------------------------------------------|
	|		app run address  ( 2Kbyte ~ (flashsize - 2Kbyte) / 2 )			|
	|---------------------------------------------------------------|
	|   					boot select code ( 0 ~ 2Kbyte )									  |
	|---------------------------------------------------------------|	
 *****************************************************************/
bool UpdateApp(void) {
uint32_t appMaxLen;
uint32_t crcVal = 0;
const sChipInfo *info;
	
	info = ReadChipInfo();
	appMaxLen = info->appLoadAddr - info->appRunAddr;
	
	crcVal = Crc32Calc((uint8_t *)(info->appLoadAddr), appMaxLen);
	FlashUnlock();
	FlashPageErase(info->appRunAddr, appMaxLen / info->pageSize);
	FlashProgram((uint16_t *)(info->appRunAddr), (uint16_t *)(info->appLoadAddr), appMaxLen);
	FlashLock();
	if(Crc32Calc((uint8_t *)(info->appRunAddr), appMaxLen) == crcVal) {
		return true;
	}
				
	return false;
}

bool WriteOptionByte(__IO uint16_t *addr, uint8_t val) {
OB_TypeDef optByte = {0};

	if((addr < (uint16_t *)OB_BASE) || (addr > ((uint16_t *)OB_BASE + sizeof(OB)))) {
		return false;
	}
	
	if((*addr & 0xFF) == val) {
		return true;
	}
	
	//Read option bytes before erase.
	for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
		if(i == ((uint32_t)addr - OB_BASE) / 2) {
			((uint16_t *)&optByte)[i] = val;
		} else {
			((uint16_t *)&optByte)[i] = ((uint16_t *)OB_BASE)[i];
		}
	}

	FlashUnlock();
	
	//enable option byte write.
	if ((FLASH->CR & FLASH_CR_OPTWRE) == 0) {
		FLASH->OPTKEYR = FLASH_OPTKEY1;
		FLASH->OPTKEYR = FLASH_OPTKEY2;
	}
	
	//option byte erase.
	FLASH->CR |= FLASH_CR_OPTER;
	FLASH->CR |= FLASH_CR_STRT;
	while ((FLASH->SR & FLASH_SR_BSY) != 0);
	if ((FLASH->SR & FLASH_SR_EOP) != 0) {
		FLASH->SR = FLASH_SR_EOP;
	}	else {
		FlashLock();
		return false;
	}
	FLASH->CR &= ~FLASH_CR_OPTER;	
	
	//write option byte.
	FLASH->CR |= FLASH_CR_OPTPG;
	for(uint8_t i=0; i<sizeof(optByte) / sizeof(uint16_t); i++) {
		((uint16_t *)OB_BASE)[i] = ((uint16_t *)&optByte)[i];
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
		if ((FLASH->SR & FLASH_SR_EOP) != 0) {
			FLASH->SR = FLASH_SR_EOP;
		}	else {
			FlashLock();
			return false;
		}
	}
	
	FLASH->CR &= ~FLASH_CR_OPTPG;
	//
	FLASH->CR |= FLASH_CR_OBL_LAUNCH;
	
	return true;
}


static void FlashUnlock(void) {
	if ((FLASH->CR & FLASH_CR_LOCK) != 0 ) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
}

static void FlashLock(void) {
	FLASH->CR |= FLASH_CR_LOCK;
}

static bool FlashPageErase(uint32_t addr, uint8_t pageCnt) {

	FLASH->CR |= FLASH_CR_PER;
	for(uint8_t i=0; i<pageCnt; i++) {
		FLASH->AR = addr + ReadChipInfo()->pageSize * i;
		FLASH->CR |= FLASH_CR_STRT;
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
		if ((FLASH->SR & FLASH_SR_EOP) != 0) {
			FLASH->SR = FLASH_SR_EOP;
		}	else {
			return false;
		}
	}
	FLASH->CR &= ~FLASH_CR_PER;
	return true;
}

static uint32_t FlashProgram(uint16_t *dest, uint16_t *src, uint16_t len) {
	
	//write device information to constChipInfo location.
	FLASH->CR |= FLASH_CR_PG;
	for(uint32_t i=0; i<len / 2; i++) {
		dest[i] = src[i];
		while ((FLASH->SR & FLASH_SR_BSY) != 0 );
		if (FLASH->SR & FLASH_SR_EOP) {
			FLASH->SR = FLASH_SR_EOP;
		} else {
			len = i * 2;
			break;
		}
	}
	FLASH->CR &= ~FLASH_CR_PG;
	
	return len;
}

static uint32_t Crc32Calc(const uint8_t *buff, uint32_t len) {
uint32_t val = 0;
	
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
	CRC->INIT = 0xFFFFFFFF;
	CRC->CR = CRC_CR_REV_OUT | CRC_CR_REV_IN_0 | CRC_CR_RESET;
	while(CRC->CR & CRC_CR_RESET);
	
	while(len--) {
		CRC->DR = *buff++;
	}
	
	val = CRC->DR;
	RCC->AHBENR &= ~RCC_AHBENR_CRCEN;	
	
	return val;
}


static const sChipInfo *ReadChipInfo(void) {
const sChipInfo *info = (const sChipInfo *)CHIP_INFO_ADDR;

	//Chip information content at address CHIP_INFO_ADDR will be erased 
	//when iboot code be programed. We will reprogram it.
	if(info->UID == 0xFFFFFFFF) {
	sChipInfo chipInfo = {0};
	
		//CRC32 calculate convert 96bit UID to 32bit UID.
		chipInfo.UID = Crc32Calc((const uint8_t *)UID_BASE, 12);
		
		//Read device ID
		chipInfo.devID = DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID_Msk;
		//Read revision ID
		chipInfo.devRev = (DBGMCU->IDCODE & DBGMCU_IDCODE_REV_ID_Msk)>>DBGMCU_IDCODE_REV_ID_Pos;
		//Calculate page size. 
		//If flash size greater than 128KByte, page size shold be 2KByte.
		chipInfo.pageSize = (*(uint16_t *)FLASHSIZE_BASE < 128) ? 1024 : 2048;
		//Calculate total pages.
		chipInfo.totalPages = (*(uint16_t *)FLASHSIZE_BASE < 128) ? \
													*(uint16_t *)FLASHSIZE_BASE : *(uint16_t *)FLASHSIZE_BASE / 2 ;
		
		//Match chip family & ram size from chipTable.
		for(uint8_t i=0; i<sizeof(chipTable)/sizeof(chipTable[0]); i++) {
			if(chipInfo.devID == chipTable[i].devID) {
				chipInfo.sramSize = chipTable[i].sramSize * 1024;
				chipInfo.family = chipTable[i].family;
				break;
			}
		}

		//Calculate application run address.
		chipInfo.appRunAddr = FLASH_BASE + IBOOT_APP_SIZE;
		//Calculate application load address.
		chipInfo.appLoadAddr = (chipInfo.pageSize * chipInfo.totalPages + IBOOT_APP_SIZE) / 2 + FLASH_BASE;
		
		//
		FlashUnlock();
		FlashProgram((uint16_t *)CHIP_INFO_ADDR, (uint16_t *)&chipInfo, sizeof(chipInfo));
		FlashLock();		
	}
	
	return info;
}
