# Makefile for toxhttpd

# TOXPFX = /usr
# TOXPFX = $(HOME)/devsys
TOXPFX = /opt/oldlibc-devsys
PREFIX ?= /usr/local
CFLAGS += -std=c99 -D_GNU_SOURCE -Wall -Wextra -g -fPIE -I$(TOXPFX)/include -Imongoose
LDFLAGS += -L$(TOXPFX)/lib -ltoxcore -ltoxencryptsave -lsodium -lvpx -lopus -lpthread

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
