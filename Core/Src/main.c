#include "iboot.h"

/******************************************************************
	Move application code from appLoadAddr to appRunAddr and run it.
 *****************************************************************/
int main(void) {
const sChipInfo *info;
	
	//Read chip informations.
	info = ReadChipInfo();
	
	//We copy application code from appLoadAddr to appRunAddr,
	//if appLoadAddr contents is a valid SP pointer.
	if((*(__IO uint32_t *)(info->appLoadAddr) >= SRAM_BASE) && \
		 (*(__IO uint32_t *)(info->appLoadAddr) < (SRAM_BASE + info->sramSize))){
		MoveAppCode(info->appRunAddr, info->appLoadAddr, (info->appLoadAddr - info->appRunAddr) / info->pageSize);
	}
	
	//Run application which locate at FLASH_BASE + IBOOT_APP_SIZE.
	RunApp(FLASH_BASE + IBOOT_APP_SIZE);
	
	//Normally program will never run to here.
	while(1);
}
