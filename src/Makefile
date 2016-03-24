
CC=gcc
CXX=g++
CXXFLAGS=-std=gnu++98 -pedantic -Wall -g -O0 -ftrapv -fstrict-aliasing -Wstrict-aliasing
CFLAGS=-std=gnu99 -pedantic -Wall -g -O0 -ftrapv -fstrict-aliasing -Wstrict-aliasing
LDFLAGS=-lcrypto substrings/libsubstrings.a

all: config action main package db files
	$(MAKE) -C substrings static
	$(CXX) config.o action.o main.o package.o db.o files.o $(LDFLAGS) -o ../packrat
config:
	$(CC) -c $(CFLAGS) config.c 
main:
	$(CC) -c $(CFLAGS) main.c 
package:
	$(CC) -c $(CFLAGS) package.c 
action:
	$(CC) -c $(CFLAGS) action.c 
db:
	$(CC) -c $(CFLAGS) db.c 
files:
	$(CC) -c $(CFLAGS) files.c 

clean:
	rm -f *.o *.gch packrat
	$(MAKE) -C substrings clean
