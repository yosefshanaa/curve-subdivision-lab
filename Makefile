# Subdivision Lab 3D — no external dependencies.
# Everything below builds with a C++17 compiler and libdl; the X11 window is
# loaded at runtime via dlopen, so no -dev packages are required.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS  ?=
LDLIBS   ?= -ldl

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := subdivlab

.PHONY: all clean run test screenshots font help

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	@mkdir -p build

-include $(OBJ:.o=.d)

run: $(BIN)
	./$(BIN)

test: $(BIN)
	./$(BIN) --selftest

screenshots: $(BIN)
	@mkdir -p screenshots
	./$(BIN) --gallery screenshots

# Regenerating the baked font atlas needs Python + Pillow; the generated header
# is committed, so this is only for changing the typefaces.
font:
	python3 tools/genfont.py

clean:
	rm -rf build $(BIN)

help:
	@echo "make            build ./subdivlab"
	@echo "make run        build and open the interactive window"
	@echo "make test       run the numeric self test"
	@echo "make screenshots  regenerate screenshots/ from the app itself"
	@echo "make clean      remove build output"
