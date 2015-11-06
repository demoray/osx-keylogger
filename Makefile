BIN = keystrokes

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
CC=clang
LD=clang

CFLAGS += -pedantic -Weverything
LIBS += -framework ApplicationServices -lsqlite3

all: $(BIN)

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(BIN) $(OBJS)
