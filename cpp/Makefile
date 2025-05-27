CXX = g++
CXXFLAGS = -s -std=c++17 -Wall -Wextra `pkg-config --cflags --libs gtkmm-3.0 sdl2 SDL2_mixer`
LIBS = `pkg-config --libs gtkmm-3.0`

willy: willy.cpp
	$(CXX) $(CXXFLAGS) -o willy willy.cpp loadlevels.cpp sound.cpp highscores.cpp $(LIBS)

clean:
	rm -f willy

.PHONY: clean

