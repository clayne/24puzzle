/* catalogue.h -- pattern database catalogues */
#ifndef CATALOGUE_H
#define CATALOGUE_H

#include <stdio.h>

#include "builtins.h"
#include "pdb.h"
#include "tileset.h"
#include "puzzle.h"

/*
 * A struct pdb_catalogue stores a catalogue of pattern databases.
 * Groups of pattern databases are used to form distance heuristics, a
 * PDB catalogue stores multiple such groups and can compute the maximal
 * h value of all groups.  The member pdbs contains the pattern
 * databases we are interested in.  The member parts contains a bitmap
 * of which PDBs make up which heuristic.  The member heuristics
 * contains a bitmap of which heuristics each PDB is used for.  The
 * member pdbs_ts contains for the PDB's tile sets for better cache
 * locality.
 */
enum {
	CATALOGUE_PDBS_LEN = 64,
	HEURISTICS_LEN = 32,

	/* flags for catalogue_load() */
	CAT_IDENTIFY = 1 << 0,
};

struct pdb_catalogue {
	struct patterndb *pdbs[CATALOGUE_PDBS_LEN];
	tileset pdbs_ts[CATALOGUE_PDBS_LEN];
	unsigned long long parts[HEURISTICS_LEN];
	size_t n_pdbs, n_heuristics;
};

/*
 * A struct partial_hvals stores the partial h values of a puzzle
 * configuration for the PDBs in a PDB catalogue.  This is useful so we
 * can avoid superfluous PDB lookups by not looking up values that did
 * not change change whenever we can.  The member fake_entries stores a
 * bitmap of those PDB whose entries we have not bothered to look up as
 * they do not contribute to the best heuristic for this puzzle
 * configuration.
 */
struct partial_hvals {
	unsigned char hvals[CATALOGUE_PDBS_LEN];
};

extern struct pdb_catalogue	*catalogue_load(const char *, const char *, int, FILE *);
extern void	catalogue_free(struct pdb_catalogue *);
extern unsigned	catalogue_partial_hvals(struct partial_hvals *, struct pdb_catalogue *, const struct puzzle *);
extern unsigned	catalogue_diff_hvals(struct partial_hvals *, struct pdb_catalogue *, const struct puzzle *, unsigned);

/*
 * This convenience function call catalogue_partial_hvals() on a
 * throw-aray struct partial_hvals and just returns the result the
 * h value predicted for p by cat.
 */
static inline unsigned
catalogue_hval(struct pdb_catalogue *cat, const struct puzzle *p)
{
	struct partial_hvals ph;

	return (catalogue_partial_hvals(&ph, cat, p));
}

/*
 * Given a struct partial_hvals, return the h value indicated
 * by this structure.  This is the maximum of all heuristics it
 * contains.
 */
static inline unsigned
catalogue_ph_hval(struct pdb_catalogue *cat, const struct partial_hvals *ph)
{
	size_t i;
	unsigned long long parts;
	unsigned max = 0, sum;

	for (i = 0; i < cat->n_heuristics; i++) {
		sum = 0;
		for (parts = cat->parts[i]; parts != 0; parts &= parts - 1)
			sum += ph->hvals[ctzll(parts)];

		if (sum > max)
			max = sum;
	}

	return (max);
}

/*
 * Given a struct partial_hvals, return a bitmap indicating the
 * heuristics whose h value is equal to the maximum h value for
 * ph.
 */
static inline unsigned
catalogue_max_heuristics(struct pdb_catalogue *cat, const struct partial_hvals *ph)
{
	size_t i;
	unsigned long long parts;
	unsigned max = 0, sum, heumap = 0;

	for (i = 0; i < cat->n_heuristics; i++) {
		sum = 0;
		for (parts = cat->parts[i]; parts != 0; parts &= parts - 1)
			sum += ph->hvals[ctzll(parts)];

		if (sum > max) {
			max = sum;
			heumap = 0;
		}

		if (sum == max)
			heumap |= 1 << i;
	}

	return (heumap);
}

#endif /* CATALOGUE_H */
