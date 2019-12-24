#ifndef	__IBOOT_H__
#define	__IBOOT_H__

#include "stm32f0xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef		void (*pfunc)(void);

typedef struct {
	uint32_t UID;
	uint32_t idCode;
	uint16_t pageSize;
	uint16_t totalPages;
	uint16_t sramSize;
	uint16_t reserved;
	uint32_t appRunAddr;
	uint32_t appLoadAddr;	
} sChipInfo;


#define		VECTOR_TBL_SIZE				(48 * 4)
#define		CHIP_INFO_MAX_SIZE		(32)

//public functions prototype
bool MoveAppCode(uint32_t destAddr, uint32_t srcAddr, uint32_t pages);
bool RunApp(uint32_t appStartAddr);
const sChipInfo *ReadChipInfo(void);

//private functions prototype
static uint16_t GetFlashPageSize(void);
static void FlashUnlock(void);
static void FlashLock(void);
static bool FlashPageErase(uint32_t addr, uint8_t pageCnt);
static uint32_t FlashProgram(uint16_t *dest, uint16_t *src, uint16_t len);
static uint32_t Crc32Calc(const uint8_t *buff, uint32_t len);


#endif //__IBOOT_H__
