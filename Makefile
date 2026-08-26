CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic

TARGET := build/itch-lob
SOURCE := main.cpp
OBJECT := build/main.o

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECT)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJECT): $(SOURCE)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
