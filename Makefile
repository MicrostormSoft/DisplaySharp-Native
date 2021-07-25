libdisplaysharp.so:libdisplaysharp.c
	gcc libdisplaysharp.c -I /usr/include/libdrm/ -ldrm -c -fPIC -o libdisplaysharp.so

.PHONY: install
install:libdisplaysharp.so
	install libdisplaysharp.so /usr/lib/