CC       ?= gcc
CFLAGS    = -std=c99 -O2 -Wall -Wextra -Wpedantic -g
CFLAGS   += -Iinclude
SRCS      = src/main.c src/lexer.c src/parser.c src/myc.c src/symtab.c \
            src/semantic.c src/opt.c src/codegen.c src/util.c src/dump.c
OBJDIR    = build
OBJS      = $(SRCS:src/%.c=$(OBJDIR)/%.o)
TARGET    = mycc
TESTDIR  ?= /tmp/mycc-test

# --- Platform detection ----------------------------------------------------
# The generated assembly is position-independent (every symbol access is
# %rip-relative), but the *link* step still has to match the host:
#
#   Linux   gcc defaults to PIE; -no-pie keeps the classic layout.
#   macOS   Apple's linker rejects -no-pie outright, and Mach-O wants a
#           leading '_' on external symbols, so the compiler is asked for
#           --target=darwin instead (BUG-018). The backend emits x86-64, so
#           the driver is pinned to that architecture: on an Apple Silicon
#           host (every current GitHub macOS runner) clang would otherwise
#           read the .s as arm64 and fail with "unrecognized instruction
#           mnemonic" on the first `movq` (BUG-031).
#   Windows MinGW defaults to non-PIE and needs no flag.
UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(UNAME_S),Darwin)
  LDFLAGS    := -arch x86_64
  TARGETFLAG := --target=darwin
else ifneq (,$(findstring MINGW,$(UNAME_S)))
  LDFLAGS    :=
  TARGETFLAG := --target=windows
else
  LDFLAGS    := -no-pie
  TARGETFLAG :=
endif

.PHONY: all clean test difftest

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@mkdir -p $(TESTDIR)
	@command -v cmp >/dev/null 2>&1 || { \
		echo "FAIL: 'cmp' is required by the optimized/--no-opt check"; \
		echo "      install diffutils (MSYS2: pacman -S diffutils)"; exit 1; \
	}
	@for t in tests/*.c; do \
		expected=$$(sed -n 's/.*EXPECT:[[:space:]]*\(-\{0,1\}[0-9][0-9]*\).*/\1/p' "$$t" | head -n1); \
		if [ -z "$$expected" ]; then \
			echo "SKIP: $$t (no EXPECT marker)"; continue; \
		fi; \
		warn=$$(sed -n 's/.*WARN:[[:space:]]*//p' "$$t" | tr -d '\r' | head -n1); \
		wantout=$$(sed -n 's/.*EXPECT-OUT:[[:space:]]*//p' "$$t" | tr -d '\r' | head -n1); \
		base=$$(basename "$$t" .c); \
		./$(TARGET) $(TARGETFLAG) "$$t" -o "$(TESTDIR)/$$base.s" \
			>"$(TESTDIR)/$$base.out" 2>&1 \
			|| { echo "FAIL: compile $$t"; cat "$(TESTDIR)/$$base.out"; exit 1; }; \
		if [ -n "$$warn" ] && ! grep -qF "$$warn" "$(TESTDIR)/$$base.out"; then \
			echo "FAIL: $$t missing warning: $$warn"; exit 1; \
		fi; \
		$(CC) $(LDFLAGS) -o "$(TESTDIR)/$$base" "$(TESTDIR)/$$base.s" \
			|| { echo "FAIL: assemble $$t"; exit 1; }; \
		"$(TESTDIR)/$$base" > "$(TESTDIR)/$$base.stdout" 2>&1; actual=$$?; \
		if [ "$$actual" != "$$expected" ]; then \
			echo "FAIL: $$t expected exit $$expected, got $$actual"; exit 1; \
		fi; \
		if [ -n "$$wantout" ] && ! grep -qF "$$wantout" "$(TESTDIR)/$$base.stdout"; then \
			echo "FAIL: $$t missing output: $$wantout"; exit 1; \
		fi; \
		echo "PASS: $$t (exit $$actual)"; \
	done
	@for t in tests/*.c; do \
		base=$$(basename "$$t" .c); \
		[ -x "$(TESTDIR)/$$base" ] || continue; \
		./$(TARGET) $(TARGETFLAG) --no-opt "$$t" -o "$(TESTDIR)/$$base.noopt.s" \
			|| { echo "FAIL: $$t --no-opt compile failed"; exit 1; }; \
		$(CC) $(LDFLAGS) -o "$(TESTDIR)/$$base.noopt" "$(TESTDIR)/$$base.noopt.s" \
			|| { echo "FAIL: $$t --no-opt assemble failed"; exit 1; }; \
		"$(TESTDIR)/$$base" > "$(TESTDIR)/$$base.opt.stdout" 2>&1; opt=$$?; \
		"$(TESTDIR)/$$base.noopt" > "$(TESTDIR)/$$base.noopt.stdout" 2>&1; noopt=$$?; \
		if [ "$$opt" != "$$noopt" ]; then \
			echo "FAIL: $$t optimized exits $$opt, --no-opt exits $$noopt"; exit 1; \
		fi; \
		if ! cmp -s "$(TESTDIR)/$$base.opt.stdout" "$(TESTDIR)/$$base.noopt.stdout"; then \
			echo "FAIL: $$t optimized and --no-opt print different output"; exit 1; \
		fi; \
	done
	@echo "PASS: optimized and --no-opt agree on every test"
	@for t in tests/errors/*.c; do \
		[ -e "$$t" ] || continue; \
		expected=$$(sed -n 's|^// EXPECT-ERROR:[[:space:]]*||p' "$$t" | tr -d '\r' | head -n1); \
		base=$$(basename "$$t" .c); \
		if ./$(TARGET) "$$t" -o "$(TESTDIR)/err_$$base.s" >"$(TESTDIR)/err.out" 2>&1; then \
			echo "FAIL: $$t was accepted but should be rejected"; exit 1; \
		fi; \
		if [ -n "$$expected" ] && ! grep -qF "$$expected" "$(TESTDIR)/err.out"; then \
			echo "FAIL: $$t missing diagnostic: $$expected"; exit 1; \
		fi; \
		echo "PASS: $$t (rejected)"; \
	done
	@rm -f "$(TESTDIR)/contract.s"; \
	if ./$(TARGET) tests/errors/undeclared.c -o "$(TESTDIR)/contract.s" \
			>/dev/null 2>&1; then :; fi; \
	if [ -e "$(TESTDIR)/contract.s" ]; then \
		echo "FAIL: a failed compile left $(TESTDIR)/contract.s behind"; exit 1; \
	fi; \
	echo "PASS: failed compile leaves no output file"
	@rm -f "$(TESTDIR)/silent.out"; \
	if ! ./$(TARGET) -w tests/25_fall_off_end.c -o "$(TESTDIR)/silent.s" \
			>"$(TESTDIR)/silent.out" 2>&1; then \
		echo "FAIL: -w turned a valid compile into a failure"; exit 1; \
	fi; \
	if grep -q "warning:" "$(TESTDIR)/silent.out"; then \
		echo "FAIL: -w did not silence the warning channel"; exit 1; \
	fi; \
	echo "PASS: -w silences warnings without failing the build"
	@./$(TARGET) --lang=myc demo/4_pair.myc -o "$(TESTDIR)/pair_myc.s" >/dev/null 2>&1 \
		|| { echo "FAIL: the myc front end rejected a valid program"; exit 1; }; \
	./$(TARGET) demo/4_pair.c -o "$(TESTDIR)/pair_c.s" >/dev/null 2>&1 \
		|| { echo "FAIL: the C twin of the toy program was rejected"; exit 1; }; \
	cmp -s "$(TESTDIR)/pair_myc.s" "$(TESTDIR)/pair_c.s" \
		|| { echo "FAIL: myc and the C twin emitted different assembly"; exit 1; }; \
	echo "PASS: --lang=myc matches the C twin (demo/4_pair)"
	@for base in else literals; do \
		myc=tests/myc/$$base.myc; c=tests/myc/$$base.c; \
		expected=$$(sed -n 's/.*EXPECT:[[:space:]]*\(-\{0,1\}[0-9][0-9]*\).*/\1/p' "$$myc" | head -n1); \
		wantout=$$(sed -n 's/.*EXPECT-OUT:[[:space:]]*//p' "$$myc" | tr -d '\r' | head -n1); \
		./$(TARGET) --lang=myc "$$myc" -o "$(TESTDIR)/myc_$$base.s" >/dev/null 2>&1 \
			|| { echo "FAIL: myc rejected $$myc"; exit 1; }; \
		./$(TARGET) "$$c" -o "$(TESTDIR)/c_$$base.s" >/dev/null 2>&1 \
			|| { echo "FAIL: the C twin $$c was rejected"; exit 1; }; \
		cmp -s "$(TESTDIR)/myc_$$base.s" "$(TESTDIR)/c_$$base.s" \
			|| { echo "FAIL: $$myc and its C twin emitted different assembly"; exit 1; }; \
		$(CC) $(LDFLAGS) -o "$(TESTDIR)/myc_$$base" "$(TESTDIR)/myc_$$base.s" \
			|| { echo "FAIL: the assembly for $$myc did not link"; exit 1; }; \
		"$(TESTDIR)/myc_$$base" > "$(TESTDIR)/myc_$$base.stdout" 2>&1; rc=$$?; \
		if [ "$$rc" != "$$expected" ]; then \
			echo "FAIL: $$myc returned $$rc instead of $$expected"; exit 1; \
		fi; \
		if [ -n "$$wantout" ] && ! grep -qF "$$wantout" "$(TESTDIR)/myc_$$base.stdout"; then \
			echo "FAIL: $$myc missing output: $$wantout"; exit 1; \
		fi; \
		echo "PASS: --lang=myc matches its C twin: $$myc"; \
	done
	@echo "All tests passed"

# Differential testing: compile tests/diff/*.c with mycc *and* with the host
# compiler, then require the same exit code. `make test` proves mycc gives a
# plausible answer; this proves it gives the same answer as a real compiler.
difftest: $(TARGET)
	@MYCC=./$(TARGET) CC="$(CC)" TARGETFLAG="$(TARGETFLAG)" LDFLAGS="$(LDFLAGS)" \
		sh difftest.sh

clean:
	rm -f $(TARGET) $(OBJS)
	-rmdir $(OBJDIR) 2>/dev/null || true
