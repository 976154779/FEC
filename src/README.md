# README #

This program is the windows version of STREAMING project, it can send video 
data that encoded by raptorQ via UDP protocal. The INPUT interface can acqurie 
data from socket and store the data in ring buffer. Then the ENCODERs can get 
and encode the data and generate the repair packets. Meanwhile, the TRANSMITTER 
sends the data to the RECEIVERs. When the RECEIVER acquire the data, they store 
the data in ring buffer, count the number of received packets of relative tag 
and acknowledge a feedback packets to the TRANSMITTER. Then the DECODERs 
decoder the received data and put it to the OUTPUT interface. At last, the 
OUTPUT interface send out the data by socket.

### DEPENDENCY ###

libRaptorQ v0.1.9  for windows  

### USER GUIDE ###

1. transmitter
Use the function IOInputInit() to open the input interface.
Use the function TransmitterInit() to start the transmitter.
Use the function TransmitterClose() to stop the transmitter and release the memory.
Use the function IOInputDeInit() to close the input interface and release the memory.

2. receiver
Use the function IOOutputInit() to open the output interface.
Use the function ReceiverInit() to start the receiver.
Use the function ReceiverClose() to stop the receiverand release the memory.
Use the function IOOutputDeInit() to close the output interface and release the memory.

3. parameter
The parameters are defined in parameter.h 

### SOURCE TREE ###

FountainVedio
|-buffersink.c		prepaire initialized empty ringbuffers automaticly.
|-buffersink.h		
|-err.h			formatting the log and error information.
|-io.c			data input and output interface.
|-io.h	
|-iRaptorQ.c		the middle interface to libRaptorQ.
|-iRaptorQ.h
|-network.c		manage the socket connection and data transmittion.
|-network.h
|-parameter.h		parameters of the program.
|-receiver.c		receive data and decode data 
|-receiver.h
|-ringbuffer.c		ring buffer
|-ringbuffer.h
|-transmitter.c		encode data and transmit data
|-transmitter.h
|-test.c		test file

###  Function Discription  ###
---------IO---------
River Mode: This mode may waste some bit of bandwidth, but it ensure
the source symbol can be constructed no longer than 1ms, though the
symbol may fill some useless byte with a head flag 'F'. 
	
Direct Distribute: source symbol will be transmitted first, and the 
receiver can output the source symbol directly if the id is arrived 
sequentially.

###  Upgrade Log ###
V1.0 	
An avilable version for straming transmitting.
-------------------------------------------------
V1.1	
This version fix some error,for example, time stamp. 
Now the video can be played smoothly. What's more, the 
RaptorQ is compiled as a static library, which can reduce
the delay of the generation of the first repair symbol.
