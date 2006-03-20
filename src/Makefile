C++ = g++

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif

CCFLAGS = -fPIC -Wall -D$(os) -finline-functions #-O3

CCFLAGS += -DTRACE
CCFLAGS += -DNO_BUSY_WAITING
#CCFLAGS += -DCUSTOM_CC
#CCFLAGS += -DCAPI
#CCFLAGS += -DNOISY_LINK

ifeq ($(arch), IA32)
   CCFLAGS += -DIA32 #-mcpu=pentiumpro -march=pentiumpro -mmmx -msse
endif

ifeq ($(arch), POWERPC)
   CCFLAGS += -mcpu=powerpc
endif

ifeq ($(arch), SPARC)
   CCFLAGS += -mcpu=sparc
endif

ifeq ($(arch), IA64)
   CCFLAGS += -DIA64
endif

ifeq ($(arch), AMD64)
   CCFLAGS += -DAMD64
endif

OBJS = common.o list.o buffer.o packet.o channel.o udt.o api.o socket.o
DIR = $(shell pwd)

all: libudt.so libudt.a udt

%.o: %.cpp udt.h
	$(C++) $(CCFLAGS) $< -c

libudt.so: $(OBJS)
ifneq ($(os), BSD)
	$(C++) -shared -o $@ $^
else
	$(C++) -dynamiclib -o libudt.dylib -lstdc++ -lpthread -lm $^
endif

libudt.a: $(OBJS)
	ar -rcs $@ $^

udt:
	cp udt.h udt

clean:
	rm -f *.o *.so *.dylib *.a udt

install:
	export LD_LIBRARY_PATH=$(DIR):$$LD_LIBRARY_PATH
