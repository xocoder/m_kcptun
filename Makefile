#
# use gmake in FreeBSD

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), FreeBSD)
	CPP=c++
else
	CPP=g++
endif

CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated -msse4.1 -msse2 -mssse3

DEBUG= -g 
RELEASE= -O2

C_SRCS := $(shell find src -name "*.c")
C_SRCS += $(shell find vendor/kcp -name "*.c")
C_SRCS += $(shell find vendor/m_net/src -name "*.c")
C_SRCS += $(shell find vendor/m_foundation/src -name "*.c")

CPP_SRCS := $(shell find src -name "*.cpp")
CPP_SRCS += vendor/cm256/cm256.cpp vendor/cm256/gf256.cpp

DIRS := $(shell find src -type d)
DIRS += $(shell find vendor/kcp -type d)
DIRS += $(shell find vendor/m_net/src -type d)
DIRS += $(shell find vendor/m_foundation/src -type d)
DIRS += $(shell find vendor/cm256 -type d)

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
