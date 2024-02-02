# Simple makefile for creating writer. Comment/uncomment CROSS_COMPILE to choose which architecture to compile for
SRC := writer.c
TARGET = writer
CFLAGS = -g -Wall
CC = gcc
#CROSS_COMPILE = aarch64-none-linux-gnu-
CROSS_COMPILE =

make: $(SRC)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	-rm -f *.o $(TARGET)
