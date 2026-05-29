CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-misleading-indentation -std=c11 -O2 -D_GNU_SOURCE
TARGET  = subzeroclaw

# Use system libcjson if available, otherwise use vendored copy
HAVE_SYSTEM_CJSON := $(shell pkg-config --exists libcjson 2>/dev/null && echo yes)
ifeq ($(HAVE_SYSTEM_CJSON),yes)
  LDFLAGS = -lcjson
else
  CFLAGS  += -Isrc
  VENDOR  = src/cJSON.c
  LDFLAGS = -lm
endif

# Build with mock/record module linked in. The non-weak llm_chat in
# szc_mock.c overrides the weak default in subzeroclaw.c.
$(TARGET): src/subzeroclaw.c src/szc_mock.c $(VENDOR)
	$(CC) $(CFLAGS) -o $(TARGET) src/subzeroclaw.c src/szc_mock.c $(VENDOR) $(LDFLAGS)

# Test: test.c includes subzeroclaw.c directly (SZC_TEST excludes main).
# szc_mock.c is also linked so the override path is exercised.
test: src/test.c src/subzeroclaw.c src/szc_mock.c $(VENDOR)
	$(CC) $(CFLAGS) -o test_subzeroclaw src/test.c src/szc_mock.c $(VENDOR) $(LDFLAGS)
	./test_subzeroclaw
watchdog: src/watchdog.c
	$(CC) $(CFLAGS) -o watchdog src/watchdog.c

clean:
	rm -f $(TARGET) test_subzeroclaw watchdog

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin/

.PHONY: clean install test watchdog
