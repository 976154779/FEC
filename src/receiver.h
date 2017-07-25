/*----------------------------------------------------------------
File Name  : receiver.h
Author     : Winglab
Data	   : 2016-12-29
Description:
	Encode the source data and transmit via UDP toreceiver. 
Check the feedback and coordinate the encode and transmitte 
pace.

------------------------------------------------------------------*/
#pragma once
#include"cross_platform.h"

#include<stdio.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<assert.h>
#include<stdbool.h>

#include"network.h"
#include"ringbuffer.h"
#include"iRaptorQ.h"
#include"parameter.h"
#include"io.h"
#include"buffersink.h"

/*
Name:	ReceiverInit
Description: Initialize the Receiver.
Parameter:
Return:
*/
void ReceiverInit();

/*
Name:	ReceiverClose
Description: Close the Receiver and release the memory.
Parameter:
Return:
*/
void ReceiverClose();