.PHONY: build
libdisplaysharp.so:libdisplaysharp.c
	gcc -c -fPIC -I /usr/include/libdrm/ -o libdisplaysharp.o libdisplaysharp.c
	gcc -shared -o libdisplaysharp.so -ldrm -lm libdisplaysharp.o

.PHONY: install
install:libdisplaysharp.so
	install libdisplaysharp.so /usr/lib/
