CXXFLAGS= -g -O3  -D_GNU_SOURCE
PROGRAMS = sendfile recvfile  sendstream recvstream
LIB_FLAGS = -I.  -lpthread
SRC_FILE  = shoit.c shoit_network.c shoit_misc.c  shoit_progress.c

all: ${PROGRAMS}


sendfile: sendfile.c ${SRC_FILE}
	gcc ${CXXFLAGS} -o sendfile sendfile.c shoit_sender.c ${SRC_FILE} ${LIB_FLAGS}


recvfile:recvfile.c ${SRC_FILE} 
	gcc ${CXXFLAGS} -o recvfile recvfile.c shoit_receiver.c ${SRC_FILE} ${LIB_FLAGS}

sendstream: sendstream.c ${SRC_FILE}
	 gcc ${CXXFLAGS} -o sendstream sendstream.c shoit_sender.c ${SRC_FILE} ${LIB_FLAGS}


recvstream:recvstream.c ${SRC_FILE}
	 gcc ${CXXFLAGS} -o recvstream recvstream.c shoit_receiver.c ${SRC_FILE} ${LIB_FLAGS}



clean:
	rm -f *.o ${PROGRAMS}
