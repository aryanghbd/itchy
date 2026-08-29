CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2

TARGET := build/itch-lob
SOURCE := main.cpp
OBJECT := build/main.o
LDLIBS := -lz

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECT)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

HEADERS := itch_messages.hpp byte_reader.hpp message_reader.hpp itch_parser.hpp order_book.hpp

$(OBJECT): $(SOURCE) $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
 
run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
