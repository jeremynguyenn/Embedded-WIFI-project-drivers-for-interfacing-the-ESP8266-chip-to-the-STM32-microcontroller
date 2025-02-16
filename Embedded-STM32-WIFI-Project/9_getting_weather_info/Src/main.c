#include <stdio.h>
#include "stm32f4xx.h"
#include "esp82xx_driver.h"
#include "fifo.h"
#include "esp82xx_lib.h"


#define SSID_NAME  "TP-Link_9A4E"
#define PASSKEY    "94933581"

char pckt_to_send[] = "GET /data/2.5/weather?q=London&APPID=5c0a64144c0ebdd7a2f73c98406dea7b HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";

int main()
{

	esp82xx_init(SSID_NAME,PASSKEY);

	while(1)
	{

		if(esp82xx_create_tcp_conn("api.openweathermap.org"))
		{
			esp82xx_send_tcp_pckt(pckt_to_send);
		}

		esp82xx_close_tcp_conn();

	}

}

