#ifndef	__IBOOT_H__
#define	__IBOOT_H__

#include <stdint.h>
#include <stdbool.h>
#include "RTE_Components.h"
#include CMSIS_device_header

typedef		void (*pfunc)(void);

typedef struct {
	uint16_t id;
	uint16_t ramSize;
	uint16_t pageSize;
	uint16_t pageCount;
} ChipInfo;

#define		VECTOR_TBL_ITEMS					(48)

//public functions prototype
bool runApp(uint32_t addr);
bool isAppCopyed(void);
void setAppCopyed(bool copyed);
void optByteLoadReset(void);
ChipInfo chipInfo(void);
bool copyApp(uint32_t destAddr, uint32_t srcAddr, uint32_t byteCount);
uint32_t crcCheckSum(const uint8_t *buff, uint32_t byteCount);

#endif //__IBOOT_H__
