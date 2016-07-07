
CC=gcc
CXX=g++
CXXFLAGS=-std=gnu++98 -pedantic -Wall -g3 -O0 -ftrapv -fstrict-aliasing -Wstrict-aliasing -Wno-long-long -fstack-protector
CFLAGS=-std=gnu99 -pedantic -Wall -g -O3 -ftrapv -fstrict-aliasing -Wstrict-aliasing -fstack-protector
LDFLAGS=-lcrypto substrings/libsubstrings.a -lsqlite3 -lcurl
#We use libcrypto to compute checksums, and sqlite is our database system.

all: config action main package db files pwsr web repos console
	$(MAKE) -C substrings static
	$(CXX) config.o action.o main.o package.o db.o files.o passwd_w_sysroot.o web.o repos.o console.o $(LDFLAGS) -o ../packrat
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
web:
	$(CXX) -c $(CXXFLAGS) web.cpp
repos:
	$(CXX) -c $(CXXFLAGS) repos.cpp
console:
	$(CXX) -c $(CXXFLAGS) console.cpp
clean:
	rm -f *.o *.gch packrat
	$(MAKE) -C substrings clean
