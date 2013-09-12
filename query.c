#include "include/common.h"

extern mongo *mongo_conn;


char * query_perfdata(char *uuid, char* type) {
  bson query[1];
  bson_init(query);
  //bson_append_string(query, "uuid", "5ccf18aa-f996-3000-a770-a4d9e8556224");
  bson_append_string(query, "uuid", uuid);
  //bson_append_string(query, "type", "10m");
  bson_append_string(query, "type", type);
  bson_finish(query);
  
  bson out[1];
  bson fields[1];
  bson_init(fields);
  bson_append_int(fields, "data", 1);
  bson_finish(fields);
  
  mongo_cursor * cursor;
  cursor = mongo_find(mongo_conn, "wisemonitor.virtual_host", query, fields, 30, 0, 0);
  
  while(mongo_cursor_next(cursor) == MONGO_OK) {
    bson_iterator it[1];
    if(bson_find(it, &cursor->current, "data")) {
      bson sub[1];
      bson_iterator_subobject_init(it, sub, 0);
      
      
      bson_iterator it1[1];
      bson_iterator_init(it1, sub);
      
      // {"cpu": {"0": {}, "1": {}}
      while(bson_iterator_next(it1)) {
	const char * key_filed = bson_iterator_key(it1);
	
	bson sub1[1];
	bson_iterator_subobject_init(it1, sub1, 0);
	
	bson_iterator it2[1];
	bson_iterator_init(it2, sub1);
	
	// {"0": {"data": "xxx", "time": "xxx"}, "1": {}}
	while(bson_iterator_next(it2)) {
	  //const char * key_no = bson_iterator_key(it2);
	  
	  bson sub2[1];
	  bson_iterator_subobject_init(it2, sub2, 0);
	  
	  bson_iterator it3[1];
	  bson_iterator_init(it3, sub2);
	  
	  printf("%s:\n", key_filed);
	  // {"data": "xxx", "time": "xxx"}
	  bson_iterator temp[1];
	  if(bson_find(temp, sub2, "time")) {
	    printf("%s\n", bson_iterator_string(temp));
	  }
	  if(bson_find(temp, sub2, "data")) {
	    printf("%d\n\n", bson_iterator_int(temp));
	  }
	  
	}
	//break;
      }
    }
  }
}
  