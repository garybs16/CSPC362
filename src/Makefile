CXX = g++
CXXFLAGS = -O3 -Wall -std=c++20

TARGET = engine
OBJS = main.o board.o movegen.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp board.hpp movegen.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

board.o: board.cpp board.hpp
	$(CXX) $(CXXFLAGS) -c board.cpp

movegen.o: movegen.cpp movegen.hpp move.hpp
	$(CXX) $(CXXFLAGS) -c movegen.cpp

clean:
	rm -f $(OBJS) $(TARGET)