LIBS_DIR = -L/usr/local/lib
LIBS = -lavcodec -lavformat -lavutil -lswscale -lz -lm
INCLUDE = -I/usr/local/include
LINK = -stdlib=libstdc++
DEBUG = -ggdb
CXXFLAGS = $(DEBUG) $(LIBS_DIR) $(LIBS) $(INCLUDE) $(LINK)
SDL = `sdl2-config --libs --cflags`
tutor1:
		gcc -v tutorial01.c -o $@ $(CXXFLAGS)
tutor2:
		gcc -v tutorial02.c -o $@ $(CXXFLAGS) $(SDL)
tutor3:
		gcc -v tutorial03.c -o $@ $(CXXFLAGS) $(SDL)
tutor4:
		gcc -v tutorial04.c -o $@ $(CXXFLAGS) $(SDL)

ffmpeg0:
	g++ -v FFMpegManager.cpp -o $@ $(CXXFLAGS)

scale:
	gcc -v scaling_video.c -o $@ $(CXXFLAGS)

clean:
	rm -rf ffmpeg0 tutor1 tutor2 tutor3 tutor4
