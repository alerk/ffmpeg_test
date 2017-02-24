LIBS_DIR = -L/usr/local/lib
LIBS = -lavcodec -lavformat -lavutil -lswscale
INCLUDE = -I/usr/local/include
LINK = -stdlib=libstdc++
CXXFLAGS = $(LIBS_DIR) $(LIBS) $(INCLUDE) $(LINK)
SDL = `sdl2-config --libs --cflags`
tutor1:
		gcc -v tutorial01.c -o $@ $(CXXFLAGS)
tutor2:
		gcc -v tutorial02.c -o $@ $(CXXFLAGS) $(SDL)
ffmpeg0:
	g++ -v FFMpegManager.cpp -o $@ $(CXXFLAGS)

scale:
	gcc -v scaling_video.c -o $@ $(CXXFLAGS)

clean:
	rm -rf ffmpeg0 tutor1 tutor2
