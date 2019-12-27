ledbadge: lib.c main.c arg.c
	gcc -o $@ $^ `pkg-config libpng --cflags` `pkg-config libpng --libs` `pkg-config hidapi-hidraw --cflags` `pkg-config hidapi-hidraw --libs` -Wall #`pkg-config libusb-1.0 --cflags` `pkg-config libusb-1.0 --libs` -Wall

