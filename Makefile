CC  = gcc -Wall -g
SRC = ./src/wserve.c ./src/main.c ./conf/routes.c ./src/json.c
HDR = ./src/wserve.h ./src/json.h
SSL = ${SECURE}
LIBBEARSSL = ./BearSSL/build/libbearssl.a

# build wserve with or without bearssl (https)
build: $(SRC) $(HDR)
ifeq ($(SSL),1)
	$(info HTTPS support with BearSSL)
	$(CC) -DENABLE_SECURE=1 $(SRC) $(LIBBEARSSL) -o ./wserve
else
	$(info Plain HTTP)
	$(CC) $(SRC) -o ./wserve
endif

# build parser test
test.parser: ./src/wserve.c ./src/wserve.h ./test/parser.test.c
	$(CC) ./src/wserve.c ./test/parser.test.c -o test.parser

# build static routing test
test.static: ./src/wserve.c ./src/wserve.h ./test/static.test.c
	$(CC) ./src/wserve.c ./test/static.test.c -o test.static

# build json test (use with JSONTestSuite)
test.json: ./src/json.c ./src/json.h
	$(CC) ./src/json.c ./test/json/main.c -o ./test/json/test_json-ws
