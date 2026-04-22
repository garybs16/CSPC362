CXX = g++
CXXFLAGS = -O3 -Wall -Wextra -std=c++20
LDFLAGS = -lgdi32

TARGET = engine.exe
TEST_TARGET = engine_tests.exe
OBJS = main.o board.o movegen.o gui.o ai.o
TEST_OBJS = test.o board.o movegen.o ai.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(TEST_OBJS)

main.o: main.cpp gui.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

board.o: board.cpp board.hpp
	$(CXX) $(CXXFLAGS) -c board.cpp

movegen.o: movegen.cpp movegen.hpp move.hpp
	$(CXX) $(CXXFLAGS) -c movegen.cpp

gui.o: gui.cpp gui.hpp ai.hpp board.hpp move.hpp
	$(CXX) $(CXXFLAGS) -c gui.cpp

ai.o: ai.cpp ai.hpp board.hpp movegen.hpp
	$(CXX) $(CXXFLAGS) -c ai.cpp

test.o: test.cpp ai.hpp board.hpp movegen.hpp movelist.hpp
	$(CXX) $(CXXFLAGS) -c test.cpp

test: $(TEST_TARGET)

.PHONY: all clean test

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET)
