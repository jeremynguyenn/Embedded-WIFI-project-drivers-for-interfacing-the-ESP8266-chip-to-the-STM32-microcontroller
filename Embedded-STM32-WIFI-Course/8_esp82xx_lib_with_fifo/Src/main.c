#include <stdio.h>
#include "stm32f4xx.h"
#include "esp82xx_driver.h"
#include "fifo.h"
#include "esp82xx_lib.h"


#define SSID_NAME  "TP-Link_9A4E"
#define PASSKEY    "94933581"

int main()
{

	esp82xx_init(SSID_NAME,PASSKEY);

	while(1)
	{

	}

}

