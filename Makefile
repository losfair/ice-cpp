CXXFLAGS := -O3 -fPIC --pie

all: test.o
	$(CXX) -O3 -std=c++11 --pie -o test test.o -lice_core -lpthread -luv -lgccjit
