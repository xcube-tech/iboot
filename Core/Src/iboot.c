#include "iboot.h"

//Device ID vs RAM size match table.
const static struct {
	uint16_t devId;
	uint16_t ramSize;
} ramSizeTable[] = {
//devId  			RAM (Kbytes)		Family
	{0x444, 		4 * 1024}, 			//"STM32F030x4/STM32F030x6/STM32F03x"},
	{0x445, 		6 * 1024}, 			//"STM32F070x6/STM32F04x"},
	{0x440, 		8 * 1024}, 			//"STM32F030x8/STM32F05x"},
	{0x448, 		16 * 1024}, 		//"STM32F070xB/STM32F07x"},
	{0x442, 		32 * 1024}, 		//"STM32F030xC/STM32F09x"},
};


//////////////////////////////////////////////////////////////////

ChipInfo chipInfo(void) {
ChipInfo info = {0};

	//Find chip ram size from ramSizeTable.
	for(uint8_t i=0; i<sizeof(ramSizeTable)/sizeof(ramSizeTable[0]); i++) {
		if((DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID_Msk) == ramSizeTable[i].devId) {
			info.id = ramSizeTable[i].devId;
			info.ramSize = ramSizeTable[i].ramSize;
			break;
		}
	}
	
	//The size of the device Flash memory expressed in Kbytes.
	uint32_t romSize = *(uint16_t *)FLASHSIZE_BASE;
	
	info.pageSize = (romSize < 128) ? 1024 : 2048;
	info.pageCount = romSize * 1024 / info.pageSize;
	
	return info;
}

/****************************************************************
	If the content in addr is a valid SP value,
	We copy vector table from appStartAddr to RAM,
	reallocate RAM to address 0, and then run application.
 ***************************************************************/
bool runApp(uint32_t addr) {
uint32_t *sp = (uint32_t *)addr;
	
	//copy vectors table to SRAM.
	uint32_t *src = sp;
	uint32_t *dest = (uint32_t *)SRAM_BASE;
	for(uint8_t i=0; i<VECTOR_TBL_ITEMS; i++) {
		*dest++ = *src++;
	}
	//reallocate SRAM to address 0x00000000
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	SYSCFG->CFGR1 |= SYSCFG_CFGR1_MEM_MODE;
	
	//update SP & PC value.
	__set_MSP(*sp);
	((pfunc)(*(sp + 1)))();	
	
	return false;
}

bool isAppCopyed(void) {
	return OB->DATA0 & 0x01;
}

void setAppCopyed(bool copyed) {
	
	if(copyed ^ isAppCopyed()) {
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
		FLASH->OPTKEYR = FLASH_OPTKEY1;
		FLASH->OPTKEYR = FLASH_OPTKEY2;
		
		if(FLASH->CR & FLASH_CR_OPTWRE) {
			OB_TypeDef optByte = {0};
			//Backup option bytes.
			for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
				((uint16_t *)(&optByte))[i] = ((uint16_t *)(OB_BASE))[i];
			}
			
			//Erase option bytes.
			FLASH->CR |= FLASH_CR_OPTER;
			FLASH->CR |=  FLASH_CR_STRT;
			while ((FLASH->SR & FLASH_SR_BSY) != 0);
			FLASH->SR |= FLASH_SR_EOP;
			FLASH->CR &= ~FLASH_CR_OPTER;
			
			optByte.DATA0 = (optByte.DATA0 & ~0x01) | copyed;
			
			//Write back option byte
			FLASH->CR |= FLASH_CR_OPTPG;
			
			for(uint8_t i=0; i<sizeof(optByte)/sizeof(uint16_t); i++) {
				((uint16_t *)(OB_BASE))[i] = ((uint16_t *)(&optByte))[i];
				while ((FLASH->SR & FLASH_SR_BSY) != 0);
			}
			FLASH->SR |= FLASH_SR_EOP;
			FLASH->CR &= ~FLASH_CR_OPTPG;
		}
		
		FLASH->CR &= ~FLASH_CR_OPTWRE;
	}
	
	optByteLoadReset();
}

bool copyApp(uint32_t destAddr, uint32_t srcAddr, uint32_t byteCount) {
	if((destAddr == srcAddr) || (byteCount == 0)) {
		return false;
	}
	
	while ((FLASH->SR & FLASH_SR_BSY) != 0);
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;	
	
	if(FLASH->CR & FLASH_CR_LOCK) {
		return false;
	}
	
	uint32_t pageSize = (*(uint16_t *)FLASHSIZE_BASE < 128) ? 1024 : 2048;
	
	//Page erase
	FLASH->CR |= FLASH_CR_PER;
	for(uint32_t i=0; i<byteCount; i+=pageSize) {
		FLASH->AR = destAddr + i;
		FLASH->CR |= FLASH_CR_STRT;
		while ((FLASH->SR & FLASH_SR_BSY) != 0);
	}
	FLASH->SR |= FLASH_SR_EOP;
	FLASH->CR &= ~FLASH_CR_PER;	
	
	//Page program
	FLASH->CR |= FLASH_CR_PG;
	for(uint32_t i=0; i<byteCount / sizeof(uint16_t); i++) {
		((uint16_t *)destAddr)[i] = ((uint16_t *)srcAddr)[i];
		while ((FLASH->SR & FLASH_SR_BSY));	
	}
	FLASH->SR |= FLASH_SR_EOP;
	FLASH->CR &= ~FLASH_CR_PG;	
	
	return true;
}

/******************************************************************
	Calculate CRC checksum of len bytes of buff with standerd CRC-32.
 *****************************************************************/
uint32_t crcCheckSum(const uint8_t *buff, uint32_t byteCount) {
uint32_t val = 0;
	
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
	CRC->INIT = UINT32_MAX;
	CRC->CR = CRC_CR_REV_OUT | CRC_CR_REV_IN_0 | CRC_CR_RESET;
	while(CRC->CR & CRC_CR_RESET);
	
	while(byteCount--) {
		*(uint8_t *)(&CRC->DR) = *buff++;
	}
	
	val = (CRC->DR ^ UINT32_MAX);
	RCC->AHBENR &= ~RCC_AHBENR_CRCEN;	
	
	return val;
}

/******************************************************************
	Option byte loader reset.
 *****************************************************************/
void optByteLoadReset(void) {
	FLASH->CR = FLASH_CR_OBL_LAUNCH;
}
