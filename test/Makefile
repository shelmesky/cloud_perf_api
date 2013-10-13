python:
	gcc -static -static-libgcc -o t test.c -Wl,-dn /usr/lib/python2.7/config/libpython2.7.a /usr/local/lib/libz.a -lssl -lcrypto -Wl,-dy -lm -lpthread -ldl -lutil -lz -lc

time:
	gcc -o itime time.c

mongo:
	gcc -o mg mongo.c -lmongoc --std=c99
