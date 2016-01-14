CC = cc
CFLAGS = -std=c99 -Wall -g
LFLAGS = 
FILES = hello_world prompt doge_code doge_grammar parsing evaluation error_handling s_expressions q_expressions variables functions conditionals strings hand_rolled_parser
PLATFORM = $(shell uname)

ifeq ($(findstring Linux,$(PLATFORM)),Linux)
	LFLAGS += -ledit -lm
	FILES += prompt_unix
endif

ifeq ($(findstring Darwin,$(PLATFORM)),Darwin)
	LFLAGS += -ledit -lm
	FILES += prompt_unix
endif

ifeq ($(findstring MINGW,$(PLATFORM)),MINGW)
	FILES += prompt_windows
endif

all: $(FILES)

%: %.c mpc.c
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@
  
