CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=tarsau.exe
SRC=tarsau.c

all: $(TARGET)

$(TARGET): $(SRC)
	gcc $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	del /Q $(TARGET)