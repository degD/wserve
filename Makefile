CC = gcc -Wall -g

# build wserve
build: ./src/wserve.c ./src/wserve.h ./src/main.c
	$(CC) ./src/wserve.c ./src/main.c  -o ./wserve

# build parser test
test: ./src/wserve.c ./src/wserve.h ./test/parser.test.c
	$(CC) ./src/wserve.c ./test/parser.test.c -o test.parser
