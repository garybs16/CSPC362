CXX = g++
CXXFLAGS = -O3 -Wall -std=c++17

TARGET = engine
OBJS = main.o board.o

all: $(TARGET)

$(TARGET): $(OBJS)
		$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp board.hpp
		$(CXX) $(CXXFLAGS) -c main.cpp
board.o: board.cpp board.hpp
		$(CXX) $(CXXFLAGS) -c board.cpp

clean:
	rm -f $(OBJS) $(TARGET)