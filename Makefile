# 编译器
CC = g++

# 编译标志
CFLAGS = -Wall -std=c++11

# FFmpeg 包含路径和库
FFMPEG_INC = -I/usr/include
FFMPEG_LIB = -lavformat -lavcodec -lavutil -lswscale -lswresample -ldrm -lpthread -lz

# 输出可执行文件名称
TARGET = ffmpeg_example

# 源文件
SRCS = main.cpp

# 生成目标文件
OBJS = $(SRCS:.cpp=.o)

# 规则
all: $(TARGET) clean_objects

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(FFMPEG_LIB)

%.o: %.cpp
	$(CC) $(CFLAGS) $(FFMPEG_INC) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) output.pcm 

clean_objects:
	rm -f $(OBJS)
