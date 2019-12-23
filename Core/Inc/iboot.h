#ifndef	__IBOOT_H__
#define	__IBOOT_H__

#include "stm32f0xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef		void (*pfunc)(void);
typedef struct {
	const char *family;
	uint32_t appRunAddr;
	uint32_t appLoadAddr;
	uint32_t UID;
	uint16_t devID;
	uint16_t devRev;
	uint16_t pageSize;
	uint16_t totalPages;
	uint16_t sramSize;
} sChipInfo;

#define		IBOOT_APP_SIZE				(2 * 1024)
#define		VECTOR_TBL_SIZE				(48 * 4)
#define		CHIP_INFO_ADDR				(FLASH_BASE + IBOOT_APP_SIZE - 64)


//public functions prototype
bool UpdateApp(void);
bool RunApp(uint32_t appStartAddr);
bool WriteOptionByte(__IO uint16_t *addr, uint8_t val);

//private functions prototype
static void FlashUnlock(void);
static void FlashLock(void);
static bool FlashPageErase(uint32_t addr, uint8_t pageCnt);
static uint32_t FlashProgram(uint16_t *dest, uint16_t *src, uint16_t len);
static uint32_t Crc32Calc(const uint8_t *buff, uint32_t len);
static const sChipInfo *ReadChipInfo(void);

#endif //__IBOOT_H__
