# Determine the platform
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  CC := clang -arch x86_64
else
  CC := gcc
endif

MAKE_DIR := $(shell pwd)
#UPPER_DIR := $(shell dirname $(shell pwd))
#WORK_DIR := $(shell dirname $(shell pwd))
WORK_DIR := $(MAKE_DIR)
INCLUDE_DIR := $(WORK_DIR)
SRC_DIR := $(WORK_DIR)
#LIBS := -lrt -lpthread
LIBS :=

LIB_DIR := /usr/local/lib
LIB_INCLUDE_DIR := /usr/local/include

OUPUT_DIR := $(WORK_DIR)/bin

FFMPEGLIBS := -lavcodec -lavformat -lavutil -lswscale -lz -lm
SDL := `sdl2-config --libs --cflags`

LINK := -c
#ifeq ($(UNAME_S),Linux)
#  LINK += -std=gnu -O2 # -fPIC
#else
#  LINK += -O2
#endif

DEBUG := -v -ggdb -Wall

#Try other option for SRC and OBJ

#SRC := $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SRC_DIR)**/*.cpp)
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:%.c=%.o)
HEADER := $(wildcard $(INCLUDE_DIR)/*.h)
TARGET := f_interface

export OBJ
export HEADER
export INCLUDE_DIR
export WORK_DIR
export TARGET

#include something.mk here to increase the obj
#include Makefile.inc
#include Makefile.obj

INCLUDES = $(addprefix -I,$(LIB_INCLUDE_DIR)) $(addprefix -I,$(INCLUDE_DIR))
EXT_INCL = $(addprefix -L,$(LIB_DIR))

CXXFLAGS = $(DEBUG) $(INCLUDES) $(EXT_INCL) $(LIBS) $(FFMPEGLIBS) $(SDL)

MKDIR := mkdir
CP := cp
RM := rm
ECHO := echo

#output to object files
%.o: %.c $(HEADER)
	$(CC) $(LINK) $< -o $@  $(CXXFLAGS)

#output to Running binary
$(TARGET): $(OBJ) $(HEADER)
	$(CC) $(OBJ) -o $@ $(CXXFLAGS)
.PHONY: all clean

all: $(TARGET)
	@if [ ! -d $(OUPUT_DIR) ]; then \
		$(MKDIR) $(OUPUT_DIR); \
	fi;
	@$(CP) ./$(TARGET) $(OUPUT_DIR)/$(TARGET)

clean:
	$(RM) $(OBJ) $(TARGET) $(OUPUT_DIR)/$(TARGET)
	@$(ECHO) apps distclean!!!
