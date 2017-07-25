src := transmitter.c receiver.c parameter.c io.c ringbuffer.c iRaptorQ.c buffersink.c network.c cross_platform.c

all:
	gcc -O3 -g -o LinuxOut/testSend testSend.c -I /home/xiong/projects/streamingwindows $(src) -lRaptorQ -lpthread
	gcc -O3 -g -o LinuxOut/testRecv testReceive.c -I /home/xiong/projects/streamingwindows $(src) -lRaptorQ -lpthread
clean:
	rm -rf testRaptorQ *~ 

