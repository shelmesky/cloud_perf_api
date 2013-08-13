#include "common.h"

#define DEBUG 0
#define server_signature "nginx/0.7.69"

int port = 1997;
int nthreads = 100;

xen_session *session;
hashmap *hashMap;


method_map_t method_map [] = {
    {"get_vm", get_vm},
    {"get_vm_list", get_vm_list},
    {NULL, NULL}
};


void exit_hook(int number) {
    PyGILState_Ensure();
    Py_Finalize();
}


char * PyCall(const char * module, const char *func, const char *format, ... ) {
    PyObject *pModule = NULL;
    PyObject *pFunc = NULL;
    PyObject *pParm = NULL;
    PyObject *pRetVal = NULL;

    pModule = PyImport_ImportModule(module);
    if(pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, func);
        if(pFunc != NULL) {
            va_list vargs;
            va_start(vargs, format);
            pParm = Py_VaBuildValue(format, vargs);
            va_end(vargs);

            pRetVal = PyEval_CallObject(pFunc, pParm);

            char *ret;
            PyArg_Parse(pRetVal, "s", &ret);
            return ret;
        }
        else {
            printf("import function error\n");
        }
    }
    else {
        printf("import module error\n");
    }
}


char* get_vm(struct evhttp_request* req , struct evkeyvalq* params)
{
    // start executer python script
    PyGILState_STATE state;
    state = PyGILState_Ensure();
    
    char * ret = PyCall("get", "get", "(s)", "hahaha");
    
    PyGILState_Release(state);
    // end execute python
    
    char *output;
    char uuid[1024];
    char host[1024];
    sprintf(uuid, "%s", evhttp_find_header(params, "uuid"));
    sprintf(host, "%s", evhttp_find_header(params, "host"));
    
    size_t str_length = 0;
    if((str_length = strlen(host)) == 0)
    {
        fprintf(stderr, "Can not find host parameter");
        return;
    }
    if((str_length = strlen(uuid)) == 0)
    {
        fprintf(stderr, "Can not find host parameter");
        return;
    }
    
    xen_vm vm;
    xen_session * host_session;
    xen_vm_metrics vm_metrics;
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
    
    cJSON *json_root, *json_data, *json_vms, *single_vm;
    //createobject是在堆上分配的内存
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("0"));
    cJSON_AddStringToObject(json_root, "session_id", session->session_id);
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString("ok"));
    cJSON_AddItemToObject(json_root, "data", json_data=cJSON_CreateObject());
    //cJSON_AddItemToObject(json_root, "pyret", cJSON_CreateString(ret));
    
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
        fprintf(stderr, "Can not find host parameter");
        return;
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
    cJSON_AddItemToObject(json_root, "data", json_data=cJSON_CreateObject());
    
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
        xen_vm_get_record(host_session, &vm_record, vms->contents[j]);
        
        if(!vm_record->is_a_snapshot && !vm_record->is_a_template &&
            !vm_record->is_control_domain && !vm_record->is_snapshot_from_vmpp)
        {
            single_vm = cJSON_CreateObject();
            cJSON_AddItemToArray(json_vms, single_vm);
            cJSON_AddStringToObject(single_vm, "uuid", vm_record->uuid);
            cJSON_AddStringToObject(single_vm, "name_label", vm_record->name_label);
            running_vm_size++;
        }
    }
    
    cJSON_AddNumberToObject(json_data, "running_size", running_vm_size);
    
    output = cJSON_PrintUnformatted(json_root);
    
    cJSON_Delete(json_root);
    xen_vm_record_free(vm_record);
    xen_vm_set_free(vms);
    
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
    char * output;
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
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
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
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, exit_hook);
    signal(SIGKILL, exit_hook);
    signal(SIGQUIT, exit_hook);
    signal(SIGTERM, exit_hook);
    signal(SIGHUP, exit_hook);

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
    
    //init hashmap
    hashMap = mk_hmap(str_hash_fn, str_eq_fn, str_del_fn, 1);
    
    if(config->all_servers != NULL) {
        //登录XenServer API
        all_host_t * xen_hosts = (all_host_t *)config->all_servers;
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
    
    parse_config_free(config);
    free_hmap(hashMap);
    return 0;
}

