NAME := $(shell basename $(PWD))
export TASK := M1
all: $(NAME)-64 $(NAME)-32

CFLAGS += -I ../
ARGS += 

include ../Makefile
include ../Makefile.lab

run64: all
	./pstree-64 $(ARGS)

run32: all
	./pstree-32 $(ARGS)