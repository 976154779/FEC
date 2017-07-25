/*----------------------------------------------------------------
File Name  : parameter.h
Author     : Winglab
Data	   : 2016-12-29
Description:
	setup the paramters for this program.

------------------------------------------------------------------*/
#pragma once

/* instruction */
#define DEBUG
//#define RELAY_MODE
#define IO_NETWORK_MODE
//#define IO_RTCP_INPUT  //edited by fanli Designed for input TRCPSender report to FEC 

#define DELAY_MEASURE	
#define PORT_SHIFT		(0)

/* socket number */
#define CHANNEL_NUMBER	(1)	
#define FEEDBACK_TX_NUMBER	(1)
#define FEEDBACK_RX_NUMBER	(2)

/*  network data rate  */
#define MB_PER_SEC		  (1048576.0)		// 1 MB/s = 1048576 byte/s
#define BYTE_RATE_DEFAULT (MB_PER_SEC/CHANNEL_NUMBER*2)		// MB/s

/*  ip address   */
#define IP_HOLOLENS		"10.0.1.71"
#define IP_ThinkPad		"10.0.1.79"
#define IP_WuPC			"10.0.1.59"
#define IP_WuDell		"10.0.1.76"
#define IP_BACK_LOOP	"127.0.0.1"
#define IP_WIN_HOSTA_PUBLIC		"54.193.0.253"
#define IP_WIN_HOSTA_PRIVATE	"172.31.14.234"
#define IP_WIN_HOSTB_PUBLIC		"54.64.23.209"
#define IP_WIN_HOSTB_PRIVATE	"172.31.23.235"
#define IP_TouchPC				"10.0.1.84"

/*  port         */
#define PORT_IO_INPUT			(PORT_SHIFT+1050)
#define PORT_IO_RTCP_INPUT		(PORT_SHIFT+1051)//edited by fanli
#define PORT_IO_OUTPUT			(PORT_SHIFT+1150)
#define PORT_IO_RTCP_OUTPUT		(PORT_SHIFT+1151)//edited by fanli
#define PORT_DATA_TX			(PORT_SHIFT+1220)
#define PORT_DATA_RX			(PORT_SHIFT+1320)
#define PORT_FEEDBACK_TX		(PORT_SHIFT+1450)
#define PORT_FEEDBACK_RX		(PORT_SHIFT+1550)
#define PORT_RELAY_TX			(PORT_SHIFT+1620)
#define PORT_RELAY_RX			(PORT_SHIFT+1720)
#define PORT_RELAY_FEEDBACK_TX	(PORT_SHIFT+1850)
#define PORT_RELAY_FEEDBACK_RX	(PORT_SHIFT+1950)

/*  --------IP Table --------- */
#define PC_USE_LOOP_BACK

#ifdef PC_USE_LOOP_BACK
//	#define IP_TX_0			IP_BACK_LOOP   //switch localloop or remote
	#define IP_TX_0			"10.0.1.57"
//	#define IP_RX_0			IP_BACK_LOOP
	#define IP_RX_0			"10.0.1.187"
#else
	#if 0	//Transmitter
	#define IP_TX_0			IP_WIN_HOSTA_PRIVATE 
	#define IP_RX_0			IP_WIN_HOSTB_PUBLIC
	#else   //Receiver
	#define IP_TX_0			IP_WIN_HOSTA_PUBLIC
	#define IP_RX_0			IP_WIN_HOSTB_PRIVATE
	#endif
#endif // PC_USE_LOOP_BACK

#define IP_RELAY		IP_BACK_LOOP

/* encoder/decoder number */
#define SOURCE_NUMBER			(2)
#define ENCODER_NUMBER		    (48)
#define DECODER_NUMBER			(48)
#define ENCODER_WINDOW_LEN		(48)
#define ENCODER_INFO_INDEX_LEN	(256)

/* Transmitter */
#define BLOCK_TIME				(10)
#define DATA_TX_NUMBER			(1)

/* receiver */
#define RECV_WINDOW_LEN			(64)  
#define DATA_RX_NUMBER			(1)
#define RECV_INFO_LIST_LEN		(ENCODER_INFO_INDEX_LEN)
/* raptorQ */
#define BLOCK_SYMBOL_NUMBER		(32)

#define SYMBOL_SIZE 			(1316)
#define SYMBOL_TIME				(3)//每个Symbol最多的拼接时长不超过SYMBOL_TIME(3ms)

#define MAX_SYMBOL_NUMBER		(32)//最大不超过MAX_SYMBOL_NUMBER个Symbol构成一个Block,

#define MAX_UDP_DATA_SIZE		(2000)//inputThread进程recv到的UDP数据报中数据的最大的大小   //edited by ZhangYong

//#define SYMBOL_SIZE 			(2632)
#define RTCP_SIZE 			(1316)//edited by fanli

#define BLOCK_SIZE		(BLOCK_SYMBOL_NUMBER * SYMBOL_SIZE)
#define MAX_MEM		(2 << 29)		// ??

#define OVERHEAD	(2)
/*  debug  */
#ifdef DEBUG
//----------------Transmitter.c------------------
//#define DEBUG_EncBSN
//#define DEBUG_FB
//#define DEBUG_TX
//#define DEBUG_ENC
//#define DEBUG_ENCSTART
//#define DEBUG_REPAIR_START
//#define DEBUG_ENC_REPAIR
//----------------Receiver.c------------------
#define DEBUG_RX
#define DEBUG_Decode
//#define DEBUG_DEC_START
//----------------IO.c------------------
//#define DEBUG_out_a
//#define DEBUG_out_b
//#define DEBUG_F_packet
//#define DEBUG_F_Flag
//#define DEBUG_TT
#define DEBUG_ONE_BLOCK
//#define DEBUG_IO_IN_DATA
#endif // DEBUG


