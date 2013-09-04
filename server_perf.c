#include "include/common.h"

extern char *converter_func;
extern char *converter_module;
extern mongo *mongo_conn;
extern xen_session *session;

int get_perf_from_xenserver(const char *type, const char*url) {
    times_before_t *before = get_before_now();
    
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
    if(0 != result) {
        fprintf(stderr, "error execute curl_easy_perform\n");
        return -1;
    }
    
    int len = strlen(data_return_p->data);
    fprintf(stderr, "Data Len: %d\n", len);
    
    // start executer python script
    PyGILState_STATE state;
    state = PyGILState_Ensure();
    PyObject *ret;
    PyObject *item, *item2, *uuid, *data;
    PyObject *key, *value;
    PyObject *original_list, *dump_list;
    
    //调用python,返回列表类型
    ret = PyCall(converter_module, converter_func, "(s)", data_return_p->data);
    //pylist_getitem return borrowed ref
    original_list = PyList_GetItem(ret, 0);
    dump_list = PyList_GetItem(ret, 1);
    //pystring_asstring return borrowed ref
    char * dumps_json = PyString_AsString(dump_list);
    //fprintf(stderr, "%s\n", dumps_json);
    
    Py_ssize_t ret_size = PyList_GET_SIZE(original_list);
    int i;
    //获取最外层列表的元素
    //每个元素是字段，{uuid: "string", data: {}}
    for(i=0; i<ret_size; i++) {
        item = PyList_GetItem(original_list, i);
        //pydict_getitem return borrowed ref
        uuid = PyDict_GetItemString(item, "uuid");
        data = PyDict_GetItem(item, PyString_FromString("data"));
        
        Py_ssize_t pos = 0;
        //pystring_asstring return borrowed ref
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
        bson_append_string(b, "type", type);
        bson_append_start_object(b, "data");
        
        //fprintf(stderr, "\n%s\n", uuid_str);
        //循环获取data字典的内容
        //每个元素的内容是{"cpu0": [{"12:00:00": 123}, {}, ...]}
        //pydict_next return borrowed ref
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
                
                //get value of key 'time' and 'data' in Dict: item2
                //pydict_getitemstring return borrowed ref
                PyObject *time_val = PyDict_GetItemString(item2, "time");
                PyObject *data_val = PyDict_GetItemString(item2, "data");
                char *time_val_str = PyString_AsString(time_val);
                //pyint_aslong return borrowed ref
                int data_val_int = (int)PyInt_AsLong(data_val);
                bson_append_string(b, "time", time_val_str);
                bson_append_int(b, "data", data_val_int);
                
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
    
    cJSON *json_root;
    //createobject是在堆上分配的内存
    json_root = cJSON_CreateObject();
    cJSON_AddItemToObject(json_root, "status", cJSON_CreateString("0"));
    cJSON_AddStringToObject(json_root, "session_id", session->session_id);
    cJSON_AddItemToObject(json_root, "message", cJSON_CreateString("ok"));
    cJSON_AddStringToObject(json_root, "objects", dumps_json);
    
    char *output = cJSON_PrintUnformatted(json_root);
    free(output);
    
    cJSON_Delete(json_root);
    curl_easy_cleanup(curl);
    free(data_return_p->data);
    
    return 0;
}