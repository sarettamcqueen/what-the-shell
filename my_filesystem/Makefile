CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -Iinclude
CFLAGS += -Wno-stringop-truncation
ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG_MODE -g -O0 -fsanitize=address
    LDFLAGS += -fsanitize=address 
    $(info === DEBUG MODE compilation (with ASAN) ===)
else
    CFLAGS += -O2
endif

SRCDIR = src
TESTDIR = tests
BUILDDIR = build

INC_FLAGS = -Iinclude -I$(SRCDIR)/disk -I$(SRCDIR)/utils -I$(SRCDIR)/filesystem -I$(SRCDIR)/vfs -I$(SRCDIR)/shell
CFLAGS += $(INC_FLAGS)
SRCS := $(shell find $(SRCDIR) -name '*.c')
OBJS := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))

MAIN_BIN = $(BUILDDIR)/my_filesystem
all: $(MAIN_BIN)

run: all
	./$(MAIN_BIN)

# === LINKING RULE ===
$(MAIN_BIN): $(OBJS)
	@echo "Linking executable..."
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# === COMPILATION RULE ===
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)  # Crea la sottocartella in build/ se non esiste
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# === TEST SUITE ===
test: all
	@echo "======================================"
	@echo "     Running Bash Test Suite          "
	@echo "======================================"
	@for script in $(TESTDIR)/*.sh; do \
		if [ -f "$$script" ]; then \
			echo ">>> Running $$script..."; \
			chmod +x "$$script"; \
			./$$script; \
			echo ""; \
		fi \
	done
	@echo "=== All tests finished ==="

# === CLEANUP ===
clean:
	@rm -rf $(BUILDDIR) *.img *.log cmds_*.txt
	@echo "Cleaned!"

help:
	@echo "Available targets: all, run, test, clean, help"

.PHONY: all test run clean help