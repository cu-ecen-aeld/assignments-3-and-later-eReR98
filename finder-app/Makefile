SRC := writer.c
TARGET = writer
OBJS := $(SRC:.c=.o)
CFLAGS = -Wall

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
