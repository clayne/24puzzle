/*-
 * Copyright (c) 2018 Robert Clausecker. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* genfsm.c -- generate a finite state machine */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "puzzle.h"
#include "compact.h"
#include "search.h"

/*
 * Determine a path leading to configuration p, the inverse last move of
 * which is last_move.  It is assumed that the path comprises len nodes,
 * including start and end node.  rounds is used to look up nodes along
 * the way with the first len entries of rounds being used.  Contrary to
 * the paths generated by search_ida(), this path also stores the initial node.
 */
static void
find_path(struct path *path, const struct puzzle *p, int last_move,
    const struct cp_slice *rounds, size_t len)
{
	struct puzzle pp = *p, p_hit;
	struct compact_puzzle cp, *hit;
	size_t i;
	int mask;

	path->pathlen = len;
	path->moves[len - 1] = zero_location(&pp);
	path->moves[len - 2] = last_move;
	move(&pp, last_move);

	for (i = 2; i < len; i++) {
		pack_puzzle(&cp, &pp);
		hit = bsearch(&cp, rounds[len - i].data, rounds[len - i].len,
		    sizeof *rounds->data, compare_cp_nomask);
		assert(hit != NULL);
		unpack_puzzle(&p_hit, hit);
		mask = move_mask(hit);
		assert(mask != 0);
		last_move = get_moves(zero_location(&pp))[ctz(mask)];
		path->moves[len - i -1] = last_move;
		move(&pp, last_move);
	}
}

/*
 * Search through expansion round len - 1 and print out all new half
 * loops to fsmfile.  We only print those loops spanning the entirety of
 * the search tree, i.e. where the two paths only join in the root node.
 * For each half loop, one branch is choosen as the canonical path.  The
 * other branches are pruned from the search tree.  For this reason,
 * the next round must be expanded before calling this function.  This
 * function assumes that the penultimate entry in rounds has already
 * been deduplicated.
 */
static void
do_loop(struct compact_puzzle *cp, FILE *fsmfile,
    struct cp_slice *rounds, size_t len)
{
	struct path paths[4];
	struct puzzle p;
	size_t i, j, n;
	int mask = move_mask(cp), short_cycles = 0, last_steps[4];
	const signed char *moves;
	char path0str[PATH_STR_LEN], pathstr[PATH_STR_LEN];

	/* is there more than one way to this configuration? */
	if (popcount(mask) <= 1)
		return;

	unpack_puzzle(&p, cp);
	moves = get_moves(zero_location(&p));

	assert(len >= 2); /* make sure every path has a last step */

	/* determine all possible paths */
	for (i = n = 0; i < 4; i++) {
		if (~mask & 1 << i)
			continue;

		find_path(paths + n, &p, moves[i], rounds, len);

		/* TODO: find correct offset */
		last_steps[n] = paths[n].moves[1];
		n++;
	}

	/*
	 * determine if any path has a short loop.  To do so, we merely
	 * need to check if the last moves are all distinct.  This is
	 * because two paths are distinct if and only if their last step
	 * (the one that moves to the solved configuration) is distinct.
	 */

	for (i = 0; i < n; i++)
		for (j = 0; j < i; j++)
			if (last_steps[i] == last_steps[j]) {
				short_cycles |= 1 << i;
				break;
			}

	/* print duplicate patterns and erase the paths from cp */
	path_string(path0str, paths + 0);

	for (i = j = 0; i < 4; i++) {
		if (~mask & 1 << i)
			continue;

		/* the first path is not a duplicate of itself */
		if (j == 0) {
			j++;
			continue;
		}

		if (~short_cycles & 1 << j) {
			/*
			 * entry of the form
			 *
			 * A,B,C = D,E,F
			 */
			path_string(pathstr, paths + j);
			fprintf(fsmfile, "%s = %s\n", pathstr, path0str);

			/*
			 * entry of the form
			 *
			 * D,E,F,C = A,B
			 */
			/* TODO */
		}

		/* erase path */
		mask &= ~(1 << i);

		j++;
	}

	/* write path back */
	cp->lo &= ~MOVE_MASK;
	cp->lo |= mask;
}

/*
 * Execute do_loop() for every half loop in rounds[len - 1].
 */
static void
do_loops(FILE *fsmfile, struct cp_slice *rounds, size_t len)
{
	struct compact_puzzle *cps = rounds[len - 1].data;
	size_t i, n_cps = rounds[len - 1].len;

	for (i = 0; i < n_cps; i++)
		do_loop(cps + i, fsmfile, rounds, len);
}



static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-l limit] [-s start_tile] [fsm]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *fsmfile;
	struct puzzle p;
	struct compact_puzzle cp;
	struct cp_slice cps[PDB_HISTOGRAM_LEN];
	int i, optchar, limit = PDB_HISTOGRAM_LEN, start_tile = 0;

	while (optchar = getopt(argc, argv, "l:s:"), optchar != -1)
		switch (optchar) {
		case 'l':
			limit = atoi(optarg);
			if (limit > PDB_HISTOGRAM_LEN)
				limit = PDB_HISTOGRAM_LEN;
			else if (limit < 0) {
				fprintf(stderr, "Limit must not be negative: %s\n", optarg);
				usage(argv[0]);
			}

			break;

		case 's':
			start_tile = atoi(optarg);
			if (start_tile < 0 || start_tile >= TILE_COUNT) {
				fprintf(stderr, "Start tile out of range, must be between 0 and 24: %s\n", optarg);
				usage(argv[0]);
			}

			break;

		default:
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		fsmfile = stdout;
		break;

	case 1:
		fsmfile = fopen(argv[optind], "w");
		if (fsmfile == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		break;

	default:
		usage(argv[0]);
		break;
	}

	p = solved_puzzle;
	move(&p, start_tile);
	pack_puzzle(&cp, &p);
	cps_init(cps + 0);
	cps_append(cps + 0, &cp);

	for (i = 1; i <= limit; i++) {
		fflush(stdout);

		cps_init(cps + i);
		cps_round(cps + i, cps + i - 1);
		do_loops(fsmfile, cps, i);
	}

	do_loops(fsmfile, cps, i);

	return (EXIT_SUCCESS);
}
