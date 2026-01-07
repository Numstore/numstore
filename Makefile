.PHONY: all debug debug-ntests release release-tests test coverage clean format docs docs-clean run-tests valgrind-tests

CMAKE = /usr/bin/cmake

all: debug 
comprehensive: debug release debug-ntests release-tests

### BUILD MODES
nspprint: debug

### BUILD TARGETS

debug: ./build/debug
	$(CMAKE) -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_NTEST=OFF
	$(CMAKE) --build build/debug -- -j$(shell nproc 2>/dev/null || echo 1)
	cp ./build/debug/compile_commands.json .

debug-ntests: ./build/debug-ntests
	$(CMAKE) -S . -B build/debug-ntests -DCMAKE_BUILD_TYPE=Debug -DENABLE_NTEST=ON
	$(CMAKE) --build build/debug-ntests -- -j$(shell nproc 2>/dev/null || echo 1)

release: ./build/release
	$(CMAKE) -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DENABLE_NTEST=ON -DENABLE_NDEBUG=ON -DENABLE_NLOG=ON
	$(CMAKE) --build build/release -- -j$(shell nproc 2>/dev/null || echo 1)

release-tests: ./build/release-tests
	$(CMAKE) -S . -B build/release-tests -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_NTEST=OFF -DENABLE_NDEBUG=ON -DENABLE_NLOG=OFF
	$(CMAKE) --build build/release-tests -- -j$(shell nproc 2>/dev/null || echo 1)

./build/debug:
	mkdir -p build/debug

./build/debug-ntests:
	mkdir -p build/debug-ntests

./build/release:
	mkdir -p build/release

./build/release-tests:
	mkdir -p build/release-tests

### TESTS

# Build mode for tests (default: debug, can be overridden with BUILD_MODE=release)
BUILD_MODE ?= debug

# Helper to get build directory based on mode
ifeq ($(BUILD_MODE),release)
	BUILD_DIR = build/release-tests
	TEST_TARGET = release-tests
else
	BUILD_DIR = build/debug
	TEST_TARGET = debug
endif

test: $(TEST_TARGET)

run-tests: test
	cd $(BUILD_DIR)/apps && ./test -- \
		../libs/nstypes/libnstypes.so \
		../libs/nspager/libnspager.so \
		../libs/nsrptcursor/libnsrptcursor.so \
		../libs/nsusecase/libnsusecase.so

nstypes-tests: test
	cd $(BUILD_DIR)/apps && ./test -- \
		../libs/nstypes/libnstypes.so

nspager-tests: test
	cd $(BUILD_DIR)/apps && ./test -- \
		../libs/nspager/libnspager.so

numstore-tests: test
	cd $(BUILD_DIR)/apps && ./test -- \
		../libs/numstore/libnumstore.so

nsusecase-tests: test
	cd $(BUILD_DIR)/apps && ./test -- \
		../libs/nsusecase/libnsusecase.so

valgrind-tests: test
	cd $(BUILD_DIR)/apps && valgrind --tool=memcheck --leak-check=full \
		--show-leak-kinds=all --track-origins=yes \
		--read-var-info=yes \
		--undef-value-errors=yes --error-exitcode=1 \
		./test 

clean:
	rm -f *.db *.wal 
	rm -rf build 

format:
	clang-format -i $(shell \
	    find libs apps \( -name '*.c' -o -name '*.h' \) \
	         ! \( -name "lempar.c" -o -name "lemon.c" -o -name "linenoise.c" -o -name "linenoise.h" \) )
