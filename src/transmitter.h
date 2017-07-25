/*----------------------------------------------------------------
File Name  : transmitter.c
Author     : Winglab
Data	   : 2016-12-29
Description:
	Receive the encoded data form transmitter and decode it and
send feedback to transmitter immediately.
------------------------------------------------------------------*/
#pragma once
#include"cross_platform.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<stdbool.h>
#include"network.h"
#include"ringbuffer.h"
#include"iRaptorQ.h"
#include"parameter.h"
#include"io.h"

/*
Name:	TransmitterInit
Description: Initialize the transmitter
Parameter:
Return:
*/
void TransmitterInit();

/*
Name:	TransmitterInit
Description: Close the transmitter and release the memory
Parameter:
Return:
*/
void TransmitterClose();
