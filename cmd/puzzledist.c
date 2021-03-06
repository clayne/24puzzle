/*-
 * Copyright (c) 2018, 2020 Robert Clausecker. All rights reserved.
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

/* puzzledist.c -- compute the number of puzzles with the given distance */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>

#include "compact.h"
#include "puzzle.h"
#include "random.h"
#include "statistics.h"

/*
 * Use samplefile as a prefix for a file name of the form %s.%d suffixed
 * with round and store up to n_samples randomly selected samples from
 * cps in it.  The samples are stored as struct sample with the move bits
 * cleared and the probability set to the reciprocal of the sphere size.
 * On error, report the error, discard the sample file, and then continue.
 * The ordering of cps is destroyed in the process.  If sorted is set,
 * sort the randomly picked samples before writing them out.
 */
static void
do_sampling(const char *samplefile, struct cp_slice *cps, int round,
    size_t n_samples, int sorted)
{
	FILE *f;
	struct sample s;
	struct compact_puzzle tmp;
	size_t i, j, count;
	char pathbuf[PATH_MAX];

	snprintf(pathbuf, PATH_MAX, "%s%d.sample", samplefile, round);
	f = fopen(pathbuf, "wb");
	if (f == NULL) {
		perror(pathbuf);
		return;
	}

	/* can't take more samples than we have */
	if (n_samples >= cps->len)
		n_samples = cps->len;

	/* use an inside-out Fisher-Yates shuffle to pick a sample */
	for (i = 0; i < n_samples; i++) {
		j = i + random64() % (cps->len - i);
		tmp = cps->data[i];
		cps->data[i] = cps->data[j];
		cps->data[j] = tmp;
	}

	if (sorted)
		qsort(cps->data, n_samples, sizeof *cps->data, compare_cp);

	for (i = 0; i < n_samples; i++) {
		s.cp = cps->data[i];
		clear_move_mask(&s.cp);
		s.p = 1.0 / cps->len;
		count = fwrite(&s, sizeof s, 1, f);
		if (count != 1) {
			if (ferror(f))
				perror(pathbuf);
			else
				fprintf(stderr, "%s: end of file encountered while writing\n", pathbuf);

			fclose(f);
			remove(pathbuf);
			return;
		}
	}

	fclose(f);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-S] [-l limit] [-f prefix] [-n n_samples] [-s seed]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct cp_slice old_cps, new_cps;
	struct compact_puzzle cp;
	int optchar, i, limit = INT_MAX, sorted = 0;
	size_t n_samples = 1 << 20;
	const char *samplefile = NULL;

	while (optchar = getopt(argc, argv, "f:l:n:s:S"), optchar != -1)
		switch (optchar) {
		case 'f':
			samplefile = optarg;
			break;

		case 'l':
			limit = atoi(optarg);
			break;

		case 'n':
			n_samples = strtoull(optarg, NULL, 0);
			break;

		case 's':
			set_seed(strtoull(optarg, NULL, 0));
			break;

		case 'S':
			sorted = 1;
			break;

		default:
			usage(argv[0]);
			break;
		}

	if (argc != optind)
		usage(argv[0]);

	cps_init(&new_cps);
	pack_puzzle(&cp, &solved_puzzle);
	cps_append(&new_cps, &cp);

	if (samplefile != NULL)
		do_sampling(samplefile, &new_cps, 0, n_samples, sorted);

	/* keep format compatible with samplegen */
	printf("%s\n\n", CONFCOUNTSTR);

	printf("%3d: %18zu/%s = %24.18e\n", 0,
	    new_cps.len, CONFCOUNTSTR, new_cps.len / CONFCOUNT);

	for (i = 1; i <= limit; i++) {

		fflush(stdout);

		old_cps = new_cps;
		cps_init(&new_cps);

		cps_round(&new_cps, &old_cps);
		if (samplefile != NULL)
			do_sampling(samplefile, &new_cps, i, n_samples, sorted);

		cps_free(&old_cps);

		printf("%3d: %18zu/%s = %24.18e\n", i,
		    new_cps.len, CONFCOUNTSTR, new_cps.len / CONFCOUNT);
	}
}
