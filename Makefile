# 编译器和编译选项
CC = gcc
CFLAGS = -Wall -g

# 源文件列表（明确指定）
SRC = $(wildcard *.c)

# 链接库选项（修正后的）
LIBS = -pthread -lpaho-mqtt3c -ljpeg -lm

# 目标可执行文件名
TARGET = myprogram

# 生成目标（添加文件依赖关系）
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# 清理目标（添加中间文件清理）
clean:
	rm -f $(TARGET) *.o