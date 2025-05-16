CC = gcc
CFLAGS = -Wall -g
LIBS = -ljpeg

TARGET = jpgtorgb
SRC = jpgtorgb.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET) *.o