SRC := aesdsocket.c
TARGET = aesdsocket
CFLAGS = -pthread -g -Wall -Werror
CC = gcc

make: $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(SRC) $(LDFLAGS)

all: 
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET)
