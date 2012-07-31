CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-llo -L./lib/
IFLAGS=-I./include/
SRC=tuiod.c
OBJS=$(SRC:.c=.o)
IDIR=./include/
LDIR=./lib/
EXE=./lib/tuiod


all: $(SRC) $(EXE)

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(IFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(EXE)
	
