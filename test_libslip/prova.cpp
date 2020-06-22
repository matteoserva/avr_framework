
#include "ELClient.h"
#include "ELClientSocket.h"
#include "serial.h"
#include "milliseconds.h"
#include <util/delay.h>
#include <avr/interrupt.h>
#include "printf.h"
#include <stdio.h>
#include "string.h"
#include "stream.h"





void udpCb(uint8_t resp_type, uint8_t client_num, uint16_t len, char *data) {
	printf("udpCb is called\n");
	if( 0 > (int16_t) len)
	{
		printf("Send error: %u",len);
	}
	else
	{
		if (resp_type == USERCB_SENT) {
			printf("\tSent \n");
		} else if (resp_type == USERCB_RECV) {
			char recvData[len+1]; // Prepare buffer for the received data
			memcpy(recvData, data, len); // Copy received data into the buffer
			recvData[len] = '\0'; // Terminate the buffer with 0 for proper printout!

			printf("received %s\n",recvData);
		} else {
			printf("Received invalid response type\n");
		}
	}
}




int main()
{
	startSerial(57600);
	startIO();
	
	milliseconds_init();
	
	sei();
	
	Stream s(serial_getch,serial_putch,serial_getch_available);
	
	ELClient cl(&s);
	ELClientSocket udp(&cl);
	
	{
	bool ok;
		do {
			ok = cl.Sync();			// sync up with esp-link, blocks for up to 2 seconds
			if (!ok) s.println("EL-Client sync failed!");
		} while(!ok);
	}
	printf("Sync\n");
	
	{
		cl.GetWifiStatus();
		ELClientPacket *packet;
		if ((packet=cl.WaitReturn()) != NULL) {
			s.print("Wifi status: ");
			s.println(packet->value);
		}
	}
	
	if (udp.begin("192.168.1.255",9999, SOCKET_UDP, udpCb) < 0 ) {
		printf("UDP2 begin failed: \n");
		while(1);
	}
	
	int count = 0;
	while(1)
	{
		 ELClientPacket * p = cl.Process();
		//printf("process eseg\n");
		//printf("WIFI status:  %u\n",wifiConnected);
		if(count++ == 100)
		{
		udp.send("Message from your Arduino Uno WiFi over UDP socket\n");
		count = 0;
		
		}
		_delay_ms(10);

	}

}
