CXXFLAGS = -Wall -Wextra -pedantic -O3
LFLAGS =  -lgsl -lgslcblas -lm


.PHONY: new all reset

all: plot.pdf

%.pdf: %.asy
	asy -f pdf $<

plot.pdf: all07.data all24.data
