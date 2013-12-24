CC = cc
CFLAGS = -std=c99 -Wall -Werror -Wno-unused -g
LFLAGS = -lm
FILES = chapter2 chapter4 chapter4a chapter5 chapter6 chapter7 chapter8 chapter9 chapter10 chapter11 chapter12 chapter13
PLATFORM = $(shell uname)

ifeq ($(findstring Linux,$(PLATFORM)),Linux)
	LFLAGS += -ledit
	FILES += chapter4b
endif

ifeq ($(findstring Darwin,$(PLATFORM)),Darwin)
	LFLAGS += -ledit
	FILES += chapter4b
endif

all: $(FILES)

%: %.c ../mpc/mpc.c
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@
  
