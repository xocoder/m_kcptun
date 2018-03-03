#
# use gmake in FreeBSD

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), FreeBSD)
	CC=cc
	CPP=c++
else
	CC=gcc
	CPP=g++
endif

CFLAGS= -Wall -Wdeprecated-declarations
CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated

DEBUG= -g
RELEASE= -O2

C_SRCS := $(shell find src -name "*.c")
C_SRCS += $(shell find vendor/kcp -name "*.c")
C_SRCS += $(shell find vendor/m_net/src -name "*.c")
C_SRCS += $(shell find vendor/m_foundation/src -name "*.c")

CPP_SRCS := $(shell find src -name "*.cpp")

DIRS := $(shell find src -type d)
DIRS += $(shell find vendor/kcp -type d)
DIRS += $(shell find vendor/m_net/src -type d)
DIRS += $(shell find vendor/m_foundation/src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

all: debug

debug: $(C_SRCS) $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o local_kcp.out $^ $(LIBS) -DLOCAL_KCP
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o remote_kcp.out $^ $(LIBS) -DREMOTE_KCP

release: $(C_SRCS) $(CPP_SRCS)
	$(CPP) $(RELEASE) $(CPPFLAGS) $(INCS) -o local_kcp.out $^ $(LIBS) -DLOCAL_KCP
	$(CPP) $(RELEASE) $(CPPFLAGS) $(INCS) -o remote_kcp.out $^ $(LIBS) -DREMOTE_KCP

clean:
	rm -rf *.out *.out.dSYM
