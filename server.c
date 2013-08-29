#include "include/common.h"

#define DEBUG 0
#define server_signature "nginx/0.7.69"

int port = 1997;
int nthreads = 100;
char *converter_module = NULL;
char *converter_func = NULL;

mongo *mongo_conn;
xen_session *session;
hashmap *hashMap;

void exit_hook(int);


method_map_t method_map [] = {
    {"get_vm", get_vm},
    {"get_vm_list", get_vm_list},
    {"get_perf", get_perf},
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

    pModule = PyImport_ImportModule(module);
    if(pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, func);
        if(pFunc != NULL) {
            va_list vargs;
            va_start(vargs, format);
            pParm = Py_VaBuildValue(format, vargs);
            va_end(vargs);

            pRetVal = PyEval_CallObject(pFunc, pParm);

            return pRetVal;
            //char *ret;
            //PyArg_Parse(pRetVal, "s", &ret);
            //return ret;
        }
        else {
            printf("import function error\n");
        }
    }
    else {
        printf("import module error\n");
    }
    return NULL;
}


int head_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    char *pos;
    char *length_str = "Content-Length: ";
    pos = strstr(ptr, length_str);
    if(pos) {
        int length_str_len = (int)strlen(length_str);
        int length_ptr = (int)strlen(ptr);
        char content_length[128];
        memcpy(content_length, ptr + length_str_len, length_ptr - length_str_len);
        strip(content_length);
        int length = atoi(content_length);
        struct data_return *ret = (struct data_return *)stream;
        ret->data = calloc(length, 1);
    }
    
    return (int)size * nmemb;
}


int write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct data_return *ret = (struct data_return *)stream;
    int data_len = (int)strlen(ret->data);
    memcpy(ret->data + data_len, ptr, (int)size * nmemb);
    return (int)size * nmemb;
}


char *get_perf(struct evhttp_request *req, struct evkeyvalq *params)
{
    char *output;
    char host[1024];
    char type[1024];
    sprintf(host, "%s", evhttp_find_header(params, "host"));
    sprintf(type, "%s", evhttp_find_header(params, "type"));
    
    size_t str_length = 0;
    if((str_length = strlen(host)) == 0)
    {
        output = method_error("Parameter is empty: host");
        return output;
    }
    if((str_length = strlen(type)) == 0)
    {
        output = method_error("Parameter is empty: type");
        return output;
    }
    
    xen_session * host_session;
    //从hash table中根据hostname获取session
    host_session = (xen_session *)hmap_get(hashMap, (void *)host);
    if(host_session == NULL) {
        output = method_error("Can not find host");
        return output;
    }
    
    //根据type参数的类型，获取对应的开始时间
    int cmp_ret;
    char *start;
    times_before_t *before = get_before_now();
    if((cmp_ret=memcmp(type, "10m", strlen("10m"))) == 0) {
        start = before->ten_minutes_ago;
    }
    else if((cmp_ret=memcmp(type, "2h", strlen("2h"))) == 0) {
        start = before->two_hours_ago;
    }
    else if((cmp_ret=memcmp(type, "1w", strlen("1w"))) == 0) {
        start = before->one_week_ago;
    }
    else if((cmp_ret=memcmp(type, "1y", strlen("1y"))) == 0) {
        start = before->one_year_ago;
    }
    else {
        start = before->ten_minutes_ago;
    }
    
    //使用libcurl调用XenServer API
    char *url_format = "http://%s/rrd_updates?session_id=%s&start=%s&cf=AVERAGE";
    char url[1024];
    sprintf(url, url_format, host, host_session->session_id, start);
    
    struct data_return data_return_p[1];
    CURL *curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_data);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &data_return_p);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data_return_p);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    CURLcode result = curl_easy_perform(curl);
    
    // start executer python script
    PyGILState_STATE state;
    state = PyGILState_Ensure();
    PyObject *ret;
    PyObject *item, *item2, *uuid, *data;
    PyObject *key, *value;
    PyObject *original_list, *dump_list;
    
    //调用python,返回列表类型
    ret = PyCall(converter_module, converter_func, "(s)", data_return_p->data);
    original_list = PyList_GetItem(ret, 0);
    dump_list = PyList_GetItem(ret, 1);
    char * dumps_json = PyString_AsString(dump_list);
    //fprintf(stderr, "%s\n", dumps_json);
    
    Py_ssize_t ret_size = PyList_GET_SIZE(original_list);
    int i;
    //获取最外层列表的元素
    //每个元素是字段，{uuid: "string", data: {}}
    for(i=0; i<ret_size; i++) {
        item = PyList_GetItem(original_list, i);
        uuid = PyDict_GetItemString(item, "uuid");
        data = PyDict_GetItem(item, PyString_FromString("data"));
        
        Py_ssize_t pos = 0;
        char *uuid_str = PyString_AsString(uuid);
        
        //查询DB中是否有uuid的记录
        //如果有插入新记录包含_oid
        //否则不包含
        //因为MongoDB不允许更新oid
        bson query[1];
        bson_init(query);
        bson_append_string(query, "uuid", uuid_str);
        bson_finish(query);
        int mg_ret = mongo_find_one(mongo_conn,
                                    "wisemonitor.virtual_host",
                                    query,
                                    bson_shared_empty(),
                                    NULL);
        
        bson b[1];
        bson_init(b);
        if(mg_ret != 0) {
            //如果uuid的记录不存在，则插入_oid
            bson_append_new_oid(b, "_id");
        }
        bson_append_string(b, "time_stamp", before->now);
        bson_append_string(b, "uuid", uuid_str);
        bson_append_start_object(b, "data");
        
        //fprintf(stderr, "\n%s\n", uuid_str);
        //循环获取data字典的内容
        //每个元素的内容是{"cpu0": [{"12:00:00": 123}, {}, ...]}
        while(PyDict_Next(data, (Py_ssize_t *)&pos, &key, &value)) {
            char * k = PyString_AsString(key);
            //fprintf(stderr, "\n%s\n", k);
            bson_append_start_array(b, k);
            
            //得到最内层的列表
            //之所以用列表是为了保持顺序
            //因为python的字典是无序的
            Py_ssize_t data_list_size = PyList_GET_SIZE(value);
            
            int j;
            //循环列表里的每个元素
            //元素是仅为单个key:value的字典
            for(j=0; j<data_list_size; j++) {
                char num[256];
                sprintf(num, "%d", j);
                bson_append_start_object(b, num);
                
                //item2为列表中的每个字典元素
                item2 = PyList_GetItem(value, j);
                
                PyObject *key1, *value1;
                Py_ssize_t pos1 = 0;
                //获取字典的key和value
                while(PyDict_Next(item2, (Py_ssize_t *)&pos1, &key1, &value1)) {
                    char *key1_str = PyString_AsString(key1);
                    char *value1_str = PyString_AsString(value1);
                    //fprintf(stderr, "%s: %s\n", key1_str, value1_str);
                    bson_append_string(b, key1_str, value1_str);
                }
                bson_append_finish_object(b);
            }
            
            bson_append_finish_object(b);
        }
        
        bson_append_finish_object(b);
        bson_finish(b);
        
        if(mg_ret != MONGO_OK) {
            //insert data to MongoDB
            if(mongo_insert(mongo_conn, "wisemonitor.virtual_host", b, NULL) != MONGO_OK) {
                fprintf(stderr, "FAIL: Failed to insert document whth err: %s\n", mongo_conn->lasterrstr);
            }
        }
        else {
            // update
            bson cond[1];
            bson_init(cond);
            bson_append_string(cond, "uuid", uuid_str);
            bson_finish(cond);
            
            bson op[1];
            bson_init(op);
            bson_append_bson(op, "$set", b);
            bson_finish(op);
            
            int update_ret = mongo_update(mongo_conn,
                         "wisemonitor.virtual_host",
                         cond,
                         op,
                         MONGO_UPDATE_MULTI,
                         0);
            if (update_ret == MONGO_OK) {
                fprintf(stderr, "Record Updated Finished!\n");
            }
            else {
                fprintf(stderr, "%s\nRecord Updated Error!\n", mongo_conn->lasterrstr);
            }
            bson_destroy(cond);
            bson_destroy(op);
        }
        
        bson_destroy(query);
        bson_destroy(b);
    }
    
    PyGILState_Release(state);
    // end execute python
    
    cJSON *json_root, *json_data, *json_vms, *single_vm;
    //createobject是在堆上分配的内存
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("0"));
    cJSON_AddStringToObject(json_root, "session_id", session->session_id);
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString("ok"));
    cJSON_AddStringToObject(json_root, "objects", dumps_json);
    
    output = cJSON_PrintUnformatted(json_root);
    
    cJSON_Delete(json_root);
    curl_easy_cleanup(curl);
    
    return output;
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
        fprintf(stderr, "Can not find host parameter");
        return NULL;
    }
    if((str_length = strlen(uuid)) == 0)
    {
        fprintf(stderr, "Can not find host parameter");
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
    
    cJSON *json_root, *json_data, *json_vms, *single_vm;
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
        fprintf(stderr, "Can not find host parameter");
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
        xen_vm_get_record(host_session, &vm_record, vms->contents[j]);
        
        //xen_vm_metrics_set *vm_metrics_set = xen_vm_metrics_set_alloc(128);
        //xen_vm_metrics_get_all(host_session, &vm_metrics_set);
        
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
    evhttp_add_header(req->output_headers, "Access-Control-Allow-Origin", "*");
    evhttp_add_header(req->output_headers, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    evhttp_add_header(req->output_headers, "Access-Control-Allow-Headers", "X-Requested-With");
    evhttp_add_header(req->output_headers, "Access-Control-Max-Age", "86400");
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
    
    // set converter module and func
    converter_module = config->convert_module;
    converter_func = config->convert_func;
    
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

