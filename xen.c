#include "include/common.h"

extern xen_session session;

static size_t
write_func(char *buffer, size_t size, size_t nmemb, xen_comms *comms)
{
    size_t n = size * nmemb;
    //printf("\n\n---Result from server -----------------------\n");
    //printf("%s\n", buffer);
    //fflush(stdout);
    return comms->func(buffer, n, comms->handle) ? n : 0;
}


static int
call_func(const void *data, size_t len, void *user_handle,
          void *result_handle, xen_result_func result_func)
{
    // 通过user_handle传递url信息
    // void *当然可以传递其他任何
    char *xen_url = (char *)user_handle;

    CURL *curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    xen_comms comms = {
        result_func,
        result_handle
    };

    curl_easy_setopt(curl, CURLOPT_URL, xen_url);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
#ifdef CURLOPT_MUTE
    curl_easy_setopt(curl, CURLOPT_MUTE, 1);
#endif
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &comms);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

    CURLcode result = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    return result;
}


xen_session * XenLogin(const char *url,
                       const char *username,
                       const char *password) {
    xen_session *session = calloc(sizeof(xen_session), 1);
    session = xen_session_login_with_password(call_func, (void *)url,
                                              username,
                                              password,
                                              xen_api_latest_version);
    return session;
}


void * XenGetAllHost(xen_session * session, xen_host_set **hosts) {
    xen_host_get_all(session, hosts);
    return NULL;
}
