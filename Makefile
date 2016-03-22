# MetaContFn -- enumerates all meta-containing functions
# Copyright (C) Ben Wiederhake
# All rights reserved

all: mcf mcf_fast

mcf: mcf.cpp
	g++ -std=c++11 -Wall -Wextra -pedantic -Werror -O2 -g \
		-o mcf mcf.cpp

mcf_fast: mcf.cpp
	g++ -std=c++11 -Wall -Wextra -pedantic -Werror -O3 \
		-DNDEBUG -fomit-frame-pointer -march=native \
		-o mcf_fast mcf.cpp

.PHONY: run
run: mcf
	./mcf

.PHONY: run_fast
run_fast: mcf_fast
	./mcf_fast
