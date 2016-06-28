/** 2016 Neil Edelman
 <p>
 This unpacks the CRON and MISN data files from EVNEW's export to text for
 GraphViz.

 @author	Neil
 @version	1; 2016-05
 @since		1; 2016-05 */

#include <stdlib.h> /* malloc free strtol */
#include <stdio.h>  /* fprintf */
#include <string.h>	/* memcpy strcat */
#include <limits.h>	/* strtol */
#include <ctype.h>	/* isalpha */
#include <stddef.h>	/* offsetof */

/* constants */
static const char *programme   = "Penguin";
static const char *year        = "2016";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

/* used to keep track of all the bits that are actually used */
static int is_bit[100001]; /* EV Bible: max 10000 */
static const int is_bit_size = sizeof is_bit / sizeof(int);

/* used in Misn and Cron */
struct BitCluster {
	int set[8], clear[8];
	int set_size, clear_size;
	struct Misn *start[8], *abort[8];
};
static const int set_capacity   = sizeof((struct BitCluster *)0)->set   / sizeof(int);
static const int clear_capacity = sizeof((struct BitCluster *)0)->clear / sizeof(int);
static const int start_capacity = sizeof((struct BitCluster *)0)->start / sizeof(struct Misn *);
static const int abort_capacity = sizeof((struct BitCluster *)0)->abort / sizeof(struct Misn *);

/* used in Misn */
enum MisnFlags {
	REQUIRES_NOT6666 = 1,
	SETS_6666 = 2,
	REQUIRES_NOT424 = 4,
	SETS_424 = 8
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
	struct BitCluster available, accept, refuse, success, failure, abort, ship;
	enum MisnFlags flags;

} misn[1000 + 128];
static const int misn_size = sizeof misn / sizeof(struct Misn);

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
	struct BitCluster enable, start, end;

} cron[2048]; /* unsure of the bound -- I saw it somewhere */
static const int cron_size = sizeof cron / sizeof(struct Cron);

/* private */
static int predicate_test(const int bit, const int is_set, enum MisnFlags *const pmf);
static int predicate_set(const int bit, const int is_set, enum MisnFlags *const pmf);
static int add_to_bitcluster(struct BitCluster *const bc, const int bit, const int is_set);
static int parse_misnbits(const char *const str, struct BitCluster *const bc, int (*predicate)(const int, const int, enum MisnFlags *const), enum MisnFlags *const flags);
static void print_edges(const struct BitCluster *const bc, const int vertex, const char *const label, const char *const sub_label);
static char *insert_scanf_useless_space(char *const str);
static void usage(void);

/* this gets so confusing; lines everywhere! so we have to cheat by looking at
 the meaning and setting flags instead; this is used as a callback in
 @see{parse_misnbits}. */
static int predicate_test(const int bit, const int is_set, enum MisnFlags *const pmf) {
	if(is_set) return -1;
	switch(bit) {
		case 6666:
			*pmf |= REQUIRES_NOT6666;
			return 0;
		case 424:
			*pmf |= REQUIRES_NOT424;
			return 0;
		default:
			break;
	}
	return -1;
}

static int predicate_set(const int bit, const int is_set, enum MisnFlags *const pmf) {
	if(!is_set) return -1;
	switch(bit) {
		case 6666:
			*pmf |= SETS_6666;
			return 0;
		case 424:
			*pmf |= SETS_424;
			return 0;
		default:
			break;
	}
	return -1;
}

/** Entry point.
 @return		Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(int argc, char **argv) {
	struct Cron c, *pc;
	struct Misn m, *pm;
	char read[2048], *r;
	const int read_size = sizeof read / sizeof(char);
	int i, no;

	if(argc > 1) { usage(); return EXIT_SUCCESS; }

	while(fgets(read, read_size, stdin)) {

		if(!strpbrk(read, "\n\r") || !(r = insert_scanf_useless_space(read))) {
			fprintf(stderr, "Line too long.\n");
			break;
		}

		/* zero temp */
		memset(&c, 0, sizeof c);
		memset(&m, 0, sizeof m);

		if(sscanf(r, "\"cron\" %d \"%127[^\"]\" %d %d %d %d %d %d %d %d %d %d \"%127[^\"]\" \"%127[^\"]\" \"%127[^\"]\" %x %x %d %d %d %d %d %d %d %d %d %d \"EOR\"\n",
			&c.id, c.name, &c.first_day, &c.first_month, &c.first_year,
			&c.last_day, &c.last_month, &c.last_year, &c.random, &c.duration,
			&c.pre_holdoff, &c.post_holdoff, c.enable_on, c.on_start, c.on_end,
			&c.contribute, &c.require, &c.government_1, &c.government_news_1,
			&c.government_2, &c.government_news_2, &c.government_3,
			&c.government_news_3, &c.government_4, &c.government_news_4,
			&c.independent_news, &c.flags) == 27) {
			if(c.id < 128 || c.id >= cron_size) {
				fprintf(stderr, "cron %d<%s> is not in range %d.\n", c.id, c.name, cron_size);
				continue;
			}

			no = 0;

			no += parse_misnbits(c.enable_on, &c.enable, 0, 0);
			no += parse_misnbits(c.on_start, &c.start, 0, 0);
			no += parse_misnbits(c.on_end, &c.end, 0, 0);

			/* ignore trivial crons (degree == 0) hack (doesn't work; all the
			 system changes have bits but don't do anything) */
			if(!no) continue;

			/* copy to global */
			pc = cron + c.id;
			memcpy(pc, &c, sizeof c);

		} else if(sscanf(r, "\"misn\" %d \"%127[^\"]\" %d %d %d %d %d %d %d %d %d %d %d %x %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \"%127[^\"]\" \"%127[^\"]\" \"%127[^\"]\" \"%127[^\"]\" \"%127[^\"]\" \"%127[^\"]\" \"%127[^\"]\" %x %d \"%127[^\"]\" \"%127[^\"]\" %d %d %x %x \"EOR\"\n",
			&m.id, m.name, &m.available_stellar, &m.available_location,
			&m.available_record, &m.available_rating, &m.available_random,
			&m.travel_stellar, &m.return_stellar, &m.cargo_type,
			&m.cargo_amount, &m.cargo_pickup_mode, &m.cargo_dropoff_mode,
			&m.scan_mask, &m.pay_value, &m.ship_count, &m.ship_system,
			&m.ship_dude, &m.ship_goal, &m.ship_behavior, &m.ship_name,
			&m.ship_start, &m.completion_government, &m.completion_reward,
			&m.ship_subtitle, &m.briefing_desc, &m.quick_briefing_desc,
			&m.load_cargo_desc, &m.dropoff_cargo_desc, &m.completion_desc,
			&m.failing_desc, &m.ship_done_desc, &m.refusing_desc, &m.time_limit,
			&m.aux_ship_count, &m.aux_ship_dude, &m.aux_ship_syst,
			&m.available_ship_type, m.available_bits, m.on_accept, m.on_refuse,
			m.on_success, m.on_failure, m.on_abort, m.on_ship_done,
			&m.require_bits, &m.date_increment, m.accept_button,
			m.refuse_button, &m.display_weight, &m.can_abort, &m.flags_1,
			&m.flags_2) == 53) {
			if(m.id < 128 || m.id >= misn_size) {
				fprintf(stderr, "Misn %d<%s> is not in range %d.\n", m.id, m.name, misn_size);
				continue;
			}

			no = 0;

			no += parse_misnbits(m.available_bits, &m.available, &predicate_test, &m.flags);
			no += parse_misnbits(m.on_accept, &m.accept, &predicate_set, &m.flags);
			no += parse_misnbits(m.on_refuse, &m.refuse, &predicate_set, &m.flags);
			no += parse_misnbits(m.on_success, &m.success, &predicate_set, &m.flags);
			no += parse_misnbits(m.on_failure, &m.failure, &predicate_set, &m.flags);
			no += parse_misnbits(m.on_abort, &m.abort, &predicate_set, &m.flags);
			no += parse_misnbits(m.on_ship_done, &m.ship, &predicate_set, &m.flags);

			/* ignore trivial misns (degree == 0) hack */
			if(!no) continue;

			pm = misn + m.id;
			memcpy(pm, &m, sizeof m);

		}
	};

	printf("digraph misn {\n\n");
	/*printf("rankdir=LR;\n\n");*/
	/*printf("rankdir=TB;\n\n");*/

	/* print bits for all used bits */
	printf("node [shape=plain style=dotted fillcolor=\"#11EE115f\"];\n");
	for(i = 0; i < is_bit_size; i++) {
		if(!is_bit[i]) continue;
		printf("bit%d [label=\"%d\"];\n", i, i);
	}
	printf("\n");

	/* print misns and vertices */
	printf("node [shape=Mrecord style=filled fillcolor=\"#1111EE5f\"];\n");
	for(i = 0; i < misn_size; i++) {
		pm = misn + i;
		if(!pm->id) continue;
		printf("misn%d [label=\"%d: %s%s%s%s%s|{<accept>accept|<refuse>refuse}|<ship>ship|{<success>success|<failure>failure|<abort>abort}\"];\n", pm->id, pm->id, pm->name, pm->flags & REQUIRES_NOT6666 ? "\\n(!6666)" : "", pm->flags & REQUIRES_NOT424 ? "\\n(!424)" : "", pm->flags & SETS_6666 ? "\\n(6666)" : "", pm->flags & SETS_424 ? "\\n(424)" : "");

		print_edges(&pm->available, i, "misn", 0);
		print_edges(&pm->accept, i, "misn", "accept");
		print_edges(&pm->refuse, i, "misn", "refuse");
		print_edges(&pm->success, i, "misn", "success");
		print_edges(&pm->failure, i, "misn", "failure");
		print_edges(&pm->abort, i, "misn", "abort");
		print_edges(&pm->ship, i, "misn", "ship");

	}
	printf("\n");

	/* print crons and vertices */
	printf("node [shape=record style=filled fillcolor=\"#EE11115f\"];\n");
	for(i = 0; i < cron_size; i++) {
		pc = cron + i;
		if(!pc->id) continue;
		printf("cron%d [label=\"%d: %s|{<start>start|<end>end}\"];\n", pc->id, pc->id, pc->name);

		print_edges(&pc->enable, i, "cron", 0);
		print_edges(&pc->start, i, "cron", "start");
		print_edges(&pc->end, i, "cron", "end");

	}
	printf("\n");

	printf("}\n");

	return EXIT_SUCCESS;
}

/**
 @param bc		Pointer to BitCluster.
 @param bit		Misn bit.
 @param is_set	Whether it's set (true) or cleared (false.)
 @return		Success. */
static int add_to_bitcluster(struct BitCluster *const bc, const int bit, const int is_set) {
	is_bit[bit] = -1;
	if(is_set) {
		if(bc->set_size >= set_capacity) {
			fprintf(stderr, "Warning: %d set size overflowed %d-term set capacity on bit %d.\n", bc->set_size, set_capacity, bit);
			return 0;
		}
		bc->set[bc->set_size++] = bit;
	} else {
		if(bc->clear_size >= clear_capacity) {
			fprintf(stderr, "Warning: %d clear size overflowed %d-term clear capacity on bit %d.\n", bc->clear_size, clear_capacity, bit);
			return 0;
		}
		bc->clear[bc->clear_size++] = bit;
	}
	return -1;
}

static int add_misn_to_bitcluster(struct BitCluster *const bc, const int m, const int is_start) {
	struct Misn **const bcmisns = is_start ? bc->start : bc->abort;
	const int capacity = is_start ? start_capacity : abort_capacity;
	int i;

	if(m < 0 || m >= misn_size) {
		fprintf(stderr, "Warning: misn %d is beyond [0, %d).\n", m, misn_size);
		return 0;
	}
	for(i = 0; i < capacity && bcmisns[i]; i++);
	if(i >= capacity) {
		fprintf(stderr, "Warning: misn %d overflowed set-size %d.\n", m, capacity);
		return 0;
	}
	if(!misn[m].id) {
		/*fprintf(stderr, "Warning: misn %d does not exist or has degree zero.\n", m); <- spam! */
		return 0;
	}
	bcmisns[i] = misn + m;
	return -1;
}

/** this is not, generally, the full power of misn bits, but I think that it
 covers the misn bits that are in the stock game; ie, assumes one min &
 max-term, and gets confused by any other
 @param str			String used to parse bits and transfer them to bc.
 @param bc			A bit cluster intended to recieve the bits.
 @param predicate	Custom callback taking bit number, is_set, and a MisnFlags;
					if present, false return means don't record the bit.
 @param flags		If predicate, MisnFlags that will be passed to it.
 @return			Success; if failed, result is undefined. */
static int parse_misnbits(const char *const str, struct BitCluster *const bc, int (*predicate)(const int, const int, enum MisnFlags *const), enum MisnFlags *const flags) {
	int is_invert[16], is_invert_top = 0, is_invert_current, is_note = 0;
	const int is_invert_capacity = sizeof is_invert / sizeof(int);
	int is_invert_size = 0;
	int num = 0;
	long k;
	char *s = (char *)str;

	/*fprintf(stderr, "parse_misnbits: str: %s; bc->set_size: %d; bc->clear_size: %d\n", str, bc->set_size, bc->clear_size);*/
	while(*s) {
		switch(*s) {
			case '(':
				if(is_invert_size >= is_invert_capacity) {
					fprintf(stderr, "Warning: <%s> more then %d parenthesis.\n", str, is_invert_capacity);
					return 0;
				}
				is_invert[is_invert_size] = is_invert_top ^ (is_invert_size ? is_invert[is_invert_size - 1] : 0);
				is_invert_size++;
				is_invert_top = 0;
				/*printf("(");*/
				break;
			case ')':
				if(!is_invert_size) {
					fprintf(stderr, "Warning: <%s> mismatched parenthesis.\n", str);
					return 0;
				}
				is_invert_size--;
				is_invert_top = 0;
				/*printf(")");*/
				break;
			case '!':
				is_invert_top ^= -1;
				/*printf("!");*/
				break;
			case 'b':
			case 'B':
				/* k is the misn bit that we parse */
				k = strtol(s + 1, &s, 0); s--;
				if(k < 0 || k >= is_bit_size) return 0;
				is_invert_current = is_invert_top ^ (is_invert_size ? is_invert[is_invert_size - 1] : 0);
				/*fprintf(stderr, "<%s> %ld:%s\n", str, k, is_invert_current ? "invert" : "normal");*/
				/* call predicate, if true, add to bitcluster */
				if(!predicate || predicate(k, !is_invert_current, flags)) {
					num++;
					add_to_bitcluster(bc, k, !is_invert_current);
				}
				is_invert_top = 0;
				break;
			case 'a': /* abort */
			case 'A':
				/* k is the abort misn */
				k = strtol(s + 1, &s, 0); s--;
				num++;
				add_misn_to_bitcluster(bc, k, 0);
				break;
			case 's': /* start */
			case 'S':
				k = strtol(s + 1, &s, 0); s--;
				num++;
				add_misn_to_bitcluster(bc, k, -1);
				break;
				/* FIXME: now do output! */
			case 'p': /* payed / sound */
			case 'P':
			case 'g': /* gender -- actually, might be improtant */
			case 'G':
			case 'q': /* absquatulate */
			case 'Q':
			case 't': /* name of ship */
			case 'T':
			case 'x': /* explored */
			case 'X':
				is_invert_top = 0;
				break;
			default:
				if(isalpha(*s)) is_note = -1;
				is_invert_top = 0;
		}
		s++;
	}
	if(is_note) fprintf(stderr, "Warning: I don't entirely understand, \"%s.\"\n", str);
	return num;
}

/** if the sub_label is 0, then assumes edge is incedent */
static void print_edges(const struct BitCluster *const bc, const int vertex, const char *const label, const char *const sub_label) {
	int i;

	if(sub_label) {
		/* set expression */
		for(i = 0; i < bc->set_size; i++) {
			printf("%s%d:%s -> bit%d;\n", label, vertex, sub_label, bc->set[i]);
		}
		for(i = 0; i < bc->clear_size; i++) {
			printf("%s%d:%s -> bit%d [color=red arrowhead=empty style=dashed];\n", label, vertex, sub_label, bc->clear[i]);
		}
		/* print edges that start automatically */
		for(i = 0; i < start_capacity; i++) {
			if(!bc->start[i]) break;
			printf("%s%d:%s -> misn%d [color=green];\n", label, vertex, sub_label, bc->start[i]->id);
		}
		for(i = 0; i < abort_capacity; i++) {
			if(!bc->abort[i]) break;
			printf("%s%d:%s -> misn%d [color=green arrowhead=empty style=dashed];\n", label, vertex, sub_label, bc->abort[i]->id);
		}
	} else {
		/* test expression -- edge incident to node */
		for(i = 0; i < bc->set_size; i++) {
			printf("bit%d -> %s%d;\n", bc->set[i], label, vertex);
		}
		for(i = 0; i < bc->clear_size; i++) {
			printf("bit%d -> %s%d [color=red arrowhead=empty style=dashed];\n", bc->clear[i], label, vertex);
		}
	}
}

/** ::facepalm:: scanf can't handle empty strings, for some weird reason; used
 when reading */
static char *insert_scanf_useless_space(char *const str) {
	static char read[2048];
	char *str1, *str2;
	const int read_size = sizeof read / sizeof(char);
	int read_pos = 0, remainder;

	for(str1 = str; (str2 = strstr(str1, "\"\"")); str1 = str2 + 2) {
		if(read_size - read_pos < str2 - str1 + 4) return 0;
		memcpy(read + read_pos, str1, str2 - str1);
		read_pos += str2 - str1;
		memcpy(read + read_pos, "\" \"", 3);
		read_pos += 3;
	}
	/* untested boundry values */
	if((remainder = strlen(str1) + 1) > read_size - read_pos) return 0;
	memcpy(read + read_pos, str1, remainder);
	return read;
}

/** Prints command-line help. */
static void usage(void) {
	fprintf(stderr, "Usage: %s < novadata.tsv > allmisns.gv\n\n", programme);
	fprintf(stderr, "The input file, eg, novadata.tsv, is misns and crons exported with EVNEW text\n");
	fprintf(stderr, "1.0.1; the output file, eg, allmisns.gv, is a GraphViz file; see\n");
	fprintf(stderr, "http://www.graphviz.org/. Then one could get a graph,\n\n");
	fprintf(stderr, "dot (or fdp, etc) allmisns.gv -O -Tpdf, or use the GUI.\n\n");
	fprintf(stderr, "Version %d.%d.\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n\n", programme, year);
}
