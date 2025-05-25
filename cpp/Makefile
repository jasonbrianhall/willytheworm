CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra `pkg-config --cflags gtkmm-3.0`
LIBS = `pkg-config --libs gtkmm-3.0`

willy: willy.cpp
	$(CXX) $(CXXFLAGS) -o willy willy.cpp $(LIBS)

clean:
	rm -f willy

.PHONY: clean
