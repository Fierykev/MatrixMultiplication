CFLAGS=-g -std=gnu11 -O3 -Werror -Wall -Wno-unused-variable -mcmodel=large

SOURCES = $(sort $(wildcard *.c *.S))
FILES_ = $(patsubst %.S,%.o,$(SOURCES))
FILES = $(patsubst %.c,%.o,$(FILES_)) 


LFLAGS = -lc -lX11 -lGL -lGLU -lglut -lGLEW -lm -lpthread

MF =  ./freeglut/lib/libglut.a

files: $(FILES)

main : Makefile files
	gcc $(CFLAGS) -o main $(FILES) $(LFLAGS)

%.o : %.c Makefile
	gcc $(CFLAGS) -MD -c $*.c

%.o : %.S Makefile
	gcc $(CFLAGS) -MD -c $*.S

run : main
	./main

test : run

clean :
	rm -f *.d
	rm -f *.o
	rm -f main
	rm -f freq.txt

-include *.d
