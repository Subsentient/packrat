
CC=gcc
CXX=g++
CXXFLAGS=-std=gnu++98 -pedantic -Wall -g3 -O0 -ftrapv -fstrict-aliasing -Wstrict-aliasing -Wno-long-long -fstack-protector
CFLAGS=-std=gnu99 -pedantic -Wall -g -O3 -ftrapv -fstrict-aliasing -Wstrict-aliasing -fstack-protector
LDFLAGS=-lcrypto substrings/libsubstrings.a

all: sqlite config action main package db files pwsr
	$(MAKE) -C substrings static
	$(CXX) config.o action.o main.o package.o db.o files.o passwd_w_sysroot.o sqlite3/sqlite3.o $(LDFLAGS) -o ../packrat
	
fast: config action main package db files pwsr
	$(MAKE) -C substrings static
	$(CXX) config.o action.o main.o package.o db.o files.o passwd_w_sysroot.o sqlite3/sqlite3.o $(LDFLAGS) -o ../packrat
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
sqlite:
	@echo "Compiling SQLite, this can take a little bit..."
	$(CC) -std=gnu99 -pedantic -Wall -O3 sqlite3/sqlite3.c -c -o sqlite3/sqlite3.o -fno-strict-aliasing -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -fstack-protector

clean:
	rm -f *.o *.gch packrat sqlite3/*.o sqlite3/*.gch
	$(MAKE) -C substrings clean
