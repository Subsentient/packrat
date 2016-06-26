all:
	$(MAKE) -C src all
fast:
	$(MAKE) -C src fast
clean:
	$(MAKE) -C src clean
	rm -f packrat
