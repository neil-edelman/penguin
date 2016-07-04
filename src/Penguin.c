/** 2016 Neil Edelman
 <p>
 This unpacks the CRON and MISN data files from EVNEW's export to text for
 GraphViz.

 @author	Neil
 @version	1.1; 2016-05
 @since		1; 2016-05 */

#include <stdlib.h> /* malloc free strtol */
#include <stdio.h>  /* fprintf */
#include <string.h>	/* memcpy strcat */
#include <limits.h>	/* strtol */
#include <ctype.h>	/* isalpha */
#include <stddef.h>	/* offsetof */
#include "List.h"
#include "Penguin.h"

/* constants */

static const char *programme   = "Penguin";
static const char *year        = "2016";
static const int versionMajor  = 1;
static const int versionMinor  = 1;

/* types */

enum Mode { M_HELP, M_ALL, M_REMOVE_USELESS, M_STRICT };

/* globals */

static struct Bit bits[100001]; /* EV Bible: max 10000 */
static const int bits_size = sizeof bits / sizeof(struct Bit);

static struct Misn misns[1000 + 128];
static const int misns_size = sizeof misns / sizeof(struct Misn);

static struct Cron crons[2048]; /* unsure of the bound -- I saw it somewhere */
static const int crons_size = sizeof crons / sizeof(struct Cron);

/* private prototypes */

static void print_bit(const int *const pb);
static void print_bits(struct List *const list);
static void parse_resource(void *const reso, const struct Helper *const helper, const int helper_size);
static void parse_bits(const char *const from, struct Cluster *const b, struct Cluster *const m, const size_t bit_misn_cluster, const int misn);
static void cluster_add_bit(struct Cluster *const c, const int is_set, const int bit, const size_t bit_misn_cluster, const int misn);
static void cluster_add_misn(struct Cluster *const c, const int is_set, const int misn);

static void print_edges(const int vertex, const struct Cluster *const bit, const struct Cluster *const misn, const char *const label, const char *const sub_label);
static char *insert_scanf_useless_space(char *const str);
static void escape(char *const string);
static void usage(void);

/* functions */

/** Contains in list? !list.forEach(&different)
 @implements	ListPredicate */
int different(int *const misn_no, int *const compare) { return *misn_no != *compare; }

/** See @see{print_bits}.
 @implements	ListAction */
static void print_bit(const int *const pb) { fprintf(stderr, " %d", *pb); }

/** Prints list<int> on stderr. */
static void print_bits(struct List *const list) {
	if(!list) {
		fprintf(stderr, "0");
		return;
	}
	fprintf(stderr, "[");
	ListForEach(list, (ListAction)&print_bit);
	fprintf(stderr, " ]");
}

static void print_cluster(struct Cluster *const cluster) {
	fprintf(stderr, "<");
	print_bits(cluster->set);
	fprintf(stderr, ",!");
	print_bits(cluster->clear);
	fprintf(stderr, ">");
}

/** This parses the bits that are in the Helper. */
static void parse_resource(void *const reso, const struct Helper *const helper, const int helper_size) {
	int i, misn = (struct Misn *)reso - misns;

	if(misn < 0 || misn > misns_size) misn = 0;
	for(i = 0; i < helper_size; i++) parse_bits((char *)reso + helper[i].raw,
		(struct Cluster *)((char *)reso + helper[i].bit_cluster),
		helper[i].misn_cluster ?
			(struct Cluster *)((char *)reso + helper[i].misn_cluster) : 0,
		helper->bit_resource_cluster, misn);
}

/** Parse from and stick in into b and m. */
static void parse_bits(const char *const from, struct Cluster *const b, struct Cluster *const m, const size_t bit_resource_cluster, const int misn) {
	int is_invert[16], is_invert_top = 0, is_invert_current, is_note = 0;
	const int is_invert_capacity = sizeof is_invert / sizeof(int);
	int is_invert_size = 0;
	long k;
	char *f = (char *)from;

	while(*f) {
		switch(*f) {
			case '(':
				if(is_invert_size >= is_invert_capacity) {
					fprintf(stderr, "Warning: <%s> more then %d parenthesis.\n", from, is_invert_capacity);
					return;
				}
				is_invert[is_invert_size] = is_invert_top ^ (is_invert_size ? is_invert[is_invert_size - 1] : 0);
				is_invert_size++;
				is_invert_top = 0;
				/*printf("(");*/
				break;
			case ')':
				if(!is_invert_size) {
					fprintf(stderr, "Warning: <%s> mismatched parenthesis.\n", from);
					return;
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
				k = strtol(f + 1, &f, 0); f--;
				if(k < 0 || k >= bits_size) return;
				is_invert_current = is_invert_top ^ (is_invert_size ? is_invert[is_invert_size - 1] : 0);
				/*fprintf(stderr, "<%s> %ld:%s\n", from, k, is_invert_current ? "invert" : "normal");*/
				/*if(!predicate || predicate(k, !is_invert_current, flags)) {*/
				cluster_add_bit(b, !is_invert_current, k, bit_resource_cluster, misn);
				is_invert_top = 0;
				break;
			case 'a': /* abort */
			case 'A':
				/* k is the abort misn */
				k = strtol(f + 1, &f, 0); f--;
				if(m) {
					cluster_add_misn(m, 0, k);
				} else {
					fprintf(stderr, "Warning: <%s> abort misn in a test field? ignoring.\n", from);
				}
				break;
			case 's': /* start */
			case 'S':
				/* k is the start misn */
				k = strtol(f + 1, &f, 0); f--;
				if(m) {
					cluster_add_misn(m, -1, k);
				} else {
					fprintf(stderr, "Warning: <%s> start misn in a test field? ignoring.\n", from);
				}
				break;
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
				if(isalpha(*f)) is_note = -1;
				is_invert_top = 0;
		}
		f++;
	}
	/*fprintf(stderr, "from<%s> bits:", from);
	print_cluster(b);
	if(m) {
		fprintf(stderr, " misns:");
		print_cluster(m);
	}
	fprintf(stderr, "\n");*/
	if(is_note) fprintf(stderr, "Warning: I don't entirely understand, \"%s.\"\n", from);
}

static void cluster_add_bit(struct Cluster *const c, const int is_set, const int bit, const size_t bit_resource_cluster, const int misn) {
	/* pointer to the list of bits of the misn, set or clear */
	struct List **const ppm = is_set ? &c->set : &c->clear;
	/* select bit_misn_cluster from bits[bit] */
	struct Cluster *const bit_misn = (struct Cluster *)((char *)(bits + bit) + bit_resource_cluster);
	/* pointer to the list of misns of the bit, set or clear */
	struct List **const ppb = is_set ? &bit_misn->set : &bit_misn->clear;

	if(bit < 0 || bit > bits_size) {
		fprintf(stderr, "Warning: %d is outside the range [0, %d).\n", bit, bits_size);
		return;
	}
	bits[bit].is_used = -1;
	bits[bit].bit     = bit;
	if(!*ppm) *ppm = List(sizeof(int));
	ListAdd(*ppm, &bit);
	/* now add it in the Bit, as well */
	if(!*ppb) *ppb = List(sizeof(int));
	ListAdd(*ppb, &misn);
}

static void cluster_add_misn(struct Cluster *const c, const int is_set, const int misn) {
	struct List **ppm = is_set ? &c->set : &c->clear;

	if(misn < 0 || misn > misns_size) {
		fprintf(stderr, "Warning: %d is outside the range [0, %d).\n", misn, misns_size);
		return;
	}
	if(!*ppm) *ppm = List(sizeof(int));
	ListAdd(*ppm, &misn);
}

/** Entry point.
 @return		Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(int argc, char **argv) {
	enum Mode mode = M_HELP;
	struct Cron c, *pc;
	struct Misn m, *pm;
	char read[2048], *r;
	const int read_size = sizeof read / sizeof(char);
	int i;

	/* what mode? */
	if(argc == 2) {
		if(!strcmp("all", argv[1])) mode = M_ALL;
		else if(!strcmp("some", argv[1])) mode = M_REMOVE_USELESS;
		else if(!strcmp("strict", argv[1])) mode = M_STRICT;
	}
	if(mode == M_HELP) { usage(); return EXIT_SUCCESS; }

	/* read all */

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
			if(c.id < 128 || c.id >= crons_size) {
				fprintf(stderr, "cron %d<%s> is not in range %d.\n", c.id, c.name, crons_size);
				continue;
			}
			escape(c.name);
			
			pc = crons + c.id;
			memcpy(pc, &c, sizeof c);
			parse_resource(pc, cron_helper, cron_helper_size);

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
			if(m.id < 128 || m.id >= misns_size) {
				fprintf(stderr, "Misn %d<%s> is not in range %d.\n", m.id, m.name, misns_size);
				continue;
			}
			escape(m.name);

			pm = misns + m.id;
			memcpy(pm, &m, sizeof m);
			parse_resource(pm, misn_helper, misn_helper_size);

		}
	};

	/* get rid of stuff */

	if(mode == M_REMOVE_USELESS || mode == M_STRICT) {
	}

	if(mode == M_STRICT) {
	}

	/* print all */

	printf("digraph misn {\n\n");

	/* print bits for all used bits */
	printf("node [shape=plain style=dotted fillcolor=\"#11EE115f\"];\n");
	for(i = 0; i < bits_size; i++) {
		if(!bits[i].is_used) continue;
		printf("bit%d [label=\"%d\"];\n", i, i);
	}
	printf("\n");

	/* print misns and vertices */
	printf("node [shape=Mrecord style=filled fillcolor=\"#1111EE5f\"];\n");
	for(i = 0; i < misns_size; i++) {
		pm = misns + i;
		if(!pm->id) continue;
		printf("misn%d [label=\"%d: %s|{<accept>accept|<refuse>refuse}|<ship>ship|{<success>success|<failure>failure|<abort>abort}\"];\n", pm->id, pm->id, pm->name);

		print_edges(i, &pm->b_available, 0, "misn", 0);
		print_edges(i, &pm->b_accept, &pm->misn_accept, "misn", "accept");
		print_edges(i, &pm->b_refuse, &pm->misn_refuse, "misn", "refuse");
		print_edges(i, &pm->b_success, &pm->misn_success, "misn", "success");
		print_edges(i, &pm->b_failure, &pm->misn_failure, "misn", "failure");
		print_edges(i, &pm->b_abort, &pm->misn_abort, "misn", "abort");
		print_edges(i, &pm->b_ship, &pm->misn_ship, "misn", "ship");

	}
	printf("\n");

	/* print crons and vertices */
	printf("node [shape=record style=filled fillcolor=\"#EE11115f\"];\n");
	for(i = 0; i < crons_size; i++) {
		pc = crons + i;
		if(!pc->id) continue;
		printf("cron%d [label=\"%d: %s|{<start>start|<end>end}\"];\n", pc->id, pc->id, pc->name);

		print_edges(i, &pc->b_enable, 0, "cron", 0);
		print_edges(i, &pc->b_start, 0, "cron", "start");
		print_edges(i, &pc->b_end, 0, "cron", "end");

	}
	printf("\n");

	printf("}\n");

	for(i = 0; i < bits_size; i++) {
		if(!bits[i].is_used) continue;
		fprintf(stderr, "b%d.available: ", i);
		print_cluster(&bits[i].misn_available);
		fprintf(stderr, "\n");
	}

	return EXIT_SUCCESS;
}

/** if the sub_label is 0, then assumes edge is incedent */
static void print_edges(const int vertex, const struct Cluster *const bit, const struct Cluster *const misn, const char *const label, const char *const sub_label) {
	int *pb, *pm;

	if(sub_label) {
		/* set expression */
		while((pb = ListIterate(bit->set))) {
			printf("%s%d:%s -> bit%d;\n", label, vertex, sub_label, *pb);
		}
		while((pb = ListIterate(bit->clear))) {
			printf("%s%d:%s -> bit%d [color=red arrowhead=empty style=dashed];\n", label, vertex, sub_label, *pb);
		}
		/* print edges that start automatically */
		if(!misn) return;
		while((pm = ListIterate(misn->set))) {
			printf("%s%d:%s -> misn%d [color=green];\n", label, vertex, sub_label, *pm);
		}
		while((pm = ListIterate(misn->clear))) {
			printf("%s%d:%s -> misn%d [color=green arrowhead=empty style=dashed];\n", label, vertex, sub_label, *pm);
		}
	} else {
		/* test expression -- edge incident to node */
		while((pb = ListIterate(bit->set))) {
			printf("bit%d -> %s%d;\n", *pb, label, vertex);
		}
		while((pb = ListIterate(bit->clear))) {
			printf("bit%d -> %s%d [color=red arrowhead=empty style=dashed];\n", *pb, label, vertex);
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

/** GraphViz has special meaning for these symbols; hack, just change them */
static void escape(char *const string) {
	char *s;

	for(s = string; *s; s++) {
		switch(*s) {
			case '<': *s = '['; break;
			case '>': *s = ']'; break;
			case '|': *s = '/'; break;
			default: break;
		}
	}
}

/** Prints command-line help. */
static void usage(void) {
	fprintf(stderr, "Usage: %s [mode] < novadata.tsv > allmisns.gv\n\n", programme);
	fprintf(stderr, "Where [mode] is one of { all, some, strict }; all does not prune, some does\n");
	fprintf(stderr, "obvious pruning, and strict prunes it the best.\n\n");
	fprintf(stderr, "The input file, eg, novadata.tsv, is misns and crons exported with EVNEW text\n");
	fprintf(stderr, "1.0.1; the output file, eg, allmisns.gv, is a GraphViz file; see\n");
	fprintf(stderr, "http://www.graphviz.org/. Then one could get a graph,\n\n");
	fprintf(stderr, "dot (or fdp, etc) allmisns.gv -O -Tpdf, or use the GUI.\n\n");
	fprintf(stderr, "Version %d.%d.\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n\n", programme, year);
}
