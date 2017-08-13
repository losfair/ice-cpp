CXXFLAGS := -std=c++14 -O3 -fPIC -Wall

all: test.o
	$(CXX) -O3 -o test test.o -lice_core -lpthread -luv -lgccjit

clean:
	rm test *.o

test_module:
	clang -emit-llvm -o test_module_2.bc -c test_module_2.c
