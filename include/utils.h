#include <time.h>

#define STR_BUF 256
#define DELAY 5

typedef struct times_before_s {
	char now[STR_BUF];
	char ten_minutes_ago[STR_BUF];
	char two_hours_ago[STR_BUF];
	char one_week_ago[STR_BUF];
	char one_year_ago[STR_BUF];
} times_before_t;

