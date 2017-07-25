/*----------------------------------------------------------------
File Name  : io.c
Author     : Winglab
Data	   : 2016-12-29
Description:
	The interface between the source data and this program. At
present, UDP interfaces are supported to input the source data
to encode or output the decoded data.
------------------------------------------------------------------*/
#pragma once

#include"cross_platform.h"

#include<assert.h>
#include<string.h>
#include<stdlib.h>
#include<stdbool.h>
#include"ringbuffer.h"
#include"parameter.h"
#include"network.h"
#include"iRaptorQ.h"

//#define _IO_TEST_   //if defined ,  the input and output data is artificial data

#define IO_INPUT_RBUFF_NUMBER	(30000)
#define IO_OUTPUT_RBUFF_NUMBER	(30000)
#define IO_RTCP_Back_RBUFF_NUMBER	(3000)//edited by fanli

enum PUT_BLOCK
{
	PUT_BLOCK_START,		// start to put a block in to output buffer
	PUT_BLOCK_CURRENT,		// put a packet is id is expected, otherwise it may be stored in array
	PUT_BLOCK_REST			// put the rest of packets.
};

typedef struct IOPacket_t{
	char flag;
	uint16_t len;
#ifdef IO_NETWORK_MODE
	uint8_t  data[SYMBOL_SIZE+sizeof(iClock_t)];
#else
	uint8_t  data[SYMBOL_SIZE];
#endif
}IOPacket_t;
typedef struct IODataInfo_t {
	uint32_t len;
	uint8_t  data[SYMBOL_SIZE];
}IODataInfo_t;

typedef struct Data_t {
	uint32_t id;
	uint32_t len;
	iClock_t ts;

	uint8_t data[600];

}Data_t;
typedef struct DataFb_t {
	uint32_t id;
	iClock_t ts;
}DataFb_t;

/*
Name:	IOInputInit
Description: Initialize the Input interface.
Parameter:
Return: 
*/
void IOInputInit();

/*
Name:	IOOutputInit
Description: Initialize the Output interface.
Parameter:
Return:
*/
void IOOutputInit();


void RTCP_Forward_Send_Init();

void RTCP_Forward_Recv_Init();
//edited by fanli
void RTCP_Back_Send_Init();

//edited by fanli
void RTCP_Back_Recv_Init();
/*
Name:	IOInputDeInit
Description: Stop and destory the Input interface.
Parameter:
Return:
*/
void IOInputDeInit();

/*
Name:	IOOutputDeInit
Description: Stop and destory the Output interface.
Parameter:
Return:
*/
void IOOutputDeInit();

/*
Name:	IOInputDataGetBlock
Description: Get a block of data from the Input interface.
Parameter:
@_pBlock : a pointer store one block of data.
Return:
*/
uint32_t IOInputDataGetBlock(ringBuffer_t *_pRb, uint8_t* _pBlock, uint32_t _tag, uint8_t _foreBSN);

/*
Name:	IOOutputDataPutBlock
Description: Output a block of data to the Output interface.
Parameter:
@_pBlock : a pointer store one block of data.
Return:
*/
uint32_t IOOutputDataPutBlock(int _command, uint8_t* _pData, uint32_t blockSymbolNumber);
void IOOutputDataPutSymbol(uint8_t *pPut);

void IO_InputData(IODataInfo_t *_dataInfo);
void IO_OutputData(IODataInfo_t *_dataInfo);
