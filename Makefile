
# build wserve
build: wserve.c main.c
	gcc -Wall -g main.c -o wserve

# build parser test
test: wserve.c parser.test.c
	gcc -Wall -g parser.test.c -o test
