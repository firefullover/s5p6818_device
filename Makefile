# 编译器和编译选项
CC = gcc
CFLAGS = -Wall -g

# 当前文件夹中的所有 .c 文件
SRC = $(wildcard *.c)

# 目标可执行文件名
TARGET = myprogram

# 生成目标
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# 清理目标
clean:
	rm -f $(TARGET)
