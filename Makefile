CXXFLAGS := -std=c++11 -O3 -fPIC -Wall

all: test.o
	$(CXX) -O3 -o test test.o -lice_core -lpthread -luv -lgccjit

clean:
	rm test *.o
