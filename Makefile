
CFLAGS = -g -Iinclude                     \
         $(shell xml2-config --cflags) \
         $(shell curl-config --cflags) \
         -W -Wall -Wmissing-prototypes -std=c99 -fPIC

LDFLAGS = $(shell xml2-config --libs) \
          $(shell curl-config --libs) \
	  -Wl,-rpath,$(shell pwd)

all:
	clang -O3 -o server server.c hash.c virt.c xen.c cJSON.c parse_json.c utils.c mongoc.c queue.c server_perf.c py_exception.c -lpthread -lvirt -lxenserver -levent -lpython2.7 -lmongoc $(CFLAGS) $(LDFLAGS) -DHMAP_THREAD_SAFE -lrt

debug:
	clang -O0 -o server server.c hash.c virt.c xen.c cJSON.c parse_json.c utils.c mongoc.c queue.c server_perf.c py_exception.c -lpthread -lvirt -lxenserver -levent -lpython2.7_d -lmongoc $(CFLAGS) $(LDFLAGS) -DPy_DEBUG -DHMAP_THREAD_SAFE -g =lrt
