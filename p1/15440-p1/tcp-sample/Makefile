PROGS=client server
CFLAGS+=-Wall

all: $(PROGS) mylib.so

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c -I../include -L../lib

mylib.so: mylib.o package_gen.o
	ld -shared -o mylib.so mylib.o package_gen.o -ldl -I../include -L../lib

server: package_process.o server.o 
	gcc  package_process.o server.o -o server  -lm  -I../include -L../lib -ldl -ldirtree

server.o: server.c package_process.h
	gcc -Wall -fPIC -DPIC -c server.c -I../include -L../lib

package_process.o: package_process.c package_process.h
	gcc -Wall -fPIC -DPIC -c package_process.c -I../include -L../lib

package_gen.o: package_gen.c package_gen.h
	gcc -Wall -fPIC -DPIC -c package_gen.c

clean:
	rm -f *.o $(PROGS)
	rm -f *.o *.so

handin:
	tar czvf ../mysolution.tgz Makefile *.c *.h *.pdf

