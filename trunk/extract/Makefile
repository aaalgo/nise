TOOLS_DIR=/memex/wdong/src/nise-tools

CFLAGS= -I$(TOOLS_DIR)/include 
CXXFLAGS= -I$(TOOLS_DIR)/include
LDFLAGS= -L$(TOOLS_DIR)/lib64 -L$(TOOLS_DIR)/lib #-L/memex/wdong/tools/x86_64-Linux/lib64 -L/memex/wdong/tools/x86_64-Linux/lib

CC=gcc
CXX=g++ -std=c++0x
LD=g++
CFLAGS += -g -O3 -Wall  #-pg
CXXFLAGS += -g -O3 -fopenmp -Wall #-pg
LDFLAGS += -static -fopenmp
LDLIBS += -lPocoFoundation -lboost_program_options -lhighgui -lcvaux -lcv -lcxcore -lpng -ljpeg -ltiff -lz -lrt -lpthread -lgsl -lgslcblas -lm

.PHONY:	all clean

HEADER = *.h
COMMON = sift.o host.o imop.o mathop.o generic.o sift-wrapper.o ../common/nise.o

PROGS = extract

all:	$(PROGS)

extract:	extract.o $(COMMON)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

#$(PROGS): %:	%.o $(COMMON)
#	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o $(PROGS)

