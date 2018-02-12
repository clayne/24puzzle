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

/* etatest.c -- compute eta using fixed samples */

/*
 * This program computes the heuristic quality factor eta by sampling
 * puzzle configurations.  The samples are stratified by the true
 * distance of the sample to the goal configuration.  Samples for the
 * first few equidistance classes as generated by puzzledist are taken
 * from a set of sample files, the remaining equidistance classes are
 * covered by random sample over the entire search space.
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parallel.h"
#include "puzzle.h"
#include "compact.h"
#include "catalogue.h"

/*
 * eqdist_sizes[d] is the fraction of 24 puzzle configurations which take
 * exactly d moves to solve
 */
static double eqdist_sizes[] = {
	1.289390056876894947e-25,
	2.578780113753789895e-25,
	5.157560227507579790e-25,
	1.289390056876894856e-24,
	3.352414147879926772e-24,
	8.252096364012127664e-24,
	2.050130190434262916e-23,
	4.719167608169434893e-23,
	1.111454229027883302e-22,
	2.454998668293607958e-22,
	5.851252078107348595e-22,
	1.320077540230564949e-21,
	3.107172159061941253e-21,
	6.857749956505453533e-21,
	1.591558616705995256e-20,
	3.460929215066686377e-20,
	7.947465069174391349e-20,
	1.710868457448927948e-19,
	3.895409824972265617e-19,
	8.302160801140543152e-19,
	1.872802696814086005e-18,
	3.949864119488325207e-18,
	8.834090030191725332e-18,
	1.845200930168931178e-17,
	4.091307990976822429e-17,
	8.460703635202500731e-17,
	1.859388625351011329e-16,
	3.805665208265298760e-16,
	8.287082334705840204e-16,
	1.678130909147960556e-15,
	3.619393759040597566e-15,
};

/* size of the eqdist_sizes array */
enum { EQDIST_SIZES_LEN = sizeof eqdist_sizes / sizeof eqdist_sizes[0] };


/*
 * Accumulate samples from sample file f into histogramm.  On IO error,
 * terminate the program.  Return the number of samples read from f.
 */
static size_t
do_samples(size_t histogram[PDB_HISTOGRAM_LEN], FILE *samplefile, struct pdb_catalogue *cat)
{
	struct puzzle p;
	struct compact_puzzle cp;
	size_t n_samples = 0;

	while (fread(&cp, sizeof cp, 1, samplefile) == 1) {
		n_samples++;
		unpack_puzzle(&p, &cp);
		histogram[catalogue_hval(cat, &p)]++;
	}

	if (ferror(samplefile)) {
		perror("do_samples");
		exit(EXIT_FAILURE);
	}

	return (n_samples);
}

/*
 * From a histogram for distance class d with n_samples samples, compute
 * the corresponding partial eta value and return it.  If f is not NULL,
 * print details about the samples to f.
 */
static double
partial_eta(size_t histogram[PDB_HISTOGRAM_LEN], size_t n_samples, int d, FILE *f)
{
	double eta = 0, invb = 1.0 / B;
	size_t i, end;

	for (i = 1; i <= PDB_HISTOGRAM_LEN; i++)
		eta = eta * invb + (double)histogram[PDB_HISTOGRAM_LEN - i];

	assert(0 <= d && d < EQDIST_SIZES_LEN);
	eta = (eta / (double)n_samples) * eqdist_sizes[d];

	if (f != NULL) {
		fprintf(f, "%3d: %13zu %e", d, n_samples, eta);

		end = 0;
		for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
			if (histogram[i] != 0)
				end = i + 1;

		for (i = 0; i < end; i++)
			fprintf(f, " %g", (double)histogram[i] / (double)n_samples);

		putc('\n', f);
	}

	return (eta);
}

/*
 * Generate a sample file name from prefix and d and open the
 * corresponding sample file.  Return the open file or NULL if the
 * file cannot be opened.  Do not print an error message but leave
 * errno in place.
 */
static FILE *
open_sample_file(const char *prefix, int d)
{
	char pathbuf[PATH_MAX];

	snprintf(pathbuf, PATH_MAX, "%s.%d", prefix, d);

	return (fopen(pathbuf, "rb"));
}

/*
 * Accumulate partial eta values from fixed sample files whose name
 * begins with prefix.  Return the value of eta computed this way.
 * Print extra information to f if f is not NULL.  If no sample file
 * whose name begins with prefix could be opened, print an error
 * message and terminate.
 */
static double
compute_eta(struct pdb_catalogue *cat, const char *prefix, FILE *f)
{
	FILE *samplefile;
	double eta = 0.0;
	size_t histogram[PDB_HISTOGRAM_LEN], n_samples;
	int d = 0;

	for (;;) {
		samplefile = open_sample_file(prefix, d);
		if (samplefile == 0)
			if (d == 0) {
				perror("open_sample_file");
				exit(EXIT_FAILURE);
			} else
				break;

		memset(histogram, 0, sizeof histogram);
		n_samples = do_samples(histogram, samplefile, cat);
		eta += partial_eta(histogram, n_samples, d, f);

		fclose(samplefile);
		d++;
	}

	return (eta);
}

static void
usage(const char *argv0)
{
	printf("Usage: %s [-iq] [-d pdbdir] -f prefix [-j nproc] catalogue\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct pdb_catalogue *cat;
	FILE *details = stdout, *messages = stderr;
	double eta;
	int optchar, catflags = 0;
	char *pdbdir = NULL, *prefix = NULL;

	while (optchar = getopt(argc, argv, "d:f:ij:q"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'f':
			prefix = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		case 'i':
			catflags |= CAT_IDENTIFY;
			break;

		case 'q':
			details = NULL;
			messages = NULL;
			break;

		default:
			usage(argv[0]);
		}

	if (prefix == NULL) {
		printf("Missing mandatory option -f prefix.\n");
		usage(argv[0]);
	}

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, catflags, messages);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	eta = compute_eta(cat, prefix, details);
	printf("eta = %e\n", eta);

	return (EXIT_SUCCESS);
}
