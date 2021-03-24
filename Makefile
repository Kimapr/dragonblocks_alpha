COMMON = array.o binsearch.o linkedlist.o map.o signal.o util.o types.o
SERVER = $(COMMON) server.o servercommands.o
CLIENT = $(COMMON) client.o clientcommands.o
LIBRARIES = -lpthread -lm
FLAGS = -g

ifdef RELEASE
FLAGS = -O3
endif

all: Dragonblocks DragonblocksServer

Dragonblocks: $(CLIENT)
	cc $(FLAGS) -o Dragonblocks $(CLIENT) $(LIBRARIES)

DragonblocksServer: $(SERVER)
	cc $(FLAGS) -o DragonblocksServer $(SERVER) $(LIBRARIES)

%.o: %.c
	cc $(FLAGS) -o $@ -c -Wall -Wextra -Wpedantic -Werror $<

clean:
	rm -rf *.o

clobber: clean
	rm -rf Dragonblocks DragonblocksServer
