CC   ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
INCLUDES = -Iapp
SRCS = app/ring_buffer.c app/dsp_fir.c
TEST = test/test_main.c
OUT = build/test_main

.PHONY: test clean

test: $(OUT)
	./$(OUT)

$(OUT): $(SRCS) $(TEST)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) $(TEST) -lm -o $(OUT)

clean:
	rm -rf build

