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

//public functions prototype
bool CopyAppCode(uint32_t destAddr, uint32_t srcAddr, uint32_t pages);
bool RunApp(uint32_t appStartAddr);
bool WriteOptByte(volatile uint16_t *addr, uint16_t val);
void ReadChipInfo(sChipInfo *info);
bool IsValueInRange(uint32_t val, uint32_t low, uint32_t high);

//private functions prototype
static uint16_t GetFlashPageSize(void);
static void FlashUnlock(void);
static void FlashLock(void);
static bool FlashPageErase(uint32_t addr, uint32_t pageCnt);
static uint32_t FlashProgram(uint16_t *dest, uint16_t *src, uint32_t len);
static uint32_t Crc32Calc(const uint8_t *buff, uint32_t len);


#endif //__IBOOT_H__
