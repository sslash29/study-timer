CC      ?= gcc
CFLAGS  ?= -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Wno-format-truncation -O2
LDLIBS  := -lncursesw

SRC_DIR := src
BUILD_DIR := build

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

TARGET := quir

.PHONY: all clean run test install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

test: $(BUILD_DIR)/test_quir
	./$(BUILD_DIR)/test_quir

$(BUILD_DIR)/test_quir: tests/test_quir.c $(SRC_DIR)/duration.c $(SRC_DIR)/dateutil.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -o $@ tests/test_quir.c $(SRC_DIR)/duration.c $(SRC_DIR)/dateutil.c

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
