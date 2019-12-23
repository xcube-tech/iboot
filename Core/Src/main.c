#include "iboot.h"

int main(void) {
	
	//User data0 in option byte is used as application update flag.
	//If it's not the default value, we update application and reset it.
	if(OB->DATA0 != 0xFF) {
		if(UpdateApp()) {
			WriteOptionByte(&(OB->DATA0), 0xFF);
		}
	}
	
	//Run application which locate at FLASH_BASE + IBOOT_APP_SIZE.
	//Incase of failed, we generate a system reset to try to recover.
	if(!RunApp(FLASH_BASE + IBOOT_APP_SIZE)) {
		NVIC_SystemReset();
	}
	
	//Program will never run to here.
	while(1);
}
