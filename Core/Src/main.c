#include "iboot.h"

int main(void) {
sChipInfo chipInfo = {0};
uint16_t appPages = 0;
	
	//Read flash size, page size, ram size, app load n run address.
	ReadChipInfo(&chipInfo);
	
	//We copy application code from appLoadAddr to appRunAddr,
	//if LSB of user data option byte is not true.
	if(!(OB->DATA0 & 0x01)) {
		//We will copy all app pages.
		appPages = chipInfo.totalPages / 2 - 1;
		
		//The content of app load address must be a valid SP pointer.
		if((*(uint32_t *)(chipInfo.appRunAddr) >= SRAM_BASE) && \
			 (*(uint32_t *)(chipInfo.appRunAddr) < SRAM_BASE + chipInfo.sramSize)) {
			CopyAppCode(chipInfo.appRunAddr, chipInfo.appLoadAddr, appPages);
		}
		
		//Set LSB of user data option byte to indicate successful upgrade.
		if(Crc32Calc((uint8_t *)(chipInfo.appRunAddr), chipInfo.pageSize * appPages) == \
			 Crc32Calc((uint8_t *)(chipInfo.appLoadAddr), chipInfo.pageSize * appPages)) {
			WriteOptByte(&(OB->DATA0), OB->DATA0 | 0x01);
		}
		//Force the option byte reload, this operation generates a system reset. 
		FLASH->CR = FLASH_CR_OBL_LAUNCH;			 
	}
	
	//Run application which locate at info.appRunAddr.
	RunApp(&chipInfo);
	
	//Normally program will never run to here.
	while(1);
}
