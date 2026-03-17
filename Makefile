CC = clang
CFLAGS = -Wall -Wextra -O2

TARGET = server
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

pretty:
	clang-format -i *.c *.h
