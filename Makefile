CC = gcc -Wall -g

# build wserve
build: ./src/wserve.c ./src/wserve.h ./src/main.c ./conf/routes.c
	$(CC) ./src/wserve.c ./src/main.c ./conf/routes.c -o ./wserve

# build parser test
test.parser: ./src/wserve.c ./src/wserve.h ./test/parser.test.c
	$(CC) ./src/wserve.c ./test/parser.test.c -o test.parser

# build static routing test
test.static: ./src/wserve.c ./src/wserve.h ./test/static.test.c
	$(CC) ./src/wserve.c ./test/static.test.c -o test.static

test.json: ./src/json.c ./src/json.h
	$(CC) ./src/json.c ./test/json/main.c -o ./test/json/test_json-ws
