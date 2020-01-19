#include "iboot.h"

int main(void) {
sChipInfo info = {0};
	
	//Read flash size, page size, ram size, app load n run address.
	ReadChipInfo(&info);
	
	//We copy application code from appLoadAddr to appRunAddr,
	//if LSB of user data option byte is not true.
	if(!(OB->DATA0 & 0x01)) {
		if(IsValueInRange(*(uint32_t *)(info.appLoadAddr), SRAM_BASE, SRAM_BASE + info.sramSize)){
			CopyAppCode(info.appRunAddr, info.appLoadAddr, info.totalPages / 2 - 1);
		}
		//Set LSB of user data option byte.
		WriteOptByte(&(OB->DATA0), OB->DATA0 | 0x01);
	}
	
	//Run application which locate at FLASH_BASE + pagesize.
	RunApp(FLASH_BASE + info.pageSize);
	
	//Normally program will never run to here.
	while(1);
}
