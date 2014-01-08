CC = cc
CFLAGS = -std=c99 -Wall -Werror -Wno-unused -g
LFLAGS = 
FILES = hello_world prompt_windows prompt greeting_code greeting_grammar parsing evaluation error_handling s_expressions q_expressions variables functions conditionals strings
PLATFORM = $(shell uname)

ifeq ($(findstring Linux,$(PLATFORM)),Linux)
	LFLAGS += -ledit
	FILES += prompt_unix
endif

ifeq ($(findstring Darwin,$(PLATFORM)),Darwin)
	LFLAGS += -ledit
	FILES += prompt_windows
endif

all: $(FILES)

%: %.c mpc.c
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@
  
