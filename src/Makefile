# Determine the platform
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  CC := clang -arch x86_64
else
  CC := gcc
endif

LIBS_DIR = -L/usr/local/lib
LIBS = -lavcodec -lavformat -lavutil -lavfilter -lswscale -lswresample -lz -lm
INCLUDE = -I/usr/local/include -I../include
LINK = -stdlib=libstdc++
DEBUG = -ggdb
CXXFLAGS = $(DEBUG) $(LIBS_DIR) $(LIBS) $(INCLUDE) $(LINK)
SDL = `sdl2-config --libs --cflags`
DEPS = ../include/FFMpegInterface.h ../include/FFMpegRaw.h

SRC = main.cpp FFMpegRaw.cpp
OBJ = $(SRC:%.cpp = %.o)

tutor1:
	$(CC) -v tutorial01.c -o $@ $(CXXFLAGS)
tutor2:
	$(CC) -v tutorial02.c -o $@ $(CXXFLAGS) $(SDL)
tutor3:
	rm -rf $@
	$(CC) -v tutorial03.c -o $@ $(CXXFLAGS) $(SDL)
tutor4:
	rm -rf $@
	$(CC) -v tutorial04.c -o $@ $(CXXFLAGS) $(SDL)
tutor5:
	rm -rf $@
	$(CC) -v tutorial05.c -o $@ $(CXXFLAGS) $(SDL)

tutor6:
	rm -rf $@
	$(CC) -v tutorial06.c -o $@ $(CXXFLAGS) $(SDL)

tutor7:
	rm -rf $@
	$(CC) -v tutorial07.c -o $@ $(CXXFLAGS) $(SDL)

concat:
	$(CC) -v concat.c -o $@ $(CXXFLAGS) $(SDL)

nconcat: ffmpeg_function.h
	$(CC) -v concat.c ffmpeg_function.c -o $@ $(CXXFLAGS)

encoder.o: encoder.h
	$(CC) -v encoder.c -c -o $@ $(CXXFLAGS) $(SDL)

muxing:
	$(CC) -v muxing.c -o $@ $(CXXFLAGS) $(SDL)

transcode:
	$(CC) -v transcoding.c -o $@ $(CXXFLAGS)

ffmpeg0:
	g++ -v FFMpegManager.cpp -o $@ $(CXXFLAGS)

%.o: %.cpp $(DEPS)
	g++ -v -c $< -o $@ $(CXXFLAGS)
ffmpegex: $(OBJ)
	g++ -v $< -o $@ $(CXXFLAGS)

scale:
	$(CC) -v scaling_video.c -o $@ $(CXXFLAGS)
ccvideo:
		$(CC) -v main.c -o $@ $(CXXFLAGS)
		$(CC) -v main-1.c -o $@-1 $(CXXFLAGS)
clean:
	rm -rf ffmpeg0 tutor1 tutor2 tutor3 tutor4 tutor5 tutor6 tutor7 muxing transcode concat ccvideo ccvideo-1 nconcat
