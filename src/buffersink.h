/*----------------------------------------------------------------
File Name  : buffersink.h
Author     : Winglab
Data	   : 2016-12-29
Description:
	 This module can initialize/destory a buffer sink which can 
store specific number of ring buffers. You can get an empty ring 
buffer from the buffer sink without waitting for the buffer to 
be initizlized because this progress has been processed by one 
thread of the buffer sink automaticly. Once the sink is not full,
it can be replenished immediately.
------------------------------------------------------------------*/
#pragma once

#include"cross_platform.h"

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include"ringbuffer.h"

//#define BUFFSINK_COUNT_MAX	16	// The max number of buffer sink that can be initialized.
/*
Name:	bufferSink_t
Description: The structure of the buffer sink.
*/
typedef struct bufferSink_t
{
	uint32_t		id;			// ID of this buffer sink.
	iThread_t		hWnd;		// Handle of the thread of this buffer sink.
	ringBuffer_t   *rbSink;		// A buffer to store the address of generated buffer.
	uint32_t		sinkLen;	// The max number of buffer in the sink.
	uint32_t		buffLen;	// The number of storage unit in a buffer.
	uint32_t		buffUnitSize;	// The size of one storage unit in buffer.
}bufferSink_t;

/*
Name:	BufferSinkInit
Description: Initialize a new buffer sink .
Parameter:
@_sinkLen: The max number of buffer in the sink.
@_buffLen: The number of storage unit in a buffer.
@_buffUnitSize: The size of one storage unit in buffer.
Return: the pointer of the structure of the buffer sink
*/
bufferSink_t *BufferSinkInit(uint32_t _sinkLen, uint32_t _buffLen, uint32_t _buffUnitSize);

/*
Name:	BufferSinkGetBuffer
Description: Get an address of a empty buffer, of which the parameters 
of the new  buffer is defined in _pSink.
Parameter: 
@_pSink: The structure of the buffer sink.
Return: The address of the empty buffer.
*/
ringBuffer_t *BufferSinkGetBuffer(bufferSink_t* _pSink);

/*
Name:	BufferSinkDestorySink
Description: Destory the specified buffer sink and release the memory.
Parameter: 
@_pSink: The structure of the buffer sink.
Return:
*/
void BufferSinkDestorySink(bufferSink_t *_pSink);
