#include "iboot.h"

int main(void) {
	
	__disable_irq();

	ChipInfo info = chipInfo();
	
	uint32_t appRunAddr = FLASH_BASE + info.pageSize;
	uint32_t appLoadAddr = FLASH_BASE + (info.pageCount / 2) * info.pageSize;
	uint32_t appMaxSize = appLoadAddr - appRunAddr;
	
	if(!isAppCopyed()) {
		copyApp(appRunAddr, appLoadAddr, appMaxSize);
		
		setAppCopyed(crcCheckSum((uint8_t *)appRunAddr, appMaxSize)
								 == crcCheckSum((uint8_t *)appLoadAddr, appMaxSize));		
	} else if(*(uint32_t *)appRunAddr > SRAM_BASE
				 && *(uint32_t *)appRunAddr <= SRAM_BASE + info.ramSize){
		//Jump to application code, if SP value pointed to SRAM address range.
		runApp(appRunAddr);
	}
	
	//Normally program will never run to here.
	while(1) {
		//NVIC_SystemReset();
	}
}
