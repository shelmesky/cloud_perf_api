
struct data_return {
    char *data;
    int size;
};

int bind_socket(int);
void *dispatch(void *);
void api_handler(struct evhttp_request *, void *);
void generic_handler(struct evhttp_request *, void *);
char *get_vm(struct evhttp_request *, struct evkeyvalq *);
char *get_vm_list(struct evhttp_request *, struct evkeyvalq *);
char *method_notfound(void);
char *method_error(const char *);
size_t head_data(char *buffer, size_t size, size_t nmemb, void *stream);
size_t write_data(char *buffer, size_t size, size_t nmemb, void *stream);
void *periodical_10m(void *);
void *periodical_2h(void *);
void *periodical_1w(void *);
void *periodical_1y(void *);
void *periodical_get_perf(void *);
int get_perf_from_xenserver(const char*, const char*);
char *query_perfdata(char*, char*);