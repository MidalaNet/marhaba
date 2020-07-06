
all:
	gcc -Wall -c "publisher.c" -lsqlite3 -ljson-c
	gcc -Wall -o "publisher" "publisher.c" -lsqlite3 -ljson-c
	gcc -Wall -c "aggregator.c" -lcurl -lsqlite3 -ltidy -lmxml
	gcc -Wall -o "aggregator" "aggregator.c" -lcurl -lsqlite3 -ltidy -lmxml

clean:
	rm -rfv *.o publisher aggregator

install:
	cp -pr publisher aggregator ../bin/
