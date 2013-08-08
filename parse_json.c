#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"


cJSON *json;


json_config_t * parse_config_json(void)
{
    char *data;
	json_config_t * config = NULL;

    data = read_file("server.conf");
	json = cJSON_Parse(data);
	if (!json) {printf("Error before: [%s]\n",cJSON_GetErrorPtr());}
	else
	{
        int i=0;
        cJSON *all_server, *server_t, *interval_t, *daemon_t;
        cJSON *host, *username, *password;
        
        host_info_t * all_hosts = calloc(MAX_XEN_HOST, sizeof(host_info_t));
        all_host_t * xen_config = calloc(1, sizeof(all_host_t));
        xen_config->size = 0;
        xen_config->hosts = all_hosts;
        xen_config->file_buf = data;
        
        all_server = cJSON_GetObjectItem(json, "servers");
        interval_t = cJSON_GetObjectItem(json, "interval");
        daemon_t = cJSON_GetObjectItem(json, "daemon");
        
        while((server_t = cJSON_GetArrayItem(all_server, i)) != NULL) {
            host = cJSON_GetObjectItem(server_t, "host");
            username = cJSON_GetObjectItem(server_t, "username");
            password = cJSON_GetObjectItem(server_t, "password");
            xen_config->hosts[i].hostname = host->valuestring;
            xen_config->hosts[i].username = username->valuestring;
            xen_config->hosts[i].password = password->valuestring;
            xen_config->size = i;
            i++;
        }
        
        config = (json_config_t *)calloc(1, sizeof(json_config_t));
        config->all_servers = (void *)xen_config;
        config->server_interval = interval_t->valueint;
        config->server_daemon = daemon_t->valueint;
	}

	return config;
}


char *read_file(char *filename)
{
	int ret;FILE *f=fopen(filename,"rb");fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);
	char *data=(char*)malloc(len+1);ret=fread(data,1,len,f);fclose(f);
    return data;
}


void parse_config(json_config_t **configuration) {
    json_config_t * config = parse_config_json();
    *configuration = config;
    
    /*
    if(config->all_servers != NULL) {
    all_host_t * xen_hosts = (all_host_t *)config->all_servers;
    while(xen_hosts->hosts->hostname != NULL) {
        fprintf(stderr, "%s : %s : %s\n",
                xen_hosts->hosts->hostname,
                xen_hosts->hosts->username,
                xen_hosts->hosts->password
        );
        xen_hosts->hosts++;
    }
    
    fprintf(stderr, "interval: %d\ndaemon: %d\n", config->server_interval, config->server_daemon);
    }
    else
    {
        fprintf(stderr, "Empty config file\n");
    }
    */
}


void parse_config_free(json_config_t * config) {
    free(config->all_servers);
    free(((all_host_t *)config->all_servers)->hosts);
    free(((all_host_t *)config->all_servers)->file_buf);
    cJSON_Delete(json);
    free(config);
}



