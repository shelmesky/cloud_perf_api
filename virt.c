#include "common.h"


virConnectPtr openConnect(char *uri, virConnectAuthPtr auth_ptr, int flags) {
    if(flags == 0)
    {
        flags = 2;
    }
    
    virConnectPtr conn;
    if(auth_ptr != NULL) {
        conn = virConnectOpenAuth(uri, auth_ptr, flags);
    }
    else {
        conn = virConnectOpen(uri);
    }
    
    if(!conn) {
        fprintf(stderr, "No connection to hypervison: %s\n", (char *)virGetLastError());
        exit(1);
    }
    
    uri = virConnectGetURI(conn);
    if(!uri) {
        fprintf(stderr, "Failed to get URI for hypervisor connectio: %s\n", (char *)virGetLastError());
        exit(1);
    }
        
    fprintf(stderr, "Connected to hypervisor at \"%s\"\n", uri);
    free(uri);
    
    return conn;
}
