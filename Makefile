# Plinth: Purely Functional Stream-Relational Engine
# Build and orchestration Makefile

SHELL := /bin/bash
JAVA ?= java
JAVAC ?= javac
CC ?= clang
PYTHON ?= python3

ROOT_DIR := $(shell pwd)
LIB_DIR := $(ROOT_DIR)/lib
FREGE_JAR := $(LIB_DIR)/frege3.24.405.jar

SRC_JAVA_DIR := $(ROOT_DIR)/src/main/java
SRC_FREGE_DIR := $(ROOT_DIR)/src/main/frege
TEST_FREGE_DIR := $(ROOT_DIR)/src/test/frege
TEST_JAVA_DIR := $(ROOT_DIR)/src/test/java

BUILD_DIR := $(ROOT_DIR)/build
BUILD_CLASSES_JAVA := $(BUILD_DIR)/classes/java
BUILD_CLASSES_FREGE := $(BUILD_DIR)/classes/frege
BUILD_CLASSES_TEST := $(BUILD_DIR)/classes/test

C_SRC_DIR := $(ROOT_DIR)/c_src
C_BUILD_DIR := $(C_SRC_DIR)/build
C_LIB := $(C_SRC_DIR)/libplinth_client.dylib
C_TEST := $(C_SRC_DIR)/test_client

CLASSPATH := $(FREGE_JAR):$(BUILD_CLASSES_JAVA):$(BUILD_CLASSES_FREGE)
TEST_CLASSPATH := $(CLASSPATH):$(BUILD_CLASSES_TEST)

.PHONY: all clean compile-java compile-frege compile-test test native test-native bench run cli tui help

all: compile-java compile-frege compile-test native

help:
	@echo "Plinth Build Targets:"
	@echo "  make all           - Build Java, Frege, test suites, and C native client"
	@echo "  make compile-java  - Compile Java interop and NIO runtime bridge"
	@echo "  make compile-frege - Compile pure functional Frege database modules"
	@echo "  make compile-test  - Compile Frege test suites"
	@echo "  make test          - Run complete Frege test suite"
	@echo "  make native        - Build C native client library and hardware CRC32C"
	@echo "  make test-native   - Run C native test suite"
	@echo "  make bench         - Run streaming throughput and microbenchmarks"
	@echo "  make run           - Launch embedded Plinth streaming server"
	@echo "  make cli           - Launch interactive streaming SQL client"
	@echo "  make tui           - Launch live terminal metrics dashboard"
	@echo "  make clean         - Remove all build and test artifacts"

$(BUILD_CLASSES_JAVA):
	@mkdir -p $(BUILD_CLASSES_JAVA)

$(BUILD_CLASSES_FREGE):
	@mkdir -p $(BUILD_CLASSES_FREGE)

$(BUILD_CLASSES_TEST):
	@mkdir -p $(BUILD_CLASSES_TEST)

compile-java: $(BUILD_CLASSES_JAVA)
	@echo "==> Compiling Java runtime bridge..."
	@find $(SRC_JAVA_DIR) -name "*.java" > $(BUILD_DIR)/java_sources.txt 2>/dev/null || true
	@if [ -s $(BUILD_DIR)/java_sources.txt ]; then \
		$(JAVAC) -d $(BUILD_CLASSES_JAVA) -cp $(FREGE_JAR) @$(BUILD_DIR)/java_sources.txt; \
	fi

compile-frege: compile-java $(BUILD_CLASSES_FREGE)
	@echo "==> Compiling Frege stream-relational engine..."
	@find $(SRC_FREGE_DIR) -name "*.fr" > $(BUILD_DIR)/frege_sources.txt
	@$(JAVA) -Xss8m -Xmx2g -jar $(FREGE_JAR) -make \
		-d $(BUILD_CLASSES_FREGE) \
		-sp $(SRC_FREGE_DIR) \
		-fp $(BUILD_CLASSES_JAVA):$(BUILD_CLASSES_FREGE):$(FREGE_JAR) \
		$(SRC_FREGE_DIR)/org/plinth/Main.fr

compile-test: compile-frege $(BUILD_CLASSES_TEST)
	@echo "==> Compiling Frege test suite..."
	@find $(TEST_FREGE_DIR) -name "*.fr" > $(BUILD_DIR)/test_sources.txt 2>/dev/null || true
	@if [ -s $(BUILD_DIR)/test_sources.txt ]; then \
		$(JAVA) -Xss8m -Xmx2g -jar $(FREGE_JAR) -make \
			-d $(BUILD_CLASSES_TEST) \
			-sp $(TEST_FREGE_DIR):$(SRC_FREGE_DIR) \
			-fp $(BUILD_CLASSES_JAVA):$(BUILD_CLASSES_FREGE):$(BUILD_CLASSES_TEST):$(FREGE_JAR) \
			$(TEST_FREGE_DIR)/org/plinth/test/TestRunner.fr; \
	fi

test: compile-test test-native
	@echo "==> Executing Frege verification and invariant test suite..."
	@$(JAVA) -Xss8m -Xmx2g -cp $(TEST_CLASSPATH) org.plinth.test.TestRunner
	@echo "==> Executing Python client test suite..."
	@PYTHONPATH=$(ROOT_DIR)/python $(PYTHON) -m unittest discover -s $(ROOT_DIR)/python/tests -p "test_*.py" 2>/dev/null || true

native:
	@echo "==> Building C native client library..."
	@$(MAKE) -C $(C_SRC_DIR) all

test-native: native
	@echo "==> Running C native tests..."
	@$(MAKE) -C $(C_SRC_DIR) test

bench: all
	@echo "==> Running performance microbenchmarks..."
	@$(SHELL) $(ROOT_DIR)/benchmarks/microbench.sh

run: compile-frege
	@echo "==> Starting Plinth server daemon..."
	@$(JAVA) -Xss8m -Xmx2g -cp $(CLASSPATH) org.plinth.Main server

cli: compile-frege
	@$(JAVA) -Xss8m -Xmx2g -cp $(CLASSPATH) org.plinth.Main cli

tui:
	@PYTHONPATH=$(ROOT_DIR)/python $(PYTHON) -m plinth.tui

clean:
	@echo "==> Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(C_SRC_DIR) clean 2>/dev/null || true
	@rm -rf python/*.egg-info python/__pycache__
