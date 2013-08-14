#include <event.h>
#include <evhttp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <python2.7/Python.h>
#include "virt.h"
#include "hash.h"
#include "xen.h"
#include "cJSON.h"
#include "parse_json.h"
#include "server.h"
#include "utils.h"


typedef struct
{
    xen_result_func func;
    void *handle;
} xen_comms;

typedef struct method_map_s
{
    char *method;
    char *(*callback) (struct evhttp_request *,
                                        struct evkeyvalq *);
} method_map_t;


extern virConnectPtr openConnect(char *, virConnectAuthPtr, int);
extern xen_session *XenLogin(const char *, const char*, const char*);
extern void *XenGetAllHost(xen_session *, xen_host_set **);
extern void parse_config(json_config_t **);
extern void parse_config_free(json_config_t *);
extern char * PyCall(const char *, const char *, const char *, ... );
extern times_before_t *get_before_now(void);