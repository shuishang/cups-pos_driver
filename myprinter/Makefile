MAKEFLAGS += --no-print-directory

CC := gcc

CCFLAGS = `cups-config --cflags` -Wall

LDFLAGS = `cups-config --libs` -lcupsimage -lcups 

target := texttokc rastertokc commandtokc

objects := texttokc.o rastertokc.o commandtokc.o

src := texttokc.c rastertokc.c commandtokc.c

.PHONY: all
all: $(target)

texttokc: texttokc.o
	$(CC) -o texttokc $(CCFLAGS) texttokc.c $(LDFLAGS)

rastertokc: rastertokc.o
	$(CC) -o rastertokc $(CCFLAGS) rastertokc.c $(LDFLAGS)

commandtokc: commandtokc.o
	$(CC) -o commandtokc $(CCFLAGS) commandtokc.c $(LDFLAGS)


.PHONY: clean
clean:
	rm -rf $(target) $(objects) *~
