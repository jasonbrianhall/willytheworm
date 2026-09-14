g++ -std=c++17 -O2 willy_main.cpp wopr_willy.cpp wopr_render.cpp highscores.cpp     -I. $(sdl2-config --cflags) -o willy $(sdl2-config --libs) miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c
