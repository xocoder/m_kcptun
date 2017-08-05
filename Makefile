
CC=gcc
CFLAGS= -g -Wall -Wdeprecated-declarations

CPP=g++
CPPFLAGS= -g -Wall -Wdeprecated-declarations

C_SRCS := $(shell find src -name "*.c")
C_SRCS += $(shell find vendor/kcp -name "*.c")
C_SRCS += $(shell find vendor/m_net/src -name "*.c")

CPP_SRCS := $(shell find src -name "*.cpp")

DIRS := $(shell find src -type d)
DIRS += $(shell find vendor/kcp -type d)
DIRS += $(shell find vendor/m_net/src -type d)


INCS := $(foreach n, $(DIRS), -I$(n))

all: local_kcp.out remote_kcp.out

local_kcp.out: $(C_SRCS) $(CPP_SRCS)
	$(CPP) $(CPPFLAGS) $(INCS) -o $@ $^ $(LIBS) -DLOCAL_KCP

remote_kcp.out: $(C_SRCS) $(CPP_SRCS)
	$(CPP) $(CPPFLAGS) $(INCS) -o $@ $^ $(LIBS) -DREMOTE_KCP

clean:
	rm -rf *.out *.out.dSYM
