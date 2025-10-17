CC = clang
CFLAGS = -Wall -Wextra -O2

TARGET = server
SRC = main.c http.c

# Detect platform (Linux vs macOS)
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    OPENSSL_DIR := $(shell brew --prefix openssl 2>/dev/null)
    ifneq ($(OPENSSL_DIR),)
        OPENSSL_FLAGS := -I$(OPENSSL_DIR)/include -L$(OPENSSL_DIR)/lib
    endif
endif

# Default rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(OPENSSL_FLAGS) -o $(TARGET) $(SRC) -lcrypto

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
