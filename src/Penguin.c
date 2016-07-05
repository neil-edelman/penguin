/** 2016 Neil Edelman
 <p>
 This unpacks the CRON and MISN data files from EVNEW's export to text for
 GraphViz. Assumes and-positives and or-negatives like in the stock game.
 <p>
 I'm realising I should have done the in Java. It would have been so much more
 elegant.

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

/* globals */

static struct Bit bits[100001]; /* EV Bible: max 10000 */
static const int bits_size = sizeof bits / sizeof(struct Bit);

static struct Misn misns[1000 + 128];
static const int misns_size = sizeof misns / sizeof(struct Misn);

static struct Cron crons[2048]; /* unsure of the bound -- I saw it somewhere */
static const int crons_size = sizeof crons / sizeof(struct Cron);

/* private prototypes */

static int different(int *const a, int *const b);
static int same(int *const a, int *const b);
static int difference(int *const a, int *const b);
static int list_contains_int(struct List *const l, const int i);
static void print_int(const int *const pb);
static void print_ints(struct List *const list);
static void print_bit(struct Bit *const b);
static void print_misn(struct Misn *const m);
static void print_cron(struct Cron *const c);
static void parse_resource(void *const reso, const struct Helper *const helper, const int helper_size);
static void parse_bits(const char *const from, struct Cluster *const b, struct Cluster *const m, const size_t bit_misn_cluster, const int misn);
static void cluster_add_bit(struct Cluster *const c, const int is_set, const int bit, const size_t bit_misn_cluster, const int misn);
static void cluster_add_misn(struct Cluster *const c, const int is_set, const int misn);
static void delete_bit_from_misn(struct Misn *const misn, int bit);
static void misn_sort_bits(struct Misn *const misn);
static int misn_compare(const struct Misn *const a, const struct Misn *const b);

static void cull_bits_reset_by_crons(void);
static void cull_misns_out_degree_zero(void);
static void merge_misns(void);
static void print_edges(const int vertex, const struct Cluster *const bit, const struct Cluster *const misn, const char *const label, const char *const sub_label);
static char *insert_scanf_useless_space(char *const str);
static void escape(char *const string);
static void usage(void);

/* functions */

/** Contains in list? @see{list_contains_int}
 @implements	ListPredicate */
static int different(int *const a, int *const b) {
	return *a != *b;
}

/** @implements	ListPredicate */
static int same(int *const a, int *const b) {
	return *a == *b;
}

/** @implements ListMetric */
static int difference(int *const a, int *const b) {
	return *a - *b;
}

/** Only call it on int lists! */
static int list_contains_int(struct List *const l, const int i) {
	return !ListShortCircuit(l, (ListPredicate)&different, (int *)&i);
}

/** See @see{print_bits}.
 @implements	ListAction */
static void print_int(const int *const pb) { fprintf(stderr, " %d", *pb); }

/** Prints list<int> on stderr. */
static void print_ints(struct List *const list) {
	if(!list) {
		fprintf(stderr, "0");
		return;
	}
	fprintf(stderr, "[");
	ListForEach(list, (ListAction)&print_int);
	fprintf(stderr, " ]");
}

static void print_cluster(struct Cluster *const cluster) {
	fprintf(stderr, "<");
	print_ints(cluster->set);
	fprintf(stderr, ",!");
	print_ints(cluster->clear);
	fprintf(stderr, ">");
}

static void print_bit(struct Bit *const b) {
	const struct Helper *help;
	int i;

	if(!b) {
		fprintf(stderr, "<null>\n");
		return;
	}
	fprintf(stderr, "Bit%d is %s\n", b->bit, b->is_used ? "used" : "not used");
	if(!b->is_used) return;
	for(i = 0; i < misn_helper_size; i++) {
		help = misn_helper + i;
		fprintf(stderr, "\t%s: ", help->name);
		print_cluster((struct Cluster *)((char *)b + help->bit_resource_cluster));
		fprintf(stderr, "\n");
	}
}

static void print_misn(struct Misn *const m) {
	const struct Helper *help;
	int i;

	if(!m) {
		fprintf(stderr, "<null>\n");
		return;
	}
	fprintf(stderr, "Misn%d:<%s> is %s\n", m->id, m->name, m->is_used ? "used" : "not used");
	if(!m->is_used) return;
	for(i = 0; i < misn_helper_size; i++) {
		help = misn_helper + i;
		fprintf(stderr, "\t%s: ", help->name);
		print_cluster((struct Cluster *)((char *)m + help->bit_cluster));
		fprintf(stderr, "\n");
	}
}

static void print_cron(struct Cron *const c) {
	const struct Helper *help;
	int i;

	if(!c) {
		fprintf(stderr, "<null>\n");
		return;
	}
	fprintf(stderr, "Cron%d is %s\n", c->id, c->is_used ? "used" : "not used");
	if(!c->is_used) return;
	for(i = 0; i < cron_helper_size; i++) {
		help = cron_helper + i;
		fprintf(stderr, "\t%s: ", help->name);
		print_cluster((struct Cluster *)((char *)c + help->bit_cluster));
		fprintf(stderr, "\n");
	}
}

/** This parses the bits that are in the Helper. */
static void parse_resource(void *const reso, const struct Helper *const helper, const int helper_size) {
	int i, misn = (struct Misn *)reso - misns;

	if(misn < 0 || misn > misns_size) misn = 0;
	for(i = 0; i < helper_size; i++) parse_bits((char *)reso + helper[i].raw,
		(struct Cluster *)((char *)reso + helper[i].bit_cluster),
		helper[i].misn_cluster ?
			(struct Cluster *)((char *)reso + helper[i].misn_cluster) : 0,
		helper[i].bit_resource_cluster, misn);
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

static void delete_bit_from_misn(struct Misn *const misn, int bit) {
	struct Cluster *cluster;
	int no = 0, i;

	if(!misn->is_used) return;
	for(i = 0; i < misn_helper_size; i++) {
		cluster = (struct Cluster *)((char *)misn + misn_helper[i].bit_cluster);
		no += ListRemoveIf(cluster->set, (ListPredicate)&same, &bit);
		no += ListRemoveIf(cluster->clear, (ListPredicate)&same, &bit);
	}
	/*if(no) fprintf(stderr, "removed %d b%d from Misn%d.\n", no, bit, misn->id);*/
}

/* unused */
static void misn_sort_bits(struct Misn *const misn) {
	struct Cluster *cluster;
	int i;
	
	if(!misn->is_used) return;
	for(i = 0; i < misn_helper_size; i++) {
		cluster = (struct Cluster *)((char *)misn + misn_helper[i].bit_cluster);
		ListSort(cluster->set, (ListMetric)&difference);
		ListSort(cluster->clear, (ListMetric)&difference);
	}
}

static int misn_compare(const struct Misn *const a, const struct Misn *const b) {
	struct Cluster *acluster, *bcluster;
	int i, diff;

	if(!a->is_used) return b->is_used ? -1 : 0; else if(!b->is_used) return 1;

	/* sort all misn bits */
	for(i = 0; i < misn_helper_size; i++) {
		acluster = (struct Cluster *)((char *)a + misn_helper[i].bit_cluster);
		bcluster = (struct Cluster *)((char *)b + misn_helper[i].bit_cluster);
		ListSort(acluster->set, (ListMetric)&difference);
		ListSort(acluster->clear, (ListMetric)&difference);
		ListSort(bcluster->set, (ListMetric)&difference);
		ListSort(bcluster->clear, (ListMetric)&difference);
		if((diff = ListCompare(acluster->set, bcluster->set, (ListMetric)&difference)) || (diff = ListCompare(acluster->clear, bcluster->clear, (ListMetric)&difference))) return diff;
	}
	return 0;
}

/** Entry point.
 @return		Either EXIT_SUCCESS or EXIT_FAILURE. */
int main(int argc, char **argv) {
	struct Cron c, *pc;
	struct Misn m, *pm;
	char read[2048], *r;
	const int read_size = sizeof read / sizeof(char);
	int i;

	/* display help? */
	if(argc > 1) { usage(); return EXIT_SUCCESS; }

	/* read all */

	while(fgets(read, read_size, stdin)) {

		if(!strpbrk(read, "\n\r") || !(r = insert_scanf_useless_space(read))) {
			fprintf(stderr, "Line too long.\n");
			break;
		}

		/* zero temp */
		memset(&c, 0, sizeof c);
		c.is_used = -1;
		memset(&m, 0, sizeof m);
		m.is_used = -1;

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

	cull_bits_reset_by_crons();

	/* fixme: eg, 379: Report Mu'hari; Vellos24a: if it sets a bit and then clears it
	 in success with no other bit being affected, then why bother */

	/* cull crons that do not connect to misns */

	/* combine misns that only differ by one availible-not bit */

	cull_misns_out_degree_zero();

	merge_misns();

	/* print all */

	printf("digraph misn {\nrankdir = \"LR\";\n\n");

	/* print misns and vertices */
	printf("node [shape=Mrecord style=filled fillcolor=\"#1111EE5f\"];\n");
	for(i = 0; i < misns_size; i++) {
		pm = misns + i;
		if(!pm->is_used) continue;
		printf("misn%d [label=\"{%d: %s|{<accept>accept|<refuse>refuse}|<ship>ship|{<success>success|<failure>failure|<abort>abort}}\"];\n", pm->id, pm->id, pm->name);

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
		if(!pc->is_used) continue;
		printf("cron%d [label=\"{%d: %s|{<start>start|<end>end}}\"];\n", pc->id, pc->id, pc->name);

		print_edges(i, &pc->b_enable, 0, "cron", 0);
		print_edges(i, &pc->b_start, 0, "cron", "start");
		print_edges(i, &pc->b_end, 0, "cron", "end");

	}
	printf("\n");

	/* print bits for all used bits */
	printf("node [constraint=false shape=plain style=dotted fillcolor=\"#11EE115f\"];\n");
	for(i = 0; i < bits_size; i++) {
		if(!bits[i].is_used) continue;
		printf("bit%d [label=\"%d\" constraint=false shape=plain style=dotted fillcolor=\"#11EE115f\"];\n", i, i);
	}
	printf("\n");

	printf("}\n");

	/* fixme: free */

	return EXIT_SUCCESS;
}

/** only the most obvious! (most famous b6666) */
static void cull_bits_reset_by_crons(void) {
	struct List *ignore_bits = List(sizeof(int));
	struct Misn *misn;
	int i, *pbit;

	/* first we must get the bits reset by crons */
	for(i = 0; i < crons_size; i++) {
		struct Cron *const cron = crons + i;
		int is_start;
		int *pstart, *pend, b;

		if(!cron->is_used) continue;
		if(ListSize(cron->b_enable.set) != 1) continue;
		if(ListSize(cron->b_enable.clear) != 0) continue;
		if(ListSize(cron->b_start.clear) + ListSize(cron->b_end.clear) != 1) continue;
		if(ListSize(cron->b_start.set) != 0 || ListSize(cron->b_end.set) != 0) continue;
		is_start = (ListSize(cron->b_start.clear) == 1);
		pstart = ListGet(cron->b_enable.set, 0);
		pend = ListGet((ListSize(cron->b_start.clear) == 1) ? cron->b_start.clear : cron->b_end.clear, 0);
		/* match */
		if((b = *pstart) != *pend) continue;
		/* misn's available set bits, than it can safely be ignored */
		if(!ListSize(bits[b].misn_available.set)) ListAdd(ignore_bits, &b);
		cron->is_used = 0; /* we don't care about the cron */
	}
	fprintf(stderr, "Cron ignore bits: ");
	ListSort(ignore_bits, (ListMetric)&difference); /* just because */
	print_ints(ignore_bits);
	fprintf(stderr, "\n");

	/* delete all misn bits that are in ignore_bits */
	for(i = 0; i < misns_size; i++) {
		misn = misns + i;
		if(!misn->is_used) continue;
		while((pbit = ListIterate(ignore_bits))) delete_bit_from_misn(misn, *pbit);
	}
	/* also delete the bits from bits */
	while((pbit = ListIterate(ignore_bits))) bits[*pbit].is_used = 0;

	List_(&ignore_bits);
}

/** nodes who's out degrees are zero are useless */
static void cull_misns_out_degree_zero(void) {
	int i;

	for(i = M_ACCEPT; i < misns_size; i++) {
		struct Misn *const misn = misns + i;
		int no = 0, field;
		if(!misn->is_used) continue;
		for(field = 0; field < misn_helper_size; field++) {
			struct Cluster *cluster = (struct Cluster *)((char *)misn + misn_helper[field].bit_cluster);
			no += ListSize(cluster->set);
			no += ListSize(cluster->clear);
		}
		if(no) continue;
		misn->is_used = 0;
		fprintf(stderr, "Misn%d has zero out-degree; it has been repressed.\n", misn->id);
	}
}

/** merges the misns that have the same topology */
static void merge_misns(void) {
	struct Misn *misn, *nisn;
	int m, n;
	char temp[128];

	for(m = 0; m < misns_size; m++) {
		misn = misns + m;
		if(!misn->is_used) continue;
		for(n = m + 1; n < misns_size; n++) {
			nisn = misns + n;
			if(!nisn->is_used || misn_compare(misn, nisn)) continue;
			snprintf(temp, sizeof temp, "\\n%d: %s", nisn->id, nisn->name);
			strncat(misn->name, temp, misn_name_size);
			nisn->is_used = 0;
			fprintf(stderr, "Misn%d merged with Misn%d.\n", nisn->id, misn->id);
		}
	}
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
			if(!misns[*pm].is_used) continue;
			printf("%s%d:%s -> misn%d [color=green];\n", label, vertex, sub_label, *pm);
		}
		while((pm = ListIterate(misn->clear))) {
			if(!misns[*pm].is_used) continue;
			printf("%s%d:%s -> misn%d [color=green arrowhead=empty style=dashed];\n", label, vertex, sub_label, *pm);
		}
	} else {
		/* test expression -- edge incident to node */
		while((pb = ListIterate(bit->set))) {
			printf("bit%d -> %s%d;\n", *pb, label, vertex);
		}
		while((pb = ListIterate(bit->clear))) {
			/*if(list_contains_int(ignore_bits, *pb)) {
				fprintf(stderr, "Ignoring Misn%d test clear b%d.\n", vertex, *pb);
				continue;
			}*/
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
	fprintf(stderr, "Usage: %s < novadata.tsv > allmisns.gv\n\n", programme);
	fprintf(stderr, "The input file, eg, novadata.tsv, is misns and crons exported with EVNEW text\n");
	fprintf(stderr, "1.0.1; the output file, eg, allmisns.gv, is a GraphViz file; see\n");
	fprintf(stderr, "http://www.graphviz.org/. Then one could get a graph,\n\n");
	fprintf(stderr, "dot (or fdp, etc) allmisns.gv -O -Tpdf, or use the GUI.\n\n");
	fprintf(stderr, "Assumes all positive bits can be grouped in a minterm and all negative bits a\n");
	fprintf(stderr, "maxterm; this is (mostly?) true for stock EV:Nova.\n\n");
	fprintf(stderr, "Version %d.%d.\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n\n", programme, year);
}
