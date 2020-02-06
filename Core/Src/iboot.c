#include "iboot.h"

//Export some functions at EXPORT_FUNC_ADDR.
const static bootCode_T bootCode __attribute__((at(EXPORT_FUNC_ADDR))) = {
	CopyAppCode,
	WriteOptByte,
	ReadChipInfo,
	Crc32Calc,
};

//Device ID vs RAM size match table.
const static struct {
	uint16_t devID;
	uint8_t sramSize;
} chipTable[] = {
//dev_ID  	RAM (Kbytes)	Family
	{0x444, 		4}, 					//"STM32F030x4/STM32F030x6/STM32F03x",
	{0x445, 		6}, 					//"STM32F070x6/STM32F04x",
	{0x440, 		8}, 					//"STM32F030x8/STM32F05x",
	{0x448, 		16}, 				//"STM32F070xB/STM32F07x",
	{0x442, 		32}, 				//"STM32F030xC/STM32F09x",
};

/****************************************************************
	If the content in info.appRunAddr is a valid SP pointer,
	We copy vector table from appStartAddr to RAM, 
	reallocate RAM to address 0, and then run application. 
 ***************************************************************/
bool RunApp(chipInfo_T *chipInfo) {
	
	//check SP point to SRAM address range.
	if((*(uint32_t *)(chipInfo->appRunAddr) >= SRAM_BASE) && \
		 (*(uint32_t *)(chipInfo->appRunAddr) < SRAM_BASE + chipInfo->sramSize)) {
		//copy vectors table to SRAM.
	__IO uint32_t *src = (uint32_t *)(chipInfo->appRunAddr);
	__IO uint32_t *dest = (uint32_t *)SRAM_BASE;
		for(uint8_t i=0; i<VECTOR_TBL_ITEMS; i++) {
			*dest++ = *src++;
		}
		//reallocate SRAM to address 0x00000000
		RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
		SYSCFG->CFGR1 |= SYSCFG_CFGR1_MEM_MODE;
		
		//update SP & PC value.
		__set_MSP(*(__IO uint32_t *)(chipInfo->appRunAddr));
		((pfunc)(*((__IO uint32_t *)(chipInfo->appRunAddr)+1)))();
	}
	
	return false;
}

/******************************************************************
	Copy n pages data from srcAddr to destAddr.
 *****************************************************************/
bool CopyAppCode(uint32_t destAddr, uint32_t srcAddr, uint32_t pages) {
bool eraseState = false;
	
	if(FlashUnlock()) {
		FlashPageErase(destAddr, pages);
		eraseState = FlashProgram((uint16_t *)destAddr, (uint16_t *)srcAddr, GetFlashPageSize() * pages);
		FlashLock();
	}
				
	return eraseState;
}

/******************************************************************
	Write option byte which locate at addr to val.
 *****************************************************************/
bool WriteOptByte(__IO uint16_t *addr, uint16_t val) {
OB_TypeDef optByte = {0};

	if(((uint32_t)addr < OB_BASE) || ((uint32_t)addr >= (OB_BASE + sizeof(optByte)))) {
		return false;
	}

	//Backup option bytes.
	for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
		((uint16_t *)(&optByte))[i] = ((uint16_t *)(OB_BASE))[i];
	}
	
	//Modify option byte
	((uint16_t *)(&optByte))[((uint32_t)addr - OB_BASE) / (sizeof(uint16_t))] = val;
	
	if(FlashUnlock()) {
		//Erase option bytes.
		FLASH->CR |= FLASH_CR_OPTER;
		FLASH->CR |=  FLASH_CR_STRT;
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
		if ((FLASH->SR & FLASH_SR_EOP) == 0) {
			return false;
		}
		FLASH->SR = FLASH_SR_EOP;
		FLASH->CR &= ~FLASH_CR_OPTER;
		
		//Write option byte
		FLASH->CR |= FLASH_CR_OPTPG;
		
		for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
			((uint16_t *)(OB_BASE))[i] = ((uint16_t *)(&optByte))[i];
			while ((FLASH->SR & FLASH_SR_BSY) != 0);
			if ((FLASH->SR & FLASH_SR_EOP) == 0) {
				return false;
			}
			FLASH->SR = FLASH_SR_EOP;
		}
		FLASH->CR &= ~FLASH_CR_OPTPG;
		
		FlashLock();	
		
		return true;
	}

	return false;
}

/******************************************************************
	Read flash size, page size, ram size, app load n run address.
 *****************************************************************/
void ReadChipInfo(chipInfo_T *chipInfo) {
	//Calculate page size. 
	//If flash size greater than 128KByte, page size shold be 2KByte.
	chipInfo->pageSize = GetFlashPageSize();

	//Calculate total pages.
	chipInfo->totalPages = (chipInfo->pageSize & 0x800) ? \
												*(uint16_t *)FLASHSIZE_BASE / 2 : *(uint16_t *)FLASHSIZE_BASE;
	
	//Find chip ram size from chipTable.
	for(uint8_t i=0; i<sizeof(chipTable)/sizeof(chipTable[0]); i++) {
		if((DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID_Msk) == chipTable[i].devID) {
			chipInfo->sramSize = chipTable[i].sramSize * 1024;
			break;
		}
	}

	//Calculate application run address.
	chipInfo->appRunAddr = FLASH_BASE + chipInfo->pageSize;
	//Calculate application load address.
	chipInfo->appLoadAddr = FLASH_BASE + chipInfo->pageSize * (chipInfo->totalPages / 2 + 1);
}

/******************************************************************
	Calculate CRC value of len bytes of buff with standerd CRC-32.
 *****************************************************************/
uint32_t Crc32Calc(const uint8_t *buff, uint32_t len) {
uint32_t val = 0;
	
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
	//CRC->INIT = 0xFFFFFFFF;
	CRC->CR = CRC_CR_REV_OUT | CRC_CR_REV_IN_0 | CRC_CR_RESET;
	while(CRC->CR & CRC_CR_RESET);
	
	while(len--) {
		*(uint8_t *)(&CRC->DR) = *buff++;
	}
	
	val = (CRC->DR ^ 0xFFFFFFFF);
	RCC->AHBENR &= ~RCC_AHBENR_CRCEN;	
	
	return val;
}



static uint16_t GetFlashPageSize(void) {
	return (*(uint16_t *)FLASHSIZE_BASE < 128) ? 1024 : 2048;
}

static bool FlashUnlock(void) {
	if((FLASH->CR & FLASH_CR_LOCK) != 0 ) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
	if((FLASH->CR & FLASH_CR_OPTWRE) == 0) {
		FLASH->OPTKEYR = FLASH_OPTKEY1;
		FLASH->OPTKEYR = FLASH_OPTKEY2;
	}
	
	return (FLASH->CR & FLASH_CR_OPTWRE);
}

static bool FlashLock(void) {
	FLASH->CR &= ~FLASH_CR_OPTWRE;
	FLASH->CR |= FLASH_CR_LOCK;	
	
	return (FLASH->CR & FLASH_CR_LOCK);
}

static bool FlashPageErase(uint32_t addr, uint32_t pageCnt) {

	FLASH->CR |= FLASH_CR_PER;
	for(uint32_t i=0; i<pageCnt; i++) {
		FLASH->AR = addr + GetFlashPageSize() * i;
		FLASH->CR |= FLASH_CR_STRT;
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
		if ((FLASH->SR & FLASH_SR_EOP) == 0) {
			return false;
		}
		FLASH->SR = FLASH_SR_EOP;
	}
	FLASH->CR &= ~FLASH_CR_PER;
	return true;
}

static bool FlashProgram(uint16_t *dest, uint16_t *src, uint32_t len) {
bool writeState = true;
	
	FLASH->CR |= FLASH_CR_PG;
	for(uint32_t i=0; i<len / sizeof(uint16_t); i++) {
		dest[i] = src[i];
		while ((FLASH->SR & FLASH_SR_BSY));	
		if (FLASH->SR & FLASH_SR_EOP) {
			FLASH->SR = FLASH_SR_EOP;
		} else {
			writeState = false;
			break;
		}
	}
	FLASH->CR &= ~FLASH_CR_PG;
	
	return writeState;
}
