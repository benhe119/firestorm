/*
 * This file is part of Firestorm NIDS.
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <stdio.h>

#include <firestorm.h>
#include <f_capture.h>
#include <f_packet.h>
#include <f_decode.h>

#define NAMESPACE_ALLOC_CHUNK	16
#define NAMESPACE_ALLOC_MASK	(NAMESPACE_ALLOC_CHUNK-1)

struct _namespace _ns_arr[NS_MAX] = {
	[NS_DLT] {.ns_label = "DLT"},
	[NS_UNIXPF] {.ns_label = "UNIX"},
	[NS_ETHER] {.ns_label = "ETHER"},
	[NS_INET] {.ns_label = "INET"},
	[NS_INET6] {.ns_label = "INET6"},
	[NS_IPX] {.ns_label = "IPX"},
	[NS_APPLE] {.ns_label = "APPLE"},
	[NS_CISCO] {.ns_label = "CISCO"},
	[NS_USTREAM] {.ns_label = "STREAM"},
	[NS_UDGRAM] {.ns_label = "DGRAM"},
	[NS_USEQPKT] {.ns_label = "SEQPKT"},
};

static unsigned int num_decoders;
static struct _decoder *decoders;

void proto_add(struct _decoder *d, struct _proto *p)
{
	assert(p != NULL && p->p_label != NULL);
	assert(d != NULL && d->d_label != NULL);

	p->p_next = d->d_protos;
	d->d_protos = p;
}

decoder_t decoder_get(proto_ns_t ns, proto_id_t id)
{
	assert(ns < NS_MAX);
	return _ns_entry_search(_ns_arr[ns].ns_reg,
				_ns_arr[ns].ns_num_reg, id);
}

static int nsentry_cmp(const void *A, const void *B)
{
	const struct _ns_entry *a = A, *b = B;
	return a->nse_id - b->nse_id;
}

void decode_init(void)
{
	static const char * const fn = "decode.dot";
	struct _decoder *d;
	struct _proto *p;
	unsigned int i;
	FILE *f;

	f = fopen(fn, "w");
	assert(f != NULL);

	fprintf(f, "strict digraph \"Internal Decode Graph\" {\n");
	fprintf(f, "\tgraph[rankdir=LR];\n");
	fprintf(f, "\tnode[shape=rectangle, style=filled, "
			"fillcolor=transparent];\n");

	for(i = 0; i < NS_MAX; i++) {
		unsigned int j;

		qsort(_ns_arr[i].ns_reg,
			_ns_arr[i].ns_num_reg,
			sizeof(*_ns_arr[i].ns_reg),
			nsentry_cmp);

		if ( _ns_arr[i].ns_num_reg )
			fprintf(f, "\t\"ns_%s\" [label=\"%s\" "
				"fillcolor=\"#b0b0ff\"];\n",
				_ns_arr[i].ns_label,
				_ns_arr[i].ns_label);
		for(j = 0; j < _ns_arr[i].ns_num_reg; j++)
			fprintf(f, "\t\"ns_%s\" -> \"d_%s\" "
				"[label=\"id 0x%x\"];\n",
				_ns_arr[i].ns_label,
				_ns_arr[i].ns_reg[j].nse_decoder->d_label,
				_ns_arr[i].ns_reg[j].nse_id);
	}

	for(d = decoders; d; d = d->d_next) {
		fprintf(f, "\t\"d_%s\" [label=\"%s\" "
			"fillcolor=\"#b0ffb0\"];\n",
			d->d_label, d->d_label);
		for(p = d->d_protos; p; p = p->p_next) {
			fprintf(f, "\t\"d_%s\" -> \"p_%s\";\n",
				d->d_label, p->p_label);
			fprintf(f, "\t\"p_%s\" [label=\"%s\" "
				"fillcolor=\"#ffb0b0\"\n];",
				p->p_label, p->p_label);
		}
	}

	fprintf(f, "}\n");
	fclose(f);
	mesg(M_INFO, "decode: %s: dumped protocol graph", fn);
}

const char *decoder_label(decoder_t d)
{
	assert(d->d_label != NULL);
	return d->d_label;
}

void decoder_add(struct _decoder *d)
{
	assert(d != NULL && d->d_label != NULL);
	d->d_next = decoders;
	decoders = d;
	num_decoders++;
}

static int ns_assure(struct _namespace *ns)
{
	static void *new;

	if ( ns->ns_num_reg & NAMESPACE_ALLOC_MASK )
		return 1;

	new = realloc(ns->ns_reg,
			sizeof(*ns->ns_reg) *
			(ns->ns_num_reg + NAMESPACE_ALLOC_CHUNK));
	if ( new == NULL )
		return 0;
	
	ns->ns_reg = new;
	return 1;
}

void decoder_register(struct _decoder *d, proto_ns_t ns, proto_id_t id)
{
	unsigned int i;
	assert(ns < NS_MAX);

	for(i = id; i < _ns_arr[ns].ns_num_reg; i++) {
		if ( _ns_arr[ns].ns_reg[i].nse_id == id ) {
			mesg(M_WARN, "decode: %s: %s / 0x%x registered by %s",
				d->d_label, _ns_arr[ns].ns_label, id,
				_ns_arr[ns].ns_reg[i].nse_decoder->d_label);
			return;
		}
	}

	assert(ns_assure(&_ns_arr[ns]));
	_ns_arr[ns].ns_reg[_ns_arr[ns].ns_num_reg].nse_id = id;
	_ns_arr[ns].ns_reg[_ns_arr[ns].ns_num_reg].nse_decoder = d;
	_ns_arr[ns].ns_num_reg++;

	return;
}

void decode(struct _source *s, struct _pkt *p)
{
	static unsigned int i;

	mesg(M_DEBUG, "packet %u", ++i);

	p->pkt_nxthdr = p->pkt_base;
	s->s_decoder->d_decode(p);

	if ( p->pkt_nxthdr < p->pkt_end )
		hex_dump(p->pkt_nxthdr, p->pkt_end - p->pkt_nxthdr, 16);
	else
		printf("\n");
}