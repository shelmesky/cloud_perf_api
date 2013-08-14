#include "include/common.h"

times_before_t *get_before_now(void) {
	time_t now_time;
	time(&now_time);
	times_before_t *before = calloc(sizeof(times_before_t), 1);
	sprintf(before->now, "%ld", now_time);
	sprintf(before->ten_minutes_ago, "%ld", now_time - (60 * 10) + DELAY);
	sprintf(before->two_hours_ago, "%ld", now_time - (60 * 60 * 2) + DELAY);
	sprintf(before->one_week_ago, "%ld", now_time - (60 * 60 * 24 * 7) + DELAY);
	sprintf(before->one_year_ago, "%ld", now_time - (60 * 60 * 24 * 365) + DELAY);
	return before;
}

void strip(char *buffer){
    register int x=0;
    register int y=0;
    register int z=0;

    if(buffer==NULL || buffer[0]=='\x0')
        return;

    /* strip end of string */
    y=(int)strlen(buffer);
    for(x=y-1;x>=0;x--){
        if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
            buffer[x]='\x0';
        else
            break;
            }
    /* save last position for later... */
    z=x;

    /* strip beginning of string (by shifting) */
    for(x=0;;x++){
        if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
            continue;
        else
            break;
            }
    if(x>0){
        /* new length of the string after we stripped the end */
        y=z+1;
        
        /* shift chars towards beginning of string to remove leading whitespace */
        for(z=x;z<y;z++)
            buffer[z-x]=buffer[z];
        buffer[y-x]='\x0';
            }

    return;
}