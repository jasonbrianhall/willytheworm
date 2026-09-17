g++ -std=c++17 -O2 willy_main.cpp wopr_willy.cpp wopr_render.cpp highscores.cpp icon.cpp \
    -I. $(sdl2-config --cflags) $(pkg-config --cflags freetype2) \
    -o willy $(sdl2-config --libs) $(pkg-config --libs freetype2) \
    miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c
