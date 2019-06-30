.PHONY: test libkvdb

test: libkvdb-test-64 libkvdb-test-32
	@echo "==== TEST 64 bit ===="
	@rm -f a.db b.db
	@LD_LIBRARY_PATH=.. ./libkvdb-test-64
	@echo "==== TEST 32 bit ===="
	@rm -f a.db b.db
	@LD_LIBRARY_PATH=.. ./libkvdb-test-32

libkvdb:
	@cd .. && make -s

libkvdb-test-64: main.c libkvdb
	gcc -I.. -L.. -m64 main.c -o libkvdb-test-64 -lkvdb-64 -pthread

libkvdb-test-32: main.c libkvdb
	gcc -I.. -L.. -m32 main.c -o libkvdb-test-32 -lkvdb-32 -pthread

clean:
	rm -f libkvdb-test-32 libkvdb-test-64 a.db b.db
