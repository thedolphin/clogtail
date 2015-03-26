all: clogtail

clogtail: clogtail.c
	gcc -g -o clogtail clogtail.c
