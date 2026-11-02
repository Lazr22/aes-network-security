CC = gcc

CFLAGS = -Wall -Wextra -O2

LIBS = -lssl -lcrypto

BUILD = build

all: $(BUILD)/sender $(BUILD)/receiver $(BUILD)/device

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/sender: src/sender.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/receiver: src/receiver.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/device: src/device.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

clean:
	rm -rf $(BUILD)
