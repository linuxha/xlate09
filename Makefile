CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic -g
TARGET  = xlate09
SRC     = xlate09.c
INFILE  = testfile.asc
OUTFILE = testfile-68k.asm
REFFILE = output.asc

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TARGET)
	./$(TARGET) $(INFILE) $(OUTFILE) I
	@echo "--- Diffing against reference $(REFFILE) ---"
	@tr -d '\r' < $(REFFILE) > /tmp/xlate_ref.asm
	@tr -d '\r' < $(OUTFILE) > /tmp/xlate_out.asm
	@if diff -u /tmp/xlate_ref.asm /tmp/xlate_out.asm; then \
		echo "PASS"; \
	else \
		echo "FAIL (see diff above)"; \
		exit 1; \
	fi

clean:
	rm -f $(TARGET) $(OUTFILE) error.txt
