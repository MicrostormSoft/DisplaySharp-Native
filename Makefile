libdisplaysharp.o:libdisplaysharp.c
	gcc -c -fPIC -I /usr/include/libdrm/ -o libdisplaysharp.o libdisplaysharp.c

libdisplaysharp.so:libdisplaysharp.o
	gcc -shared -o libdisplaysharp.so -ldrm -lm libdisplaysharp.o

.PHONY: install
install:libdisplaysharp.so
	install libdisplaysharp.so /usr/lib/