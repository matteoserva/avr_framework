#ifndef _LIBSLIP_H
#define _LIBSLIP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif



enum LibSlipSockMode
{
	LIBSLIP_TCP_CLIENT = 0,
	LIBSLIP_TCP_CLIENT_LISTEN,
	LIBSLIP_TCP_SERVER,
	LIBSLIP_UDP
};

enum LibSlipResult
{
	LIBSLIP_ERROR,
	LIBSLIP_SENT,
	LIBSLIP_RECEIVED
};


typedef void (*libslip_cb_t)(uint8_t sockFd,enum LibSlipResult result, uint8_t* data, uint16_t len );

void libslip_init();

int libslip_connect(const char* host, uint16_t port, enum LibSlipSockMode sock_mode,libslip_cb_t _userCb);

void libslip_send(int sockFd, void* data, int len);

void libslip_process();



#ifdef __cplusplus
}
#endif


#endif