CXXFLAGS := -O3 -fPIC

all: test.o
	$(CXX) -O3 -std=c++11 -o test test.o -lice_core -lpthread -luv -lgccjit

clean:
	rm test *.o
