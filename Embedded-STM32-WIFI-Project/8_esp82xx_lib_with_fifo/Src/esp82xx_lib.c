#include "esp82xx_lib.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "fifo.h"


#define SR_RXNE		(1U<<5)
#define SR_TXE		(1U<<7)

#define SERVER_REPSONSE_SIZE		1024

#define ESP8266_WIFI_MODE_STA				1
#define ESP8266_WIFI_MODE_AP				2
#define ESP8266_WIFI_MODE_STA_AND_AP		3



#define MAX_NUM_OF_TRY				10

/*Buffer to hold characters after "+IPD," substring*/
char server_resp_buffer[SERVER_REPSONSE_SIZE];

char sub_str[32];
volatile bool searching       			  =  false ;
volatile bool is_response     			  =  false;
volatile int  search_idx 	  			  = 0;

char server_resp_sub_str[16] 			  = "+ipd,";
volatile bool server_search_resp_cmplt    = false;
volatile int  server_resp_search_idx 	  = 0;
volatile int server_resp_searching       =  0 ;

int server_resp_idx						  = 0 ;


char temp_buffer[1024];


static void wait_resp(char *pt);

static uint8_t esp82xx_reset(void);
static uint8_t esp82xx_set_wifi_mode(uint8_t mode);
static uint8_t esp82xx_list_access_points(void);
static uint8_t exp82xx_join_wifi_access_point(const char * ssid, const char * password);
static uint8_t esp82xx_get_local_ip_addr(void);
static uint8_t esp82xx_dns_get_ip(char *website);

static void esp_server_resp_srch_strt(void);
static void search_check(char letter);
static void esp_server_resp_srch_check(char letter);
static void copy_software_to_hardware(void);
static void esp82xx_send_cmd(const char * cmd);
static void esp_uart_callback(void);
static void esp82xx_process_data(void);
static void uart_output_char(char data);

void esp82xx_init(const char * ssid, const char * password)
{
	/*Enable fifos*/
	tx_fifo_init();
	rx_fifo_init();

	/*Enable RS pin*/
	esp_rs_pin_init();

	/*Enable esp uart*/
	esp_uart_init();

	/*Enable debug uart*/
	debug_uart_init();

	/*Initialize flags*/
	searching  = false;
	is_response =  false;
	server_resp_searching = 0;
	server_search_resp_cmplt = 0;

	printf("ESP8266 Initialization...\n\r");

	/*Enable interrupt*/
	NVIC_EnableIRQ(USART1_IRQn);

	/*Reset esp module*/

	if(esp82xx_reset() == 0)
	{
		printf("Reset failure, could not reset \n\r");
	}
	else{
		printf("Reset was successful...\n\r");
	}

	if(esp82xx_set_wifi_mode(ESP8266_WIFI_MODE_STA) == 0)
	{
		printf("SetWifiMode Failed\n\r");
	}
	else{
		printf("Wifi mode set successfully....\n\r");
	}

	esp82xx_list_access_points();

	/*Join wifi*/

	if(exp82xx_join_wifi_access_point(ssid,password)== 0)
	{
		printf("Could not join wifi\n\r");

	}
	else
	{
		printf("Wifi joined successfully...\n\r");

	}

	/*Get ip address of "google.com"*/
	esp82xx_dns_get_ip("google.com");

}



/*Reset esp module*/

static uint8_t esp82xx_reset(void)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;

	wait_resp("ok\r\n");

	while(num_of_try)
	{
		/*Set reset pin low */
		esp_rs_pin_disable();

		/*Wait a bit*/
		systick_delay_ms(10);

		/*Set reset pin high*/
		esp_rs_pin_enable();

		/*Send RST command*/
		esp82xx_send_cmd("AT+RST\r\n");

		/*Wait */
		systick_delay_ms(500);

		/*Check for response*/
		if(is_response)
		{
			/*Success*/

			return 1;
		}

		num_of_try--;
	}

	/*Failed */
	return 0;
}


/*Set Wifi mode*/

static uint8_t esp82xx_set_wifi_mode(uint8_t mode)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");

    while(num_of_try)
    {
    	sprintf((char *)temp_buffer, "AT+CWMODE=%d\r\n",mode);
    	esp82xx_send_cmd((const char *) temp_buffer);
		systick_delay_ms(500);

		if(is_response)
		{
			/*Success*/

			return 1;
		}
		num_of_try--;

    }

    return 0;
}
/*List access points*/
static uint8_t esp82xx_list_access_points(void)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");

	while(num_of_try)

	{
		esp82xx_send_cmd("AT+CWLAP\r\n");
		systick_delay_ms(5000);
		if(is_response)
		{
			/*Success*/

			return 1;
		}
		num_of_try--;

	}

    return 0;

}

/*Join access point*/
static uint8_t exp82xx_join_wifi_access_point(const char * ssid, const char * password)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");
	while(num_of_try)
	{
		sprintf((char *)temp_buffer,"AT+CWJAP=\"%s\",\"%s\"\r\n",ssid,password);
		esp82xx_send_cmd((const char *)temp_buffer);
		systick_delay_ms(3000);
		if(is_response)
		{
			/*Success*/

			return 1;
		}
		num_of_try--;

	}
    return 0;

}

/*Get local ip address*/
static uint8_t esp82xx_get_local_ip_addr(void)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");
	while(num_of_try)
	{
		esp82xx_send_cmd("AT+CIFSR\r\n");
		systick_delay_ms(3000);

		if(is_response)
		{
			/*Success*/

			return 1;
		}

		num_of_try--;


	}

    return 0;

}

/*Create tcp connection*/
uint8_t esp82xx_create_tcp_conn(char *ip_address)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");
	while(num_of_try)
	{
		sprintf((char *)temp_buffer,"AT+CIPSTART=\"TCP\",\"%s\",80\r\n",ip_address);
		esp82xx_send_cmd((char const *)temp_buffer);
		systick_delay_ms(3000);
		if(is_response)
		{
			/*Success*/

			return 1;
		}
		num_of_try--;

	}
    return 0;

}

/*Send TCP Packet to remote server*/
uint8_t esp82xx_send_tcp_pckt(char * pckt)
{
	 /*Combine packet length and command*/
	 sprintf((char *)temp_buffer,"AT+CIPSEND=%d\r\n", strlen(pckt));

	/*Send packet length and command*/
	esp82xx_send_cmd(temp_buffer);

	systick_delay_ms(50);

	/*Send packet*/
	esp82xx_send_cmd(pckt);

	/*Initialize server response search*/
	esp_server_resp_srch_strt();

	while( server_search_resp_cmplt ==  false)
	{
		systick_delay_ms(1);
	}

	if(server_search_resp_cmplt == false )
	{
		return 0;
	}
	else
	{
		return 1;
	}

}

/*Close tcp connection*/

uint8_t esp82xx_close_tcp_conn(void)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");

	while(num_of_try)
	{
		esp82xx_send_cmd("AT+CIPCLOSE\\r\n");
		systick_delay_ms(3000);
		if(is_response)
			{
				/*Success*/

				return 1;
			}
		num_of_try--;
	}

	return 0;
}


/*Get domain name ip address*/

static uint8_t esp82xx_dns_get_ip(char *website)
{
	uint8_t num_of_try = MAX_NUM_OF_TRY;
	wait_resp("ok\r\n");
	while(num_of_try)
	{
		sprintf((char *)temp_buffer,"AT+CIPDOMAIN=\"%s\"\r\n",website);
		esp82xx_send_cmd(temp_buffer);
		systick_delay_ms(3000);
		if(is_response)
			{
				/*Success*/

				return 1;
			}
		num_of_try--;
	}
	return 0;
}



/*Initialize string search for server response*/
static void esp_server_resp_srch_strt(void){

	strcpy(server_resp_sub_str,"+ipd,");
	server_resp_search_idx = 0;
	server_resp_searching  = 1;
	server_search_resp_cmplt = false;
	server_resp_idx = 0;


}

/*Initialize string search in rx data stream*/
static void wait_resp(char *pt)
{
	strcpy(sub_str,pt);
	search_idx 	=	0;
	is_response	=   false;
	searching	=   true;

}

/*Convert to lowercase*/
char lc(char letter)
{
	if((letter >='A')&&(letter<='Z')) {
		letter |=0x20;
	}
	return letter;
}

/*Search for string in rx data stream*/
static void search_check(char letter)
{
	if(searching)
	{
		/*Check if characters match*/
		if(sub_str[search_idx] == lc(letter))
		{
			search_idx++;

			/*Check if strings match*/
			if(sub_str[search_idx] == 0){
				is_response = true;
				searching   =  false;
			}
		}
		else
		{
			/*Start over*/
			search_idx =  0;;
		}
	}
}

/*Search for server response in rx data stream*/
static void esp_server_resp_srch_check(char letter)
{
	if(server_resp_searching == 1)
	{
		/*Check if characters match*/
        if(server_resp_sub_str[server_resp_search_idx] == lc(letter) )
        {
        	server_resp_search_idx++;
			/*Check if strings match*/
        	if(server_resp_sub_str[server_resp_search_idx] ==  0)
        	{
        		server_resp_searching =  2;
        		strcpy(server_resp_sub_str,"\n\rok\r\n");
        		server_resp_search_idx = 0;

        	}

        }
        else
        {
			/*Start over*/
    		server_resp_search_idx = 0;
        }
	}
	else if(server_resp_searching == 2)
	{
		if(server_resp_idx < SERVER_REPSONSE_SIZE){

			server_resp_buffer[server_resp_idx] =  lc(letter);
			server_resp_idx++;
		}

		/*Check if characters match*/

		 if(server_resp_sub_str[server_resp_search_idx] == lc(letter) )
		 {
	        	server_resp_search_idx++;

				/*Check if strings match*/

	        	if(server_resp_sub_str[server_resp_search_idx] ==  0)
	        	{
	        		server_search_resp_cmplt =  true;
	        		server_resp_searching    = 0 ;
	        	}
		 }
		 else{
			 /*Start over*/
			   server_resp_search_idx = 0;
		 }
	}
}

/*Copy content of tx_fifo in Debug UART DR*/
static void copy_software_to_hardware(void)
{
	char letter;
	while((USART2->SR & SR_TXE) && tx_fifo_size()> 0){
		tx_fifo_get(&letter);
		USART2->DR = letter;
	}
}

/*Output uart character*/
static void uart_output_char(char data)
{
	if(tx_fifo_put(data) ==  FIFOFAIL)
	{
		return;
	}
	copy_software_to_hardware();

}

 static void esp82xx_process_data(void)
{
	char letter;

	/*Check if there new data in wifi uart data register*/
	if(USART1->SR & SR_RXNE)
	{
		/*Store data from wifi uart data register in local variable*/
		letter = USART1->DR;

		/*Print data from wifi uart data register to debug uart i.e computer*/
		uart_output_char(letter);

		/*Check for response*/
		search_check(letter);

		/*Check for server response*/
		esp_server_resp_srch_check(letter);


	}
}


/*Call back function for esp82xx uart*/

static void esp_uart_callback(void)
{
	esp82xx_process_data();
}

/*esp82xx uart irqhandler*/
void USART1_IRQHandler(void)
{
	esp_uart_callback();
}


/*Send command to esp82xx*/
static void esp82xx_send_cmd(const char * cmd)
{
	int index = 0;
   	while(cmd[index] !=0 )
   	{
   		esp_uart_write_char(cmd[index++]);
   	}
}


