C++ = g++

CCFLAGS = -Wall -I../src -O2 -finline-functions

ifeq ($(arch), IA32)
   CCFLAGS += -mcpu=pentiumpro -march=pentiumpro -mmmx -msse -DIA32
endif

ifeq ($(arch), POWERPC)
   CCFLAGS += -mcpu=powerpc
endif

LDFLAGS = -lstdc++ -lpthread -lm -L../src -ludt

ifeq ($(os), UNIX)
   LDFLAGS += -lsocket
endif

OBJS = appserver.o appclient.o sendfile.o recvfile.o

DIR = $(shell pwd)

all: appserver appclient sendfile recvfile

%.o: %.cpp
	$(C++) $(CCFLAGS) $< -c

appserver: appserver.o
	$(C++) $(LDFLAGS) $^ -o $@
appclient: appclient.o
	$(C++) $(LDFLAGS) $^ -o $@
sendfile: sendfile.o
	$(C++) $(LDFLAGS) $^ -o $@
recvfile: recvfile.o
	$(C++) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o
	rm appserver appclient sendfile recvfile

install:
	export PATH=$(DIR):$$PATH
