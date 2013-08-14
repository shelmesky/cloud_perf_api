#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_BUF 256
#define DELAY 5

typedef struct times_before_s {
	char now[STR_BUF];
	char ten_minutes_ago[STR_BUF];
	char two_hours_ago[STR_BUF];
	char one_week_ago[STR_BUF];
	char one_year_ago[STR_BUF];
} times_before_t;


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


int main(int argc, char **argv)
{
	times_before_t *before = get_before_now();
	fprintf(stderr, "now is: %s\n", before->now);
	fprintf(stderr, "ten minutes ago is: %s\n", before->ten_minutes_ago);
	fprintf(stderr, "two hours ago is: %s\n", before->two_hours_ago);
	fprintf(stderr, "one week ago is: %s\n", before->one_week_ago);
	fprintf(stderr, "one year ago is: %s\n", before->one_year_ago);
	free(before);
	return 0;
}

