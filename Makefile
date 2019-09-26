CXXFLAGS= -Wall -O3  -D_GNU_SOURCE
PROGRAMS = sendfile recvfile  sendstream recvstream
LIB_FLAGS =  -lpthread
INCLUDES   = -I.
OBJ = shoit.c shoit_network.o shoit_misc.o  shoit_progress.o shoit_sender.o shoit_receiver.o

all: ${PROGRAMS}

.c.o:
	gcc ${CXXFLAGS} -c $< -o $@

sendfile: sendfile.o ${OBJ}
	gcc ${CXXFLAGS} $^ -o $@ ${LIB_FLAGS} ${INCLUDES}


recvfile:recvfile.o ${OBJ} 
	gcc ${CXXFLAGS}  $^ -o $@ ${LIB_FLAGS} ${INCLUDES}

sendstream: sendstream.o ${OBJ}
	 gcc ${CXXFLAGS}  $^ -o $@ ${LIB_FLAGS} ${INCLUDES}


recvstream:recvstream.o ${OBJ}
	 gcc ${CXXFLAGS}  $^ -o $@ ${LIB_FLAGS} ${INCLUDES}


clean:
	rm -f *.o ${PROGRAMS}
