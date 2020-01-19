#include "iboot.h"

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
	If the content in appStartAddr is a valid SP pointer,
	We copy vector table from appStartAddr to RAM, 
	reallocate RAM to address 0, and then run application. 
 ***************************************************************/
bool RunApp(uint32_t appStartAddr) {
sChipInfo info = {0};

	ReadChipInfo(&info);
	
	//check SP point to SRAM address range.
	if(IsValueInRange(*(uint32_t *)appStartAddr, SRAM_BASE, SRAM_BASE + info.sramSize)){
		//copy vectors table to SRAM.
	__IO uint32_t *src = (uint32_t *)appStartAddr;
	__IO uint32_t *dest = (uint32_t *)SRAM_BASE;
		for(uint8_t i=0; i<VECTOR_TBL_ITEMS; i++) {
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
	Copy application code from srcAddr to destAddr.
 *****************************************************************/
bool CopyAppCode(uint32_t destAddr, uint32_t srcAddr, uint32_t pages) {
uint32_t crcVal = 0;
uint16_t pageSize = GetFlashPageSize();
bool updateSuccess = false;
	
	crcVal = Crc32Calc((uint8_t *)srcAddr, pageSize * pages);
	FlashUnlock();
	FlashPageErase(destAddr, pages);
	FlashProgram((uint16_t *)destAddr, (uint16_t *)srcAddr, pageSize * pages / sizeof(uint16_t));
	
	if(Crc32Calc((uint8_t *)destAddr, pageSize * pages) == crcVal) {
		updateSuccess = true;
	}
                       
	FlashLock();
				
	return updateSuccess;
}


bool WriteOptByte(volatile uint16_t *addr, uint16_t val) {
OB_TypeDef optByte = {0};

	if(((uint32_t)addr < OB_BASE) || ((uint32_t)addr >= (OB_BASE + sizeof(optByte)))) {
		return false;
	}

	//Backup option bytes.
	for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
		((uint16_t *)(&optByte))[i] = ((uint16_t *)(OB_BASE))[i];
	}
	
	//Modify option byte
	((uint16_t *)(&optByte))[((uint32_t)addr - OB_BASE) / 2] = val;
	
	FlashUnlock();

	//Erase option bytes.
	FLASH->CR |= FLASH_CR_OPTER;
	FLASH->CR |=  FLASH_CR_STRT;
	while ((FLASH->SR & FLASH_SR_BSY) != 0);
	if ((FLASH->SR & FLASH_SR_EOP) != 0) {
		FLASH->SR = FLASH_SR_EOP;
	}	else {
		return false;
	}
	FLASH->CR &= ~FLASH_CR_OPTER;
	
	//Write option byte
	FLASH->CR |= FLASH_CR_OPTPG;
	
	for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
		((uint16_t *)(OB_BASE))[i] = ((uint16_t *)(&optByte))[i];
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
		if ((FLASH->SR & FLASH_SR_EOP) != 0) {
			FLASH->SR = FLASH_SR_EOP;
		}	else {
			return false;
		}	
	}
	FLASH->CR &= ~FLASH_CR_OPTPG;
	
	FlashLock();
	
	FLASH->CR |= FLASH_CR_OBL_LAUNCH;
	
	//while ((FLASH->SR & FLASH_SR_BSY) != 0);
	
	return true;
}

void ReadChipInfo(sChipInfo *info) {

	//Calculate page size. 
	//If flash size greater than 128KByte, page size shold be 2KByte.
	info->pageSize = GetFlashPageSize();

	//Calculate total pages.
	info->totalPages = (info->pageSize & 0x800) ? \
												*(uint16_t *)FLASHSIZE_BASE / 2 : *(uint16_t *)FLASHSIZE_BASE;
	
	//Find chip ram size from chipTable.
	for(uint8_t i=0; i<sizeof(chipTable)/sizeof(chipTable[0]); i++) {
		if((DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID_Msk) == chipTable[i].devID) {
			info->sramSize = chipTable[i].sramSize * 1024;
			break;
		}
	}

	//Calculate application run address.
	info->appRunAddr = FLASH_BASE + info->pageSize;
	//Calculate application load address.
	info->appLoadAddr = FLASH_BASE + info->pageSize * (info->totalPages / 2 + 1);
}

bool IsValueInRange(uint32_t val, uint32_t low, uint32_t high) {
	return ((val >= low) && (val < high));
}

static uint16_t GetFlashPageSize(void) {
	return (*(uint16_t *)FLASHSIZE_BASE < 128) ? 1024 : 2048;
}

static void FlashUnlock(void) {
	if((FLASH->CR & FLASH_CR_LOCK) != 0 ) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
	if((FLASH->CR & FLASH_CR_OPTWRE) == 0) {
		FLASH->OPTKEYR = FLASH_OPTKEY1;
		FLASH->OPTKEYR = FLASH_OPTKEY2;
	}
}

static void FlashLock(void) {
	FLASH->CR |= FLASH_CR_LOCK;
	FLASH->CR &= ~FLASH_CR_OPTWRE;
}

static bool FlashPageErase(uint32_t addr, uint32_t pageCnt) {

	FLASH->CR |= FLASH_CR_PER;
	for(uint32_t i=0; i<pageCnt; i++) {
		FLASH->AR = addr + GetFlashPageSize() * i;
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

static uint32_t FlashProgram(uint16_t *dest, uint16_t *src, uint32_t len) {
	
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

