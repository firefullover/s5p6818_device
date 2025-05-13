# 编译器和编译选项
CC = gcc
CFLAGS = -Wall -g

# 源文件列表（自动查找所有.c文件）
SRC = $(wildcard *.c)

# 头文件列表（自动查找所有.h文件，便于依赖管理）
HEADERS = $(wildcard *.h)

# 目标可执行文件名
TARGET = myprogram

# 链接库选项
LIBS = -pthread -lpaho-mqtt3c -ljpeg -lm -lcjson

# 默认目标
all: $(TARGET)

# 生成目标（添加头文件依赖）
$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# 清理目标
clean:
	rm -f $(TARGET) *.o