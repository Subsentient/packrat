
CC=gcc
CXX=g++
CXXFLAGS=-std=gnu++98 -pedantic -Wall -g3 -Og -ftrapv -fstrict-aliasing -Wstrict-aliasing
CFLAGS=-std=gnu99 -pedantic -Wall -g -Og -ftrapv -fstrict-aliasing -Wstrict-aliasing
LDFLAGS=-lcrypto substrings/libsubstrings.a

all: config action main package db files pwsr
	$(MAKE) -C substrings static
	$(CXX) config.o action.o main.o package.o db.o files.o passwd_w_sysroot.o $(LDFLAGS) -o ../packrat
config:
	$(CXX) -c $(CXXFLAGS) config.cpp
main:
	$(CXX) -c $(CXXFLAGS) main.cpp 
package:
	$(CXX) -c $(CXXFLAGS) package.cpp 
action:
	$(CXX) -c $(CXXFLAGS) action.cpp 
db:
	$(CXX) -c $(CXXFLAGS) db.cpp 
files:
	$(CXX) -c $(CXXFLAGS) files.cpp 
pwsr:
	$(CXX) -c $(CXXFLAGS) passwd_w_sysroot.cpp 

clean:
	rm -f *.o *.gch packrat
	$(MAKE) -C substrings clean
