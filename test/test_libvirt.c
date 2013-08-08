#include "../common.h"


int main(void) {
	char * uri="qemu+tcp://127.0.0.1/system";
	virConnectPtr conn;
	conn = openConnect(uri, NULL, 0);
	int doms;
	doms = virConnectNumOfDomains(conn);
	fprintf(stderr, "doms: %d\n", doms);
}
