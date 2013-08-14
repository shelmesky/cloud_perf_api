
#define MAX_XEN_HOST 512

//typedef struct all_host_s {
//    host_info_t hosts[MAX_XEN_HOST];
//    int size;
//} all_host_t;


typedef struct host_info_s {
    char *hostname;
    char *username;
    char *password;
} host_info_t;

typedef struct all_host_s {
    host_info_t * hosts;
    int size;
    char *file_buf;
} all_host_t;


typedef struct json_config_s {
    void *all_servers;
    int server_interval;
    int server_daemon;
    char *convert_module;
    char *convert_func;
} json_config_t;

json_config_t * parse_config_json(void);
char *read_file(char *);
void parse_config_free(json_config_t *);
