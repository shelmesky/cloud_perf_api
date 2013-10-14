#include "include/common.h"

#define DEBUG 0
#define server_signature "nginx/0.7.69"
#define MAX_GET_PERF_THREAD 20

int port = 1997;
int nthreads = 100;
char *converter_module = NULL;
char *converter_func = NULL;

mongo *mongo_conn;
xen_session *session;
hashmap *hashMap;
//用于保存配置文件
json_config_t *global_config = NULL;
//全局线程安全队列
QUEUE *queue;

void exit_hook(int);


method_map_t method_map [] = {
    {"get_perfdata", get_perfdata},
    {NULL, NULL}
};


void exit_hook(int number) {
    PyGILState_Ensure();
    Py_Finalize();
}


PyObject * PyCall(const char * module, const char *func, const char *format, ... ) {
    PyObject *pModule = NULL;
    PyObject *pFunc = NULL;
    PyObject *pParm = NULL;
    PyObject *pRetVal = NULL;

    //pyimport_importmodule return new ref
    pModule = PyImport_ImportModule(module);
    if(pModule != NULL) {
        //pyobject_getattrstring return new ref
        pFunc = PyObject_GetAttrString(pModule, func);
        if(pFunc != NULL) {
            va_list vargs;
            va_start(vargs, format);
            //py_vabuildvalue return new ref
            pParm = Py_VaBuildValue(format, vargs);
            va_end(vargs);

            pRetVal = PyEval_CallObject(pFunc, pParm);
            if(NULL != pRetVal) {
                Py_DecRef(pParm);
                Py_DecRef(pFunc);
                Py_DecRef(pModule);
                return pRetVal;
            } else {
                PyErr_Print(); 
                fprintf(stderr, "Error occured while executer Python!\n");
            }
        }
        else {
            PyErr_Print(); 
            fprintf(stderr, "import function error\n");
        }
    }
    else { 
        PyErr_Print(); 
        fprintf(stderr, "import module error\n");
    }
    
    Py_DecRef(pParm);
    Py_DecRef(pFunc);
    Py_DecRef(pModule);
    return (PyObject *)NULL;
}


size_t head_data(char *buffer, size_t size, size_t nmemb, void *stream)
{
    char *pos;
    char *length_str = "Content-Length: ";
    pos = strstr(buffer, length_str);
    if(pos) {
        size_t length_str_len = strlen(length_str);
        size_t length_ptr = strlen(buffer);
        char content_length[128];
        memcpy(content_length, buffer + length_str_len, length_ptr - length_str_len);
        strip(content_length);
        int length = atoi(content_length);
        fprintf(stderr, "Content-Length: %d\n", length);
        struct data_return *ret = (struct data_return *)stream;
        ret->data = (char *)calloc((length + 1) * sizeof(char), 1);
        ret->size = length;
    }
    
    return size * nmemb;
}


size_t write_data(char *buffer, size_t size, size_t nmemb, void *stream)
{
    struct data_return *ret = (struct data_return *)stream;
    size_t data_len = strlen(ret->data);
    memcpy(ret->data + data_len, buffer, size * nmemb);
    strip(ret->data);
    return size * nmemb;
}


char *get_perfdata(struct evhttp_request *req, struct evkeyvalq *params)
{
    char *output;
    char uuid[512];
    char type[32];
    
    const char *uuid_header = evhttp_find_header(params, "uuid");
    const char *type_header = evhttp_find_header(params, "type");
    if(uuid_header == NULL) {
        output = method_error("Argument 'uuid' is missed!");
        return output;
    }
    if(type_header == NULL) {
        output = method_error("Argument 'type' is missed!");
        return output;
    }
    
    sprintf(uuid, "%s", uuid_header);
    sprintf(type, "%s", type_header);
    
    cJSON *result = query_perfdata(uuid, type);
    if(result != (cJSON *)NULL) {
        output = cJSON_PrintUnformatted(result);
        cJSON_Delete(result);
        return output;
    }
}


char* get_vm(struct evhttp_request* req , struct evkeyvalq* params)
{
    char *output;
    char uuid[1024];
    char host[1024];
    sprintf(uuid, "%s", evhttp_find_header(params, "uuid"));
    sprintf(host, "%s", evhttp_find_header(params, "host"));
    
    size_t str_length = 0;
    if((str_length = strlen(host)) == 0)
    {
        fprintf(stderr, "Can not find host parameter\n");
        return NULL;
    }
    if((str_length = strlen(uuid)) == 0)
    {
        fprintf(stderr, "Can not find host parameter\n");
        return NULL;
    }
    
    xen_vm vm;
    xen_session * host_session;
    //从hash table中根据hostname获取session
    host_session = (xen_session *)hmap_get(hashMap, (void *)host);
    if(host_session == NULL) {
        output = method_error("Can not find host");
        return output;
    }
   
    //根据uuid查询vm
    xen_vm_get_by_uuid(host_session, &vm, uuid);
    xen_vm_record *vm_record = xen_vm_record_alloc();
    //得到vm的record
    xen_vm_get_record(host_session, &vm_record, vm);
    
    cJSON *json_root, *json_data;
    //createobject是在堆上分配的内存
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("0"));
    cJSON_AddStringToObject(json_root, "session_id", session->session_id);
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString("ok"));
    cJSON_AddItemToObject(json_root, "objects", json_data=cJSON_CreateObject());
    
    cJSON_AddStringToObject(json_data, "uuid", vm_record->uuid);
    cJSON_AddStringToObject(json_data, "name_label", vm_record->name_label);
    cJSON_AddNumberToObject(json_data, "mem_static_max", vm_record->memory_static_max);
    cJSON_AddNumberToObject(json_data, "vcpu_max", vm_record->vcpus_max);
    
    output = cJSON_PrintUnformatted(json_root);
    
    cJSON_Delete(json_root);
    xen_vm_record_free(vm_record);
    xen_vm_free(vm);
    
    return output;
}


char * get_vm_list(struct evhttp_request *req,
                                      struct evkeyvalq *params) {
    
    char *output;
    char host[1024];
    sprintf(host, "%s", evhttp_find_header(params, "host"));
    size_t str_length = 0;
    if((str_length = strlen(host)) == 0)
    {
        fprintf(stderr, "Can not find host parameter\n");
        return NULL;
    }
    
    xen_session * host_session;
    host_session = (xen_session *)hmap_get(hashMap, (void *)host);
    if(host_session == NULL) {
        output = method_error("Can not find host");
        return output;
    }
    
    
    cJSON *json_root, *json_data, *json_vms, *single_vm;
    //createobject是在堆上分配的内存
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("0"));
    cJSON_AddStringToObject(json_root, "session_id", session->session_id);
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString("ok"));
    cJSON_AddItemToObject(json_root, "objects", json_data=cJSON_CreateObject());
    
    // start get XenServer Host configuration
    xen_host_set *hosts = xen_host_set_alloc(2);
    xen_host_get_all(host_session, &hosts);
    
    //get name and ip address
    xen_host_record *host_record = xen_host_record_alloc();
    xen_host_get_record(host_session, &host_record, hosts->contents[0]);
    
    //get cpu_count
    xen_string_string_map *cpu_info = xen_string_string_map_alloc(128);
    xen_host_get_cpu_info(host_session, &cpu_info, hosts->contents[0]);
    
    //get cpu model
    xen_host_cpu_set *host_cpu = xen_host_cpu_set_alloc(128);
    xen_host_get_host_cpus(host_session, &host_cpu, hosts->contents[0]);
    xen_host_cpu_record *cpu_record = xen_host_cpu_record_alloc();
    xen_host_cpu_get_record(host_session, &cpu_record, host_cpu->contents[0]);
    
    //get physical NIC
    //more than 1 NIC, get info in a loop
    xen_pif_set *pifs = xen_pif_set_alloc(8);
    xen_host_get_pifs(host_session, &pifs, hosts->contents[0]);
    xen_pif_record *pif_record = xen_pif_record_alloc();
    xen_pif_get_record(host_session, &pif_record, pifs->contents[0]);
    
    xen_host_metrics metrics;
    xen_host_metrics_record *metrics_record = xen_host_metrics_record_alloc();
    xen_host_get_metrics(host_session, &metrics, hosts->contents[0]);
    xen_host_metrics_get_record(host_session, &metrics_record, metrics);
    // end get XenServer configuration
    
    cJSON_AddStringToObject(json_data, "name", host_record->hostname);
    cJSON_AddStringToObject(json_data, "ip", host_record->address);
    cJSON_AddStringToObject(json_data, "edition", host_record->edition);
    
    cJSON_AddStringToObject(json_data, "cpu_count", cpu_info->contents[0].val);
    cJSON_AddStringToObject(json_data, "cpu_modelname", cpu_record->modelname);
    
    cJSON_AddNumberToObject(json_data, "memory_total", metrics_record->memory_total/1024/1024);
    cJSON_AddNumberToObject(json_data, "memory_free", metrics_record->memory_free/1024/1024);
    
    xen_vm_set * vms = xen_vm_set_alloc(100);
    xen_vm_record *vm_record = xen_vm_record_alloc();
    
    //获取所有虚拟机
    xen_vm_get_all(host_session, &vms);
    
    //fprintf(stderr, "All vm: %d\n", (int)vms->size);
    cJSON_AddNumberToObject(json_data, "size", (unsigned int)vms->size);
    
    json_vms = cJSON_CreateArray();
    cJSON_AddItemToObject(json_data, "vms", json_vms);
    
    int vm_size = vms->size, j, running_vm_size = 0;
    //查看每台虚拟机
    //并排除模板、快照、和管理域的虚拟机
    for(j=0; j<vm_size; j++) {
        bool ret_g = xen_vm_get_record(host_session, &vm_record, vms->contents[j]);
        
        if(ret_g) {
            if(!vm_record->is_a_snapshot && !vm_record->is_a_template &&
                !vm_record->is_control_domain && !vm_record->is_snapshot_from_vmpp)
            {
                xen_vm_guest_metrics vm_m;
                xen_vm_get_guest_metrics(host_session, &vm_m, vms->contents[j]);
                xen_vm_guest_metrics_record *vm_r = xen_vm_guest_metrics_record_alloc();
                bool ret_r = xen_vm_guest_metrics_get_record(host_session, &vm_r, vm_m);
                
                if(ret_r) {
                    char *ip = vm_r->networks->contents[0].val;
                
                    single_vm = cJSON_CreateObject();
                    cJSON_AddItemToArray(json_vms, single_vm);
                    cJSON_AddStringToObject(single_vm, "uuid", vm_record->uuid);
                    cJSON_AddStringToObject(single_vm, "name_label", vm_record->name_label);
                    cJSON_AddStringToObject(single_vm, "ip", ip);
                    running_vm_size++;
                }
            }
        }
    }
    
    cJSON_AddNumberToObject(json_data, "running_size", running_vm_size);
    
    output = cJSON_PrintUnformatted(json_root);
    
    cJSON_Delete(json_root);
    xen_vm_record_free(vm_record);
    xen_vm_set_free(vms);
    
    xen_host_set_free(hosts);
    xen_host_record_free(host_record);
    xen_string_string_map_free(cpu_info);
    xen_host_cpu_set_free(host_cpu);
    xen_host_cpu_record_free(cpu_record);
    xen_pif_set_free(pifs);
    xen_pif_record_free(pif_record);
    xen_host_metrics_free(metrics);
    xen_host_metrics_record_free(metrics_record);
    
    return output;
}

char * method_notfound(void) {
    cJSON *json_root;
    //createobject是在堆上分配的内存
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("1"));
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString("method not found"));
    char * output = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return output;
}


char * method_error(const char *msg) {
    cJSON *json_root;
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("1"));
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString(msg));
    char * output = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return output;
}


int bind_socket(int port) {
	int r, nfd;
	nfd = socket(AF_INET, SOCK_STREAM, 0);
	if (nfd < 0) return -1;

	int one = 1;
	r = setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	r = bind(nfd, (struct sockaddr *)&addr, sizeof(addr));
	if (r < 0) return -1;
	r = listen(nfd, 1024);
	if (r < 0) return -1;

	int flags;
	flags = fcntl(nfd, F_GETFL, 0);
    fcntl(nfd, F_SETFL, flags | O_NONBLOCK);
	
	return nfd;
}


void *dispatch(void *arg) {
    event_base_dispatch((struct event_base *)arg);
    return NULL;
}


void api_handler(struct evhttp_request * req, void *arg)
{
    int ret;
    char * output = NULL;
    char *decoded_uri;
    decoded_uri = evhttp_decode_uri(req->uri);
    struct evkeyvalq params;
    evhttp_parse_query(decoded_uri, &params);
    char call[4096];
    sprintf(call, "%s", evhttp_find_header(&params, "call"));
    
    method_map_t * method_map_p;
    method_map_p = method_map;
    while(1)
    {
        if(method_map_p->callback != NULL) {
            if((ret=memcmp(method_map_p->method, call, strlen(call)) == 0))
            {
                output = method_map_p->callback(req, &params);
                break;
            }
            method_map_p++;
        }
        else {
                output = method_notfound();
                break;
        }
    }
    
    struct evbuffer *buf = evbuffer_new();
    if(buf == NULL) return;
    evbuffer_add_printf(buf, "%s", output);
    evhttp_add_header(req->output_headers, "Server", server_signature);
    evhttp_add_header(req->output_headers, "Content-Type", "application/json; charset=UTF-8");
    evhttp_add_header(req->output_headers, "Access-Control-Allow-Origin", "*");
    evhttp_add_header(req->output_headers, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    evhttp_add_header(req->output_headers, "Access-Control-Allow-Headers", "X-Requested-With");
    evhttp_add_header(req->output_headers, "Access-Control-Max-Age", "86400");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
    
    if(output != NULL) {
        free(output);
    }
}


void generic_handler(struct evhttp_request * req, void *arg)
{
    struct evbuffer *buf = evbuffer_new();
    if(buf == NULL) return;
    evbuffer_add_printf(buf, "Rquested not found.\n");
    evhttp_add_header(req->output_headers, "Server", server_signature);
    evhttp_send_reply(req, 404, "not found", buf);
}


int main(void)
{		
    // process signal
    signal(SIGALRM, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, exit_hook);
    signal(SIGKILL, exit_hook);
    signal(SIGQUIT, exit_hook);
    signal(SIGTERM, exit_hook);
    signal(SIGHUP, exit_hook);
    
    //Connect to MongoDB
    mongo conn[1];
    mongo_conn = conn;
    int result;
    MongoConnect(&mongo_conn, "127.0.0.1", 27017, &result);
    if(result != 0) {
        fprintf(stderr, "Failed connect to MongoDB!\n");
    }

    //init python
    Py_Initialize();
    PyEval_InitThreads();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('./')");
    
    //PyEval_ReleaseLock();
    PyEval_ReleaseThread(PyThreadState_Get());
    
    //init xenserver api
    xmlInitParser();
    xmlKeepBlanksDefault(0);
    xen_init();
    curl_global_init(CURL_GLOBAL_ALL);
    
    //parse config.json
    json_config_t * config = (json_config_t *)calloc(1, sizeof(json_config_t));
    parse_config(&config);
    //global_config = &(((all_host_t *)config->all_servers)->hosts);
    global_config = config;
    
    // set converter module and func
    converter_module = config->convert_module;
    converter_func = config->convert_func;
    
    //init hashmap
    hashMap = mk_hmap(str_hash_fn, str_eq_fn, str_del_fn, 1);
    
    all_host_t *xen_hosts = NULL;
    if(config->all_servers != NULL) {
        //登录XenServer API
        xen_hosts = (all_host_t *)config->all_servers;
        while(xen_hosts->hosts->hostname != NULL) {
            char *hostname_ori = xen_hosts->hosts->hostname;
            size_t hostname_len = strlen(hostname_ori);
            char *hostname = calloc(1024, sizeof(char));
            char *http_protocol = "http://";
            strncat(hostname, http_protocol, strlen(http_protocol));
            strncat(hostname, hostname_ori, (int)hostname_len);
            
            session = XenLogin(hostname,
                               xen_hosts->hosts->username,
                               xen_hosts->hosts->password);
            hmap_add(hashMap,
                     (void *)hostname_ori,
                     (void *)session);
            xen_hosts->hosts++;
        }
    }
    
    //重置hosts指针的位置
    xen_hosts->hosts -= xen_hosts->size;
    
    //init QUEUE
    queue = Initialize_Queue();
    
    //start periodical threads
    int thread_ret;
    pthread_t periodical_t[4];
    thread_ret = pthread_create(&periodical_t[0], NULL, periodical_10m, NULL);
    if(0 != thread_ret) return -1;
    
    thread_ret = pthread_create(&periodical_t[1], NULL, periodical_2h, NULL);
    if(0 != thread_ret) return -1;
    
    thread_ret = pthread_create(&periodical_t[2], NULL, periodical_1w, NULL);
    if(0 != thread_ret) return -1;
    
    thread_ret = pthread_create(&periodical_t[3], NULL, periodical_1y, NULL);
    if(0 != thread_ret) return -1;
    
    //start periodical_get_perf
    int n;
    pthread_t periodical_geter[MAX_GET_PERF_THREAD];
    for(n=0; n<MAX_GET_PERF_THREAD; n++) {
        thread_ret = pthread_create(&periodical_geter[n], NULL, periodical_get_perf, NULL);
        if(0 != thread_ret) return -1;
    }
    

	//start http server
	int i, r, nfd;
	nfd = bind_socket(port);
	if(nfd<0) return -1;

    pthread_t threads[nthreads];

	for(i=0; i<nthreads; i++) {
		struct event_base *base = event_init();
		if(base == NULL) return -1;

		struct evhttp *httpd = evhttp_new(base);
		if(httpd == NULL) return -1;

		r = evhttp_accept_socket(httpd, nfd);
		if(r != 0) return -1;

        evhttp_set_allowed_methods(httpd,
                                   EVHTTP_REQ_GET|
                                  EVHTTP_REQ_POST|
                                  EVHTTP_REQ_PUT|
                                  EVHTTP_REQ_DELETE|
                                  EVHTTP_REQ_HEAD|
                                  EVHTTP_REQ_OPTIONS|
                                  EVHTTP_REQ_CONNECT);
        
        evhttp_set_gencb(httpd, generic_handler, NULL);
        evhttp_set_cb(httpd, "/api", api_handler, NULL);
        evhttp_set_timeout(httpd, 3000);
        
        //event_base非线程安全?
        r = pthread_create(&threads[i], NULL, dispatch, base);
        if(r != 0) return -1;
	}
	
	for(i=0; i<nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    //Py_InitializeEx(0);
    parse_config_free(config);
    free_hmap(hashMap);
    return 0;
}

