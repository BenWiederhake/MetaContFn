# MetaContFn -- enumerates all meta-containing functions
# Copyright (C) Ben Wiederhake
# All rights reserved

all: mcf

mcf: mcf.cpp
	g++ -std=c++11 -Wall -Wextra -pedantic -Werror -O2 -g \
		-o mcf mcf.cpp

.PHONY: run
run: mcf
	./mcf
