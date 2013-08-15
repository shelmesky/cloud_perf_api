#include <mongo.h>
#include <stdio.h>
#include <stdio.h>


int main(int argc, char **argv)
{
	bson b[1];
	bson query[1];
	mongo conn[1];
	mongo_cursor cursor[1];
	int result;

	if(mongo_client(conn, "127.0.0.1", 27017) != MONGO_OK) {
		switch(conn->err)
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
				fprintf(stderr, "MongoDB connection error number: %d.\n", conn->err);
		}
	}


	bson_init(query);
	bson_append_string(query, "city_name", "南京");
	bson_finish(query);

	mongo_cursor_init(cursor, conn, "bangboox.city_shop");
	mongo_cursor_set_query(cursor, query);
	while(mongo_cursor_next(cursor) == MONGO_OK) {
		bson_print((bson *)mongo_cursor_bson(cursor));
	}

	bson_init(b);

	bson_append_new_oid(b, "_id");
	bson_append_new_oid(b, "record_id");

	bson_append_start_array(b, "items");
		bson_append_start_object(b, "0");
			bson_append_string(b, "name", "roy.lieu");
			bson_append_int(b, "age", 30);
		bson_append_finish_object(b);

		bson_append_start_object(b, "1");
			bson_append_string(b, "name", "jimmy.chen");
			bson_append_int(b, "age", 35);
		bson_append_finish_object(b);
	bson_append_finish_object(b);

	bson_append_start_object(b, "address");
	bson_append_string(b, "stree", "Jufeng RD.");
	bson_append_int(b, "zip", 232044);
	bson_append_finish_object(b);

	bson_finish(b);

	//printf("\n\n");
	//bson_print(b);
	
	if(mongo_insert(conn, "test.record", b, NULL) != MONGO_OK) {
		fprintf(stderr, "FAIL: Failed to insert document whth err: %d\n", conn->err);
	}

	return 0;
}
