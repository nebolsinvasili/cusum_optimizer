CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -fopenmp -fPIC
LDFLAGS = -fopenmp

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
CONFIG_DIR = config
LIB_DIR = lib
SHARED_LIB = $(LIB_DIR)/libcusum.so

TARGET = $(BIN_DIR)/cusum

SOURCES = \
    $(SRC_DIR)/Calculator.cpp \
    $(SRC_DIR)/Config.cpp \
    $(SRC_DIR)/main.cpp \
    $(SRC_DIR)/ProgressBar.cpp \
    $(SRC_DIR)/Simulator.cpp \
    $(SRC_DIR)/Utils.cpp \
    $(SRC_DIR)/bridge.cpp

OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.hpp)

JSON_INCLUDE = /usr/include
CXXFLAGS += -I$(INCLUDE_DIR) -I$(JSON_INCLUDE)

.PHONY: all clean clean-results clean-logs clean-csv clean-all distclean help run run-config run-quick run-detailed debug validate test lib

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "[LINK] Linking target: $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[OK] Build complete: $(TARGET)"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS) | $(BUILD_DIR)
	@echo "[COMPILE] Compiling: $<"
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(SHARED_LIB): $(OBJECTS)
	@mkdir -p $(LIB_DIR)
	@echo "[LINK] Building shared library: $@"
	$(CXX) -shared -fPIC -o $@ $^ $(LDFLAGS)
	@echo "[OK] Shared library: $(SHARED_LIB)"

lib: $(SHARED_LIB)

# ============================================================================
# RUN TARGETS
# ============================================================================

run: $(TARGET)
	@echo "[RUN] Running CUSUM..."
	./$(TARGET)

run-config: $(TARGET)
	@echo "[RUN] Running with config file..."
	./$(TARGET) --config $(CONFIG_DIR)/default_config.json

run-quick: $(TARGET)
	@echo "[RUN] Quick run (reduced simulations)..."
	./$(TARGET) --simulations 1000 --k_start 5.0 6.0 0.5 --H_start 4.0 5.0 0.5

run-detailed: $(TARGET)
	@echo "[RUN] Detailed run..."
	./$(TARGET) --simulations 10000 --k_start 5.0 6.0 0.1 --H_start 4.0 5.0 0.1

# ============================================================================
# DEBUG BUILD
# ============================================================================

debug: CXXFLAGS += -g -O0 -DDEBUG
debug: clean $(TARGET)
	@echo "[DEBUG] Debug build completed"

# ============================================================================
# VALIDATION (тесты по книге Qiu, гл. 4.2.2)
# ============================================================================

TEST_DIR = tests
VALIDATE_BIN = $(TEST_DIR)/validate

$(VALIDATE_BIN): tests/validate.cpp $(BUILD_DIR)/Simulator.o $(BUILD_DIR)/Utils.o | $(TEST_DIR)
	@echo "[TEST] Building validation binary..."
	$(CXX) -std=c++17 -Wall -Wextra -O3 -fopenmp -I$(INCLUDE_DIR) -o $@ \
		tests/validate.cpp $(BUILD_DIR)/Simulator.o $(BUILD_DIR)/Utils.o $(LDFLAGS)
	@echo "[OK] Test binary: $(VALIDATE_BIN)"

$(TEST_DIR):
	@mkdir -p $(TEST_DIR)

validate: $(VALIDATE_BIN)
	@echo "[TEST] Running validation against Qiu (ch. 4.2.2)..."
	./$(VALIDATE_BIN) --simulations 10000

test: validate

# ============================================================================
# CLEAN TARGETS
# ============================================================================

clean:
	@echo "[CLEAN] Removing object files and executable..."
	@rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d $(TARGET)
	@rm -f $(BIN_DIR)/cusum_sn $(VALIDATE_BIN)
	@rm -rf $(LIB_DIR)
	@echo "[OK] Build clean complete"

clean-results:
	@echo "[CLEAN] Removing all result files (logs, CSV, checkpoints, plots)..."
	@rm -f *.log *.csv *.txt *.png *.pdf
	@echo "[OK] Results cleaned"

clean-logs:
	@echo "[CLEAN] Removing log files..."
	@rm -f *.log *.txt
	@echo "[OK] Logs cleaned"

clean-csv:
	@echo "[CLEAN] Removing CSV data files..."
	@rm -f *.csv
	@echo "[OK] CSV files cleaned"

clean-checkpoints:
	@echo "[CLEAN] Removing checkpoint files..."
	@rm -f arl_checkpoint.txt
	@echo "[OK] Checkpoints cleaned"

clean-all: clean-results distclean
	@echo "[CLEAN] Full clean complete"

distclean: clean
	@echo "[CLEAN] Removing build and bin directories..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "[OK] Distclean complete"

# ============================================================================
# HELP TARGET
# ============================================================================

help:
	@echo "================================================================================"
	@echo "         CUSUM-SN ARL Calculator - Makefile Help"
	@echo "================================================================================"
	@echo ""
	@echo "Build targets:"
	@echo "  make              - Build the main executable"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make lib          - Build shared library for Python bindings"
	@echo ""
	@echo "Run targets:"
	@echo "  make run          - Build and run the program"
	@echo "  make run-config   - Run with default configuration"
	@echo "  make run-quick    - Quick run with minimal parameters"
	@echo "  make run-detailed - Detailed run with more simulations"
	@echo ""
	@echo "Clean targets:"
	@echo "  make clean        - Remove object files and executable"
	@echo "  make clean-results - Remove all generated results (logs, CSV, checkpoints, plots)"
	@echo "  make clean-logs   - Remove only log files (*.log, *.txt)"
	@echo "  make clean-csv    - Remove only CSV data files (*.csv)"
	@echo "  make clean-checkpoints - Remove checkpoint file only"
	@echo "  make clean-all    - Remove everything (results + build)"
	@echo "  make distclean    - Remove build and bin directories"
	@echo ""
	@echo "Other:"
	@echo "  make validate    - Build and run book-based validation (Qiu ch. 4.2.2)"
	@echo "  make test        - Alias for make validate"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "================================================================================"

.DEFAULT_GOAL := all

DEPFLAGS = -MMD -MP
CXXFLAGS += $(DEPFLAGS)
DEPENDENCIES = $(OBJECTS:.o=.d)
-include $(DEPENDENCIES)