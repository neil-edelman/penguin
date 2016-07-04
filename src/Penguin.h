struct Cluster {
	struct List *set;
	struct List *clear;
};

enum Type { T_MISN, T_CRON };

enum Where { M_AVAILABLE, M_ACCEPT, M_REFUSE, M_SUCCESS, M_ABORT, M_SHIP, C_ENABLE, C_START, C_END };

struct Bit {
	
	int is_used;
	int bit;
	
	struct Cluster misn_available;
	struct Cluster misn_accept;
	struct Cluster misn_refuse;
	struct Cluster misn_success;
	struct Cluster misn_abort;
	struct Cluster misn_ship;
	struct Cluster cron_enable;
	struct Cluster cron_start;
	struct Cluster cron_end;

};

struct Misn {
	int id;
	char name[128];
	int available_stellar;
	int available_location;
	int available_record;
	int available_rating;
	int available_random;
	int travel_stellar;
	int return_stellar;
	int cargo_type;
	int cargo_amount;
	int cargo_pickup_mode;
	int cargo_dropoff_mode;
	unsigned scan_mask;
	int pay_value;
	int ship_count;
	int ship_system;
	int ship_dude;
	int ship_goal;
	int ship_behavior;
	int ship_name;
	int ship_start;
	int completion_government;
	int completion_reward;
	int ship_subtitle;
	int briefing_desc;
	int quick_briefing_desc;
	int load_cargo_desc;
	int dropoff_cargo_desc;
	int completion_desc;
	int failing_desc;
	int ship_done_desc;
	int refusing_desc;
	int time_limit; /* -1, 0 no time */
	int aux_ship_count;
	int aux_ship_dude;
	int aux_ship_syst;
	int available_ship_type;
	char available_bits[128];
	char on_accept[128];
	char on_refuse[128];
	char on_success[128];
	char on_failure[128];
	char on_abort[128];
	char on_ship_done[128];
	unsigned require_bits;
	int date_increment;
	char accept_button[128];
	char refuse_button[128];
	int display_weight;
	int can_abort;
	unsigned flags_1;
	unsigned flags_2;
	
	/* parse into numeric data */
	struct Cluster b_available, b_accept, b_refuse, b_success, b_failure, b_abort, b_ship;
	struct Cluster misn_accept, misn_refuse, misn_success, misn_failure, misn_abort, misn_ship;
	
};

struct Cron {
	int id;
	char name[128];
	int first_day;
	int first_month;
	int first_year;
	int last_day;
	int last_month;
	int last_year;
	int random;
	int duration;
	int pre_holdoff;
	int post_holdoff;
	char enable_on[128];
	char on_start[128];
	char on_end[128];
	unsigned contribute;
	unsigned require;
	int government_1;
	int government_news_1;
	int government_2;
	int government_news_2;
	int government_3;
	int government_news_3;
	int government_4;
	int government_news_4;
	int independent_news;
	int flags;
	
	/* parse into numeric data */
	struct Cluster b_enable, b_start, b_end;
	
};

/* records the things that go together */
static const struct Helper {
	char *name;
	enum Where where;
	size_t raw, bit_cluster, misn_cluster;
	size_t bit_resource_cluster;
} misn_helper[] = {
	{ "available", M_AVAILABLE, offsetof(struct Misn, available_bits), offsetof(struct Misn, b_available), 0, offsetof(struct Bit, misn_available) },
	{ "accept", M_ACCEPT, offsetof(struct Misn, on_accept), offsetof(struct Misn, b_accept), offsetof(struct Misn, misn_accept), offsetof(struct Bit, misn_accept) },
	{ "refuse", M_REFUSE, offsetof(struct Misn, on_refuse), offsetof(struct Misn, b_refuse), offsetof(struct Misn, misn_refuse), offsetof(struct Bit, misn_refuse) },
	{ "success", M_SUCCESS, offsetof(struct Misn, on_success), offsetof(struct Misn, b_success), offsetof(struct Misn, misn_success), offsetof(struct Bit, misn_success) },
	{ "abort", M_ABORT, offsetof(struct Misn, on_abort), offsetof(struct Misn, b_abort), offsetof(struct Misn, misn_abort), offsetof(struct Bit, misn_abort) },
	{ "on_ship_done", M_SHIP, offsetof(struct Misn, on_ship_done), offsetof(struct Misn, b_ship), offsetof(struct Misn, misn_ship), offsetof(struct Bit, misn_ship) }
}, cron_helper[] = {
	{ "enable", C_ENABLE, offsetof(struct Cron, enable_on), offsetof(struct Cron, b_enable), 0, offsetof(struct Bit, cron_enable) },
	{ "start", C_START, offsetof(struct Cron, on_start), offsetof(struct Cron, b_start), 0, offsetof(struct Bit, cron_start) },
	{ "end", C_END, offsetof(struct Cron, on_end), offsetof(struct Cron, b_end), 0, offsetof(struct Bit, cron_end) }
};
static const int misn_helper_size = sizeof misn_helper / sizeof(struct Helper);
static const int cron_helper_size = sizeof cron_helper / sizeof(struct Helper);
