#include "include/common.h"

void *MongoConnect(mongo **conn, const char * host, const int port, int *result){
    *result = -1;
    if(mongo_client(*conn, host, port) != MONGO_OK) {
        switch((*conn)->err)
        {
            case MONGO_CONN_SUCCESS:
                fprintf(stderr, "OK: Connected to MongoDB!\n");
                break;
            case MONGO_CONN_NO_SOCKET:
                fprintf(stderr, "FAIL: Cloud not create a socket!\n");
                break;
            case MONGO_CONN_FAIL:
                fprintf(stderr, "FAIL: Could not connect to mongod!.");
                break;
            default:
                fprintf(stderr, "MongoDB connection error number: %d.\n", (*conn)->err);
        }
    }
    *result = 0;
    return NULL;
}