/*----------------------------------------------------------------
File Name  : network.h
Author     : Winglab
Data	   : 2016-12-29
Description:
Instruction: Use this module you can set some UDP sockets,
sendrate-controllable  data or read size-specified datavia UDP.
------------------------------------------------------------------*/
#include"cross_platform.h"
#include"network.h"
#ifdef DELAY_MEASURE
#include"iRaptorQ.h"
#endif // DELAY_MEASURE

static int socketCount = 0;		// count the number of initialized sockets

void _PreduceToken(tokenBucket_t *_bucket);

/*
Name:	_PreduceToken
Description: Function for token bucket. Preduce the token.
Parameter:
@_bucket   : The structure of the token bucket.
Return: 
*/
void _PreduceToken(tokenBucket_t *_bucket)
{
	if (_bucket->tokenNum == _bucket->limitMaxFlowSpeed){
		return;
	}

	int addTokenNum = 0; //

	if (_bucket->produceTime == 0){
		_bucket->produceTime = iGetTime();
		addTokenNum = (int)(_bucket->limitFlowSpeed);
	}else{
		iClock_t curTime = iGetTime();
		addTokenNum = (int)((curTime - (_bucket->produceTime)) * _bucket->limitFlowSpeed);
		if (addTokenNum > 0){
			_bucket->produceTime = curTime;
		}
	}

	_bucket->tokenNum = _bucket->tokenNum + addTokenNum;
	if (_bucket->tokenNum > _bucket->limitMaxFlowSpeed){
		_bucket->tokenNum = _bucket->limitMaxFlowSpeed;
	}
}

/*
Name:	_GetToken
Description: Function for token bucket. Get the token.
Parameter:
@_bucket: The structure of the token bucket.
@_need	: The number of tokens needed to be comsumed.
Return: '0' if the bucket is empty 
*/
int _GetToken(tokenBucket_t *_bucket, uint32_t _need)
{
	_PreduceToken(_bucket);

	if (_bucket->tokenNum< _need){
		return 0;
	}
	_bucket->tokenNum -= _need;
	return 1;
}

/*
Name:	_GetWaitTime
Description: To calculate the time of sleep.
@_bucket: The structure of the token bucket.
@_need	: The number of tokens needed to be comsumed.
Return: The time of sleep(microsecond).
*/
long _GetWaitTime(tokenBucket_t *_bucket, uint32_t _need) {
	_PreduceToken(_bucket);
	if (_bucket->tokenNum >= _need) {
		return 0;
	}
	uint32_t tokens_needed = _need - _bucket->tokenNum;
	uint32_t estimate_milli = (1000 * tokens_needed) / (uint32_t)_bucket->limitFlowSpeed;
	estimate_milli += ((1000 * tokens_needed) % (uint32_t)(_bucket->limitFlowSpeed)) ? 1 : 0;
	return estimate_milli;
}

/*
Name:	NetworkUDPInit
Description: Create and set a UDP socket
Parameter:
@_hostAddr   : The ip and port of host PC.
@_remoteAddr : The ip and port of remote PC.
Return: The socket descriptor that has been set.
*/
netInfo_t* NetworkUDPInit(const netAddr_t *_localAddr, const netAddr_t *_remoteAddr)
{
	
#ifdef OS_WINDOWS
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	/*    WSAStartup          */
	if (WSAStartup(sockVersion, &wsaData) != 0) return 0;
#endif
	/*    Create socket       */
	SOCKET serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serSocket == -1){
		printf("socket error ! (%s:%d; %s:%d)\r\n", _localAddr->ip, _localAddr->port, _remoteAddr->ip, _remoteAddr->port);
		exit(1);
	};

	/*    Bind local address    */
	if (_localAddr != NULL) {
		SOCKADDR_IN localAddr;
		memset((void*)&localAddr,0,sizeof(localAddr));
		localAddr.sin_family = AF_INET;
		localAddr.sin_port = htons(_localAddr->port);
#ifdef OS_WINDOWS		
		localAddr.sin_addr.S_un.S_addr = inet_addr(_localAddr->ip);
#endif
#ifdef OS_LINUX 
		localAddr.sin_addr.s_addr = inet_addr(_localAddr->ip);
#endif	
		if (bind(serSocket, (SOCKADDR *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
			printf("bind error ! local:(%s:%d)\r\n", _localAddr->ip, _localAddr->port);
			iCloseSocket(serSocket);
			exit(1);
		}
	}
	/*    Connect remote address    */
	if(_remoteAddr != NULL){
		SOCKADDR_IN remoteAddr;
		memset((void*)&remoteAddr, 0, sizeof(remoteAddr));
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_port = htons(_remoteAddr->port);

#ifdef OS_WINDOWS
		remoteAddr.sin_addr.S_un.S_addr = inet_addr(_remoteAddr->ip);
#endif // OS_WINDOWS
#ifdef OS_LINUX
		remoteAddr.sin_addr.s_addr = inet_addr(_remoteAddr->ip);
#endif // OS_LINUX

		if (connect(serSocket, (SOCKADDR *)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR) {
			printf("connect error ! (%s:%d; %s:%d)\r\n", _localAddr->ip, _localAddr->port, _remoteAddr->ip, _remoteAddr->port);
			iCloseSocket(serSocket);
			exit(1);
		}
	}

	netInfo_t* netInfo = (netInfo_t*)malloc(sizeof(netInfo_t));
	netInfo->hSocket = serSocket;
	netInfo->netID = socketCount;
	/* token bucket initialize */
	netInfo->tokenBucket.tokenNum = 0;
	netInfo->tokenBucket.produceTime = 0;
	netInfo->tokenBucket.limitFlowSpeed = BYTE_RATE_DEFAULT/1000;
	netInfo->tokenBucket.limitMaxFlowSpeed = (uint32_t)BYTE_RATE_DEFAULT;

	socketCount++;

	return netInfo;
}

/*
Name:	NetworkUDPClose
Description: Close a UDP socket adn release the memory.
Parameter:
@_serSocket   : The structure of the socket information.
Return:
*/
void NetworkUDPClose(netInfo_t *_serSocket)
{
	if (socketCount > 0){

		iCloseSocket(_serSocket->hSocket);

#ifdef OS_WINDOWS
		WSACleanup();
#endif
		free(_serSocket);
		_serSocket = NULL;
		socketCount--;
	}
}

/*
Name:	NetworkUDPSend
Description: Send a serial of data, of which the length is specified.
Parameter:
@_socket : The socket descriptor.
@_data : A pointer to the data.
@_len  : The length of data.
Return: The return value of send() function of system.
*/
void NetworkUDPSend(netInfo_t *_socket, void *_data, uint32_t _len)
{
	send(_socket->hSocket, (char*)_data, _len, 0);
}

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
int NetworkUDPSendLimit(netInfo_t *_socket, void *_data, uint32_t _len, uint32_t _rate)
{ 
	while (_GetToken(&(_socket->tokenBucket), _len) == 0) {
		#ifdef OS_WINDOWS
				_USleep(_GetWaitTime(&(_socket->tokenBucket), _len));
		#endif
		#ifdef OS_LINUX
				usleep(_GetWaitTime(&(_socket->tokenBucket), _len));
		#endif
	}
#ifdef DELAY_MEASURE
		raptorQPacket_t* packet = (raptorQPacket_t*)_data;
		packet->ts[0] = iGetTime();
#endif // DELAY_MEASURE
	return send(_socket->hSocket, (char*)_data, _len, 0);
} 

/*
Name:	NetworkUDPReceive
Description: Receive a serial of data.
Parameter:
@_socket : The socket descriptor.
@_data : A pointer to the data.
@_len  : The length of data.
Return: The return value of recv() function of system.接收到的数据的大小
*/
int NetworkUDPReceive(netInfo_t *_socket, void *_data, uint32_t _len)
{
	return recv(_socket->hSocket, (char*)_data, _len, 0) ;
}
