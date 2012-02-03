CC=g++
CXX=g++
LD=g++
CFLAGS += -g  -Wall -fopenmp -O3
#-O3 -fopenmp
LDFLAGS += -static  -fopenmp
LDLIBS += -lboost_program_options#-llshkit -lwthttp -lwtext -lwt -lboost_regex -lboost_thread -lboost_filesystem -lboost_program_options -lboost_date_time -lboost_signals -lboost_system -lgomp -lpthread -lz -lrt -lhighgui -lcvaux -lcv -lcxcore -lpng -ljpeg -ltiff -lz -lrt

.PHONY:	all clean

HEADER = *.h
COMMON = 

PROGS = fbi-run manku-run fbi-trie #fbi-exp

all:	$(PROGS)

%.o:	%.cpp $(HEADER)
	$(CXX) $(CFLAGS) -c -o $@ $*.cpp

$(PROGS): %:	%.o $(COMMON)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm *.o $(PROGS)

