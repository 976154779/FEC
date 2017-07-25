/*----------------------------------------------------------------
File Name  : err.h
Author     : Winglab
Data	   : 2016-12-29
Description:
	To print the log or error information with a defined format.
------------------------------------------------------------------*/
#pragma once
#include "cross_platform.h"

#include <stdio.h>

#define pr_error(fmt, ...)	fprintf(stderr, fmt, ##__VA_ARGS__)	// print the error information

#define pr_info(fmt, ...)	fprintf(stdout, fmt, ##__VA_ARGS__)	// print the log

#define debug(fmt, ...) do {		\
	printf("[%.3f]"fmt, (float)iGetTime()/1000, ##__VA_ARGS__);	\
} while (0)		// print the log with a time stamp

