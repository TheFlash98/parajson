CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SRC = main.cpp simdjson.cpp
OBJDIR = build
OBJ = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRC))
TARGET = $(OBJDIR)/quickstart

.PHONY: all build clean run dirs

all: $(TARGET)

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJDIR)