CC       ?= gcc
CFLAGS    = -std=c99 -O2 -Wall -Wextra -Wpedantic -g
CFLAGS   += -Iinclude
SRCS      = src/main.c src/lexer.c src/parser.c src/symtab.c \
            src/semantic.c src/codegen.c src/util.c src/dump.c
OBJDIR    = build
OBJS      = $(SRCS:src/%.c=$(OBJDIR)/%.o)
TARGET    = mycc
TESTDIR  ?= /tmp/opencode

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@mkdir -p $(TESTDIR)
	@for t in tests/*.c; do \
		expected=$$(sed -n 's/.*EXPECT:[[:space:]]*\(-\{0,1\}[0-9][0-9]*\).*/\1/p' "$$t" | head -n1); \
		if [ -z "$$expected" ]; then \
			echo "SKIP: $$t (no EXPECT marker)"; continue; \
		fi; \
		base=$$(basename "$$t" .c); \
		./$(TARGET) "$$t" -o "$(TESTDIR)/$$base.s" \
			|| { echo "FAIL: compile $$t"; exit 1; }; \
		$(CC) -no-pie -o "$(TESTDIR)/$$base" "$(TESTDIR)/$$base.s" \
			|| { echo "FAIL: assemble $$t"; exit 1; }; \
		"$(TESTDIR)/$$base"; actual=$$?; \
		if [ "$$actual" != "$$expected" ]; then \
			echo "FAIL: $$t expected exit $$expected, got $$actual"; exit 1; \
		fi; \
		echo "PASS: $$t (exit $$actual)"; \
	done
	@for t in tests/errors/*.c; do \
		[ -e "$$t" ] || continue; \
		expected=$$(sed -n 's|^// EXPECT-ERROR:[[:space:]]*||p' "$$t" | head -n1); \
		base=$$(basename "$$t" .c); \
		if ./$(TARGET) "$$t" -o "$(TESTDIR)/err_$$base.s" >"$(TESTDIR)/err.out" 2>&1; then \
			echo "FAIL: $$t was accepted but should be rejected"; exit 1; \
		fi; \
		if [ -n "$$expected" ] && ! grep -qF "$$expected" "$(TESTDIR)/err.out"; then \
			echo "FAIL: $$t missing diagnostic: $$expected"; exit 1; \
		fi; \
		echo "PASS: $$t (rejected)"; \
	done
	@echo "All tests passed"

clean:
	rm -f $(TARGET) $(OBJS)
	-rmdir $(OBJDIR) 2>/dev/null || true
