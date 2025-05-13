CC = gcc
CFLAGS = -Wall -g
LIBS = -ljpeg

TARGET = jpg2rgb
SRC = jpg2rgb.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET) *.o