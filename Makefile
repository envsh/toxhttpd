# Makefile for toxhttpd

PREFIX ?= /usr/local
CFLAGS += -Wall -Wextra -g -I$(HOME)/devsys/include -Imongoose
LDFLAGS += -L$(HOME)/devsys/lib -ltoxcore -ltoxencryptsave -lsodium -lvpx -lopus -lpthread

SRC = main.c http_server.c tox_core.c event_queue.c push_sse.c push_ws.c json_util.c log.c bootstrap.c mongoose/mongoose.o
OBJ = $(SRC:.c=.o)
TARGET = toxhttpd

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o mongoose/*.o $(TARGET)

.PHONY: all clean