CC      ?= gcc
CFLAGS   = -std=c99 -O2 -Wall -Wextra -Wpedantic -g
CFLAGS  += -Iinclude
SRCS     = src/main.c src/lexer.c src/parser.c src/symtab.c \
           src/semantic.c src/codegen.c src/util.c src/dump.c
OBJS     = $(SRCS:.c=.o)
TARGET   = mycc
BUILDDIR = /tmp/opencode

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@mkdir -p $(BUILDDIR)
	@for t in tests/*.c; do \
		expected=$$(sed -n 's/.*EXPECT:[[:space:]]*\(-\{0,1\}[0-9][0-9]*\).*/\1/p' "$$t" | head -n1); \
		if [ -z "$$expected" ]; then \
			echo "SKIP: $$t (no EXPECT marker)"; continue; \
		fi; \
		base=$$(basename "$$t" .c); \
		./$(TARGET) "$$t" -o "$(BUILDDIR)/$$base.s" \
			|| { echo "FAIL: compile $$t"; exit 1; }; \
		$(CC) -no-pie -o "$(BUILDDIR)/$$base" "$(BUILDDIR)/$$base.s" \
			|| { echo "FAIL: assemble $$t"; exit 1; }; \
		"$(BUILDDIR)/$$base"; actual=$$?; \
		if [ "$$actual" != "$$expected" ]; then \
			echo "FAIL: $$t expected exit $$expected, got $$actual"; exit 1; \
		fi; \
		echo "PASS: $$t (exit $$actual)"; \
	done
	@echo "All tests passed"

clean:
	rm -f $(TARGET) $(OBJS)
