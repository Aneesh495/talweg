# Talweg: High-Assurance Flash Translation Layer (FTL) & Storage Engine
# Star Language: ATS2 (Postiats 0.4.2) with C17 runtime glue

CC ?= clang
PATSOPT ?= patsopt
PATSCC ?= patscc

PATSHOME ?= /opt/homebrew/lib/ats2-postiats-0.4.2

CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-label -std=gnu17
CFLAGS += -Iinclude -Isrc/c -I$(PATSHOME) -I$(PATSHOME)/ccomp/runtime

PATSFLAGS ?= -IATS src/ats/SATS -IATS src/ats/DATS -IATS src/ats/HATS

BIN_DIR = bin
BUILD_DIR = build
SRC_ATS_SATS = $(wildcard src/ats/SATS/*.sats)
SRC_ATS_DATS = $(wildcard src/ats/DATS/*.dats)
SRC_C = $(wildcard src/c/*.c)
SRC_CLI = $(wildcard src/cli/*.c)

OBJ_ATS_C = $(patsubst src/ats/DATS/%.dats, $(BUILD_DIR)/%_dats.c, $(SRC_ATS_DATS))
OBJ_ATS = $(patsubst src/ats/DATS/%.dats, $(BUILD_DIR)/%_dats.o, $(SRC_ATS_DATS))
OBJ_C = $(patsubst src/c/%.c, $(BUILD_DIR)/%.o, $(SRC_C))
OBJ_CLI = $(patsubst src/cli/%.c, $(BUILD_DIR)/%.o, $(SRC_CLI))

TARGET_LIB = $(BUILD_DIR)/libtalweg.a
TARGET_CLI = $(BIN_DIR)/talweg

TEST_SRCS = $(wildcard tests/*.c)
TEST_BINS = $(patsubst tests/%.c, $(BIN_DIR)/%, $(TEST_SRCS))

.PHONY: all clean test check-em-dash dirs

all: dirs $(TARGET_LIB) $(TARGET_CLI)

dirs:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)

# Generate C from ATS dynamic files
$(BUILD_DIR)/%_dats.c: src/ats/DATS/%.dats $(SRC_ATS_SATS) | dirs
	PATSHOME=$(PATSHOME) $(PATSOPT) $(PATSFLAGS) -o $@ -d $<

$(BUILD_DIR)/%_dats.o: $(BUILD_DIR)/%_dats.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/c/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/cli/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_LIB): $(OBJ_C) $(OBJ_ATS)
	ar rcs $@ $^

$(TARGET_CLI): $(OBJ_CLI) $(TARGET_LIB) | dirs
	$(CC) $(CFLAGS) $(OBJ_CLI) $(TARGET_LIB) -o $@ -lpthread

test: dirs $(TARGET_LIB) $(TEST_BINS)
	@echo "=== Running C Unit Tests ==="
	@for t in $(TEST_BINS); do \
		echo "Running $$t..."; \
		$$t || exit 1; \
	done
	@echo "=== Running Python Integration Tests ==="
	@python3 -m unittest discover -s tests -p "test_*.py" -v
	@echo "=== Verifying No Em-Dash Typography Invariant ==="
	@python3 tools/lint_no_em_dash.py

$(BIN_DIR)/test_%: tests/test_%.c $(TARGET_LIB) | dirs
	$(CC) $(CFLAGS) $< $(TARGET_LIB) -o $@ -lpthread

check-em-dash:
	@python3 tools/lint_no_em_dash.py

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR) *.flash *.journal *.sock
