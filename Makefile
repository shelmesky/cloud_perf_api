
CFLAGS = -g -Iinclude                     \
         $(shell xml2-config --cflags) \
         $(shell curl-config --cflags) \
         -W -Wall -Wmissing-prototypes -std=c99 -fPIC

LDFLAGS = -g $(shell xml2-config --libs) \
          $(shell curl-config --libs) \
	  -Wl,-rpath,$(shell pwd)

all:
	gcc -O0 -o server server.c hash.c virt.c xen.c cJSON.c parse_json.c -lpthread -lvirt -lxenserver -levent -lpython2.7 $(CFLAGS) $(LDFLAGS)
