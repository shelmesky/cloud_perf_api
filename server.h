
int bind_socket(int);
void *dispatch(void *);
void api_handler(struct evhttp_request *, void *);
void generic_handler(struct evhttp_request *, void *);
char *get_vm(struct evhttp_request *, struct evkeyvalq *);
char *get_vm_list(struct evhttp_request *, struct evkeyvalq *);
char *method_notfound(void);
char *method_error(const char *);
