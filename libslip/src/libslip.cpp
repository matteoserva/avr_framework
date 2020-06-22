#include "libslip.h"
#include "stream.h"
#include "ELClient.h"
#include "ELClientSocket.h"
#include "serial.h"
#include "stdio.h"
#include "string.h"
#include "misc.h"
#include <util/delay.h>
#define LIBSLIP_MAX_CLIENTS 4

enum LibSlipState {LIBSLIP_OFF, LIBSLIP_PRE_INIT, LIBSLIP_SYNCING, LIBSLIP_WAITING_WIFI, LIBSLIP_RUNNING};


static Stream serial_stream(serial_getch,serial_putch,serial_getch_available);
static ELClient cl(&serial_stream);
static FP<void, void*> socketCb;
static libslip_cb_t userCb[LIBSLIP_MAX_CLIENTS] = {0};

static enum LibSlipState libSlipState = LIBSLIP_OFF;
struct ConnectionData
{
	char host[16];
    uint16_t port;
	enum LibSlipSockMode sock_mode;
	libslip_cb_t _userCb;
};

uint8_t reverseMap[LIBSLIP_MAX_CLIENTS];

struct ConnectionInfo
{
	struct ConnectionData connectionData;
	bool enabled;
	int sockfd;
};

struct ConnectionInfo activeConnections[LIBSLIP_MAX_CLIENTS];



static void udpCb(uint8_t resp_type, uint8_t client_num, uint16_t len, char *data) {
	
	enum LibSlipResult result = LIBSLIP_ERROR;
	
	
	
	if( 0 > (int16_t) len)
	{
		result = LIBSLIP_ERROR;
	}
	else
	{
		if (resp_type == USERCB_SENT) {
			result = LIBSLIP_SENT;
		} else if (resp_type == USERCB_RECV) {
			result = LIBSLIP_RECEIVED;
		} else {
			result = LIBSLIP_ERROR;
		}
	}
	
	if( activeConnections[client_num].connectionData._userCb)
		activeConnections[client_num].connectionData._userCb[client_num](client_num,result,(uint8_t*)data,len);
}


static	void socketCallback(void *res)
		{
			
		if (!res) return;
		uint8_t _resp_type; /**< Response type: 0 = send, 1 = receive; 2 = reset connection, 3 = connection */
		uint16_t _len; /**< Number of sent/received bytes */
		char *_data;
		uint8_t _client_num;
		ELClientResponse *resp = (ELClientResponse *)res;
		resp->popArg(&_resp_type, 1);
		resp->popArg(&_client_num, 1);
		resp->popArg(&_len, 2);
		
		if (_resp_type == 1)
		{
			resp->popArgPtr((void**)&_data);
		}

		if(_client_num >= LIBSLIP_MAX_CLIENTS)
			return;
		if(reverseMap[_client_num] >= LIBSLIP_MAX_CLIENTS)
			return;
		if(!activeConnections[reverseMap[_client_num]].enabled)
			return;
			
		udpCb(_resp_type, reverseMap[_client_num] , _len, _data);
	}
	
void libslip_reset_sockets()
{
	
	ELClient *_elc = &cl;
	uint8_t mode = LIBSLIP_UDP;
	char host[] = "0.0.0.0";
	uint16_t port = 0;
	for(int i = 0; i < 2*LIBSLIP_MAX_CLIENTS; i++)
	{
		
		_elc->Request(CMD_SOCKET_SETUP, (uint32_t)&socketCb, 3);
		_elc->Request(host, strlen(host));
		_elc->Request(&port, 2);
		_elc->Request(&mode, 1);
		_elc->Request();
			ELClientPacket *pkt = NULL;
		do
		{
			pkt = _elc->WaitReturn();
		} while(pkt == NULL || pkt->cmd != CMD_RESP_V);
	
		int res = (int)pkt->value;
		

		
		uint8_t ultimo = (res >= 0) &&  ( (res+1) % LIBSLIP_MAX_CLIENTS == 0);
		if( i+1 >= LIBSLIP_MAX_CLIENTS && ultimo)
			break;
		
	}
}



int libslip_handle_connect(unsigned int index)
{
		ELClient *_elc = &cl;
	uint8_t mode = activeConnections[index].connectionData.sock_mode;
	int res = -1;
	
	_elc->Request(CMD_SOCKET_SETUP, (uint32_t)&socketCb, 3);
	_elc->Request(activeConnections[index].connectionData.host, strlen(activeConnections[index].connectionData.host));
	_elc->Request(&activeConnections[index].connectionData.port, 2);
	_elc->Request(&mode, 1);
	_elc->Request();

		ELClientPacket *pkt = NULL;
	do
	{
		pkt = _elc->WaitReturn();
	} while(pkt == NULL || pkt->cmd != CMD_RESP_V);
	
	res = (int)pkt->value;
	printf("vero %d\n",res);

	if(res >= 0 && res < LIBSLIP_MAX_CLIENTS)
	{
		userCb[res] = activeConnections[index].connectionData._userCb ;
		reverseMap[res] = index;
		activeConnections[index].sockfd = res;
		printf("res %u index %u\n",index,res);
		return index;
	}
	return -1;
	
}

int libslip_connect(const char* host, uint16_t port, enum LibSlipSockMode sock_mode,libslip_cb_t _userCb)
{
	
	int trovato = -1;
	for(int i = 0; i <LIBSLIP_MAX_CLIENTS; i++)
	{
		if(!activeConnections[i].enabled)
		{
			trovato = i;
			break;
		}
	}
	if(trovato < 0)
		return -1;
		
	activeConnections[trovato].connectionData._userCb = _userCb;
	activeConnections[trovato].connectionData.sock_mode = sock_mode;
	activeConnections[trovato].connectionData.port = port;
	strcpy(activeConnections[trovato].connectionData.host,host);
	activeConnections[trovato].enabled = true;
	if(libSlipState == LIBSLIP_RUNNING)
	return libslip_handle_connect(trovato);
	
	
		return trovato;

}






static uint8_t wifiConnected =0;
void wifiCb(void *response) {
	ELClientResponse *res = (ELClientResponse*)response;
	if (res->argc() == 1) {
		uint8_t status;
		res->popArg(&status, 1);

		if(status == STATION_GOT_IP) {

			wifiConnected = 1;
		} else {

			wifiConnected = 0;
		}
	}
}




	
	
void libslip_send(int sockFd, void* data, int len)
{
	if(libSlipState != LIBSLIP_RUNNING)
		return;
	if(!activeConnections[sockFd].enabled)
			return;
	
	
	ELClient *_elc = &cl;
	_elc->Request(CMD_SOCKET_SEND, activeConnections[sockFd].sockfd, 2);
	_elc->Request(data, len);
	if (data != NULL && len > 0)
	{
		_elc->Request(data, len);
	}

	_elc->Request();
}	


static void libslip_handle_running()
{
	ELClientPacket * p = NULL;
	do
	{
		p = cl.Process();
		if(p)
		{
			if(p->cmd == CMD_SOCKET_SEND)
		
			{
				uint16_t len = p->argc;
				uint8_t* buffer = (uint8_t* ) p;
				buffer += 4;
				
				for(unsigned int i = 0; i < len; i++)
					serial_putch(buffer[i]);
					
			}
			else if(p->cmd == CMD_SYNC)
			{
				libslip_init();
				//watchdog_reboot();
			}
		}
	} while(p != NULL);
}
	
	
	
static void libslip_handle_init()
{
	
	cl.wifiCb.attach(wifiCb);
	cl.SyncStart();
	libSlipState = LIBSLIP_SYNCING;
}	
	
static void libslip_handle_wait_wifi()
{
	if(wifiConnected == 0)
		cl.Process();
	if(wifiConnected != 0)
	{
		libslip_reset_sockets();
		
		
		for(int i = 0; i < LIBSLIP_MAX_CLIENTS;i++)
			if(activeConnections[i].enabled)
				libslip_handle_connect(i);
				
		libSlipState = LIBSLIP_RUNNING;
	}
	
}
	
static void libslip_handle_syncing()
{
	enum SyncResult result = cl.SyncCheck();
	if(result == SYNC_RESULT_SUCCESS)
	{
		serial_stream.println("Sync\n");
		socketCb.attach(socketCallback);
		libSlipState = LIBSLIP_WAITING_WIFI;
	}
	else if(result == SYNC_RESULT_FAILURE)
	{
		serial_stream.println("EL-Client sync failed!");
		libSlipState = LIBSLIP_PRE_INIT;
	}
	else
	{
		
	}
}
void libslip_init()
{
	for(int i = 0; i < LIBSLIP_MAX_CLIENTS;i++)
		activeConnections[i].enabled = false;
	libSlipState = LIBSLIP_PRE_INIT;
}

/*
extern "C" unsigned int libslip_getState()
{
	return libSlipState;
}*/

void libslip_process()
{

	switch(libSlipState)
	{
		case LIBSLIP_OFF:
		break;
		case LIBSLIP_PRE_INIT:
			libslip_handle_init();
		break;
		case LIBSLIP_SYNCING:
			libslip_handle_syncing();
		break;
		case LIBSLIP_WAITING_WIFI:
			libslip_handle_wait_wifi();
		break;
		
		case LIBSLIP_RUNNING:
		libslip_handle_running();
		break;
		
	}
	
	
	
	
}
	
	
	
	
