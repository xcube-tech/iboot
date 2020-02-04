#ifndef	__IBOOT_H__
#define	__IBOOT_H__

#include "stm32f0xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef		void (*pfunc)(void);

typedef struct {
	uint32_t appRunAddr;
	uint32_t appLoadAddr;	
	uint16_t pageSize;
	uint16_t totalPages;
	uint16_t sramSize;
} sChipInfo;


#define		VECTOR_TBL_ITEMS			(48)
#define		EXPORT_FUNC_ADDR			(0x080003F0)

//public functions prototype
bool RunApp(sChipInfo *chipInfo);
bool CopyAppCode(uint32_t destAddr, uint32_t srcAddr, uint32_t pages);
bool WriteOptByte(__IO uint16_t *addr, uint16_t val);
void ReadChipInfo(sChipInfo *chipInfo);
uint32_t Crc32Calc(const uint8_t *buff, uint32_t len);


//private functions prototype
static uint16_t GetFlashPageSize(void);
static bool FlashUnlock(void);
static bool FlashLock(void);
static bool FlashPageErase(uint32_t addr, uint32_t pageCnt);
static uint32_t FlashProgram(uint16_t *dest, uint16_t *src, uint32_t len);

#endif //__IBOOT_H__
