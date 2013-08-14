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
