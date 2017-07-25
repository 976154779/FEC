/*----------------------------------------------------------------
File Name  : network.h
Author     : Winglab
Data	   : 2016-12-29
Description:
	Instruction: Use this module you can set some UDP sockets, 
sendrate-controllable  data or read size-specified datavia UDP.
------------------------------------------------------------------*/
#pragma once
#include"cross_platform.h"
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#ifdef OS_WINDOWS
	#pragma comment(lib,"ws2_32.lib")
#endif //OS_WINDOWS

#include"parameter.h"
/*
Name:	tokenBucket_t
Description: A structure to store the information of a token bucket
*/
typedef struct tokenBucket_t {
	uint32_t tokenNum;
	iClock_t produceTime;
	double limitFlowSpeed;
	uint32_t limitMaxFlowSpeed;
}tokenBucket_t;

/*
Name:	netAddr_t
Description: A structure to store the ip and port
*/
typedef struct netAddr_t
{
	char ip[16];	
	uint16_t port;
}netAddr_t;
/*
Name:	netInfo_t
Description: A structure to store the information of a socket
*/
typedef struct netInfo_t
{
	SOCKET hSocket;		// The handle of the socket
	uint16_t netID;		// The id of the socket in this module.
	tokenBucket_t tokenBucket;	// The structure of the token bucket for this socket
}netInfo_t;

/*
Name:	NetworkUDPInit
Description: 
and set a UDP socket
Parameter:
@_hostAddr   : The ip and port of host PC.
@_remoteAddr : The ip and port of remote PC.
Return: The socket descriptor that has been set.
*/
netInfo_t* NetworkUDPInit(const netAddr_t *_localAddr, const netAddr_t *_remoteAddr);
/*
Name:	NetworkUDPClose
Description: Close a UDP socket adn release the memory.
Parameter:
@_serSocket   : The structure of the socket information.
Return: 
*/
void NetworkUDPClose(netInfo_t *_serSocket);

/*
Name:	NetworkUDPSendLimit
Description: Send a serial of data, of which the length is specified and
the byte rate is limitted.
Parameter:
@_socket : The socket descriptor.
@_data : A pointer to the data.
@_len  : The length of data.
@_rate : The data rate of transmisstion. Byte per second.
Return: The return value of send() function of system.
*/
int NetworkUDPSendLimit(netInfo_t* _socket, void *_data, uint32_t _len, uint32_t _rate);

/*
Name:	NetworkUDPSend
Description: Send a serial of data, of which the length is specified.
Parameter:
@_socket : The socket descriptor.
@_data : A pointer to the data.
@_len  : The length of data.
Return: The return value of send() function of system.
*/
void NetworkUDPSend(netInfo_t* _socket, void *_data, uint32_t _len);

/*
Name:	NetworkUDPReceive
Description: Receive a serial of data.
Parameter:
@_socket : The socket descriptor.
@_data : A pointer to the data.
@_len  : The length of data.
Return: -1 when meet error
*/
int NetworkUDPReceive(netInfo_t *_socket, void *_data, uint32_t _len);
int _GetToken(tokenBucket_t *_bucket, uint32_t _need);

/*
Name:	_GetWaitTime
Description: To calculate the time of sleep.
@_bucket: The structure of the token bucket.
@_need	: The number of tokens needed to be comsumed.
Return: The time of sleep(microsecond).
*/
long _GetWaitTime(tokenBucket_t *_bucket, uint32_t _need);