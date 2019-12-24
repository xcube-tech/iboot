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
	If the content in appStartAddr is a valid SP pointer,
	We copy vector table from appStartAddr to RAM, 
	reallocate RAM to address 0, and then run application. 
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
	Erase appLoadAddr contents if copy success.
 *****************************************************************/
bool MoveAppCode(uint32_t destAddr, uint32_t srcAddr, uint32_t pages) {
uint32_t crcVal = 0;
bool updateSuccess = false;
	
	crcVal = Crc32Calc((uint8_t *)srcAddr, GetFlashPageSize() * pages);
	FlashUnlock();
	FlashPageErase(destAddr, pages);
	FlashProgram((uint16_t *)destAddr, (uint16_t *)srcAddr, GetFlashPageSize() * pages / sizeof(uint16_t));
	
	if(Crc32Calc((uint8_t *)destAddr, GetFlashPageSize() * pages) == crcVal) {
		FlashPageErase(srcAddr, pages);
		updateSuccess = true;
	}
	
	FlashLock();
				
	return updateSuccess;
}

const sChipInfo *ReadChipInfo(void) {
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
		chipInfo.pageSize = GetFlashPageSize();
		//Calculate total pages.
		chipInfo.totalPages = (chipInfo.pageSize > 1024) ? \
													*(uint16_t *)FLASHSIZE_BASE / 2 : *(uint16_t *)FLASHSIZE_BASE;
		
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


static uint16_t GetFlashPageSize(void) {
	return (*(uint16_t *)FLASHSIZE_BASE < 128) ? 1024 : 2048;
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

