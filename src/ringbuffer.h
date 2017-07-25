/*----------------------------------------------------------------
File Name  : ringbuffer.c
Author     : Winglab
Data	   : 2016-12-29
Description:
	You can use this module to create and initialize some ring 
buffers and get or put data to the buffer.
------------------------------------------------------------------*/
#pragma once

#include"cross_platform.h"

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
/*
Name:	ringBuffer_t
Description: The data structure of ring buffer
*/
typedef struct ringBuffer_t
{
	uint32_t number;				// the number of storage unit in ring buffer 
	uint32_t head, tail;			// the index points to the head node and tail node.	
	uint32_t unitSize;				// the size of the storage unit
	iMutex_t hMutexHead;			// lock the operation on the buffer
	iMutex_t hMutexTail;	
	iSemaphore_t semEmpty;			// the singal related to the number of empty and occupied storage unit
	iSemaphore_t semOccupied;	
	void *buff[0];					// sotrage units  
}ringBuffer_t;

/*
Name:	RingBufferInit
Description: Allocate a ring buffer, of which the number and the
size of storage unit are specified by the parameter.
Parameter:
@_number: the number of storage unit in a ringbuffer.
@_size  : the size of each storage unit.
Return: A pointer to the allocated ring buffer.
*/
ringBuffer_t *RingBufferInit(uint32_t _number, uint32_t _sizeUnit);

/*
Name:	RingBufferPut
Description: Put data with specified length into a ring buffer.
Parameter:
@_pRb  : A pointer to the ringbuffer.
@_data : The address of data to be storeed in the ring buffer.
Return:
*/
void RingBufferPut(ringBuffer_t *_pRb, void *_data);
uint32_t RingBufferTryPut(ringBuffer_t *_pRb, void *_data);
/*
Name:	RingBufferPut
Description: Get data with specified length from a ring buffer.
Parameter:
@_pRb  : A pointer to the ringbuffer.
@_data : the address to stroe the got data.
Return:
*/
void RingBufferGet(ringBuffer_t *_pRb, void *_data);

/*
Name:	RingBufferTryGet
Description: Get data with specified length from a ring buffer.
If no data can be acquired, it will return immediately.
Parameter:
@_pRb  : A pointer to the ringbuffer.
@_data : the address to stroe the got data.
Return:
*/
uint32_t RingBufferTryGet(ringBuffer_t *_pRb, void *_data);

/*
Name:	RingBufferFlush
Description: Flush the data stored in the buffer
Parameter:
@_pRb  : A pointer to the ringbuffer.
Return:
*/
void RingBufferFlush(ringBuffer_t *_pRb);

/*
Name:	RingBufferDestroy
Description: Destory the ring buffer and release the memory
Parameter:
@_pRb  : A pointer to the ringbuffer.
Return:
*/
void RingBufferDestroy(ringBuffer_t *_pRb);
