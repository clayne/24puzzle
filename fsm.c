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

/* fsm.c -- finite state machines */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"
#include "puzzle.h"

/*
 * fsm_dummy is a finite state machine which does not recognize any
 * patterns at all.
 */
static const unsigned dummy_states[1][4] = {
	FSM_BEGIN, FSM_BEGIN, FSM_BEGIN, FSM_BEGIN,
};

const struct fsm fsm_dummy = {
	.sizes = {
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
	},
#define DUMMY (unsigned (*)[4])dummy_states
	.tables = {
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
	},
#undef DUMMY
};

/*
 * fsm_simple is a finite state machine that just recognizes loops of
 * length 2, i.e. moves that directly undo the previous move.  To save
 * space, state arrays are reused where possible.
 */
static const unsigned simple_states_LU[5][4] = {
	             2,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* begin */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from above (dummy) */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from left (dummy) */
	     FSM_MATCH,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from right */
	             2,      FSM_MATCH, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from below */
};

static const unsigned simple_states_U[5][4] = {
	             3,              2,              1, FSM_UNASSIGNED, /* begin */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from above (dummy) */
	     FSM_MATCH,              2,              1, FSM_UNASSIGNED, /* from left */
	             3,      FSM_MATCH,              1, FSM_UNASSIGNED, /* from right */
	             3,              2,      FSM_MATCH, FSM_UNASSIGNED, /* from below */
};

static const unsigned simple_states_RU[5][4] = {
	             3,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* begin */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from above (dummy) */
	     FSM_MATCH,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from left */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from right (dummy) */
	             3,      FSM_MATCH, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from below */
};

/* same entries where not FSM_UNASSIGNED */
#define simple_states_LD simple_states_L
static const unsigned simple_states_L[5][4] = {
	             4,              2,              1, FSM_UNASSIGNED, /* begin */
	     FSM_MATCH,              2,              1, FSM_UNASSIGNED, /* from above */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from left (dummy) */
	             4,      FSM_MATCH,              1, FSM_UNASSIGNED, /* from right */
	             4,              2,      FSM_MATCH, FSM_UNASSIGNED, /* from below */
};

/* same entries where not FSM_UNASSIGNED */
#define simple_states_D simple_states_C
#define simple_states_RD simple_states_C
static const unsigned simple_states_C[5][4] = {
	             4,              3,              2,              1, /* begin */
	     FSM_MATCH,              3,              2,              1, /* from above */
	             4,      FSM_MATCH,              2,              1, /* from left */
	             4,              3,      FSM_MATCH,              1, /* from right */
	             4,              3,              2,      FSM_MATCH, /* from below */
};

static const unsigned simple_states_R[5][4] = {
	             4,              3,              1, FSM_UNASSIGNED, /* begin */
	     FSM_MATCH,              3,              1, FSM_UNASSIGNED, /* from above */
	             4,      FSM_MATCH,              1, FSM_UNASSIGNED, /* from left */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from right (dummy) */
	             4,              3,      FSM_MATCH, FSM_UNASSIGNED, /* from below */
};

const struct fsm fsm_simple = {
	.sizes = {
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
	},
#define STATES(xy) (unsigned (*)[4])simple_states_##xy
	.tables = {
		STATES(LU), STATES(U), STATES(U), STATES(U), STATES(RU),
		STATES(L), STATES(C), STATES(C), STATES(C), STATES(R),
		STATES(L), STATES(C), STATES(C), STATES(C), STATES(R),
		STATES(L), STATES(C), STATES(C), STATES(C), STATES(R),
		STATES(LD), STATES(D), STATES(D), STATES(D), STATES(RD),
	},
#undef STATES
#undef simple_states_D
#undef simple_states_RD
#undef simple_states_LD
};

/*
 * Load a finite state machine from file fsmfile.  On success, return a
 * pointer to the FSM loader.  On error, return NULL and set errno to
 * indicate the problem.
 */
extern struct fsm *
fsm_load(FILE *fsmfile)
{
	struct fsmfile header;
	struct fsm *fsm = malloc(sizeof *fsm);
	size_t i, count;
	int error;

	if (fsm == NULL)
		return (NULL);

	rewind(fsmfile);
	count = fread(&header, sizeof header, 1, fsmfile);
	if (count != 1) {
		error = errno;

		if (!ferror(fsmfile))
			errno = EINVAL;

		free(fsm);
		errno = error;
		return (NULL);
	}

	/* make it easier to bail out */
	for (i = 0; i < TILE_COUNT; i++)
		fsm->tables[i] = NULL;

	for (i = 0; i < TILE_COUNT; i++) {
		fsm->sizes[i] = header.lengths[i];
		fsm->tables[i] = malloc(fsm->sizes[i] * sizeof *fsm->tables[i]);
		if (fsm->tables[i] == NULL)
			goto fail;

		if (fseeko(fsmfile, header.offsets[i], SEEK_SET) != 0)
			goto fail;

		count = fread(fsm->tables[i], sizeof *fsm->tables[i], fsm->sizes[i], fsmfile);
		if (count != fsm->sizes[i]) {
			if (!ferror(fsmfile))
				errno = EINVAL;

			goto fail;
		}
	}

	return (fsm);

fail:
	error = errno;
	fsm_free(fsm);
	errno = error;

	return (NULL);
}