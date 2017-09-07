CC=clang
CFLAGS=-march=native -O3 -g
COPTS=-std=c11 -I. -Wall -Wno-missing-braces -Wno-parentheses
LDLIBS=-lpthread

OBJ=index.o puzzle.o tileset.o validation.o ranktbl.o rank.o random.o pdb.o \
	moves.o parallel.o pdbgen.o pdbverify.o pdbdiff.o histogram.o \
	cindex.o pdbdom.o pdbident.o ida.o search.o
BINARIES=cmd/pdbstats test/indextest util/rankgen test/ranktest \
	cmd/genpdb cmd/verifypdb cmd/diffcode cmd/reducepdb test/qualitytest

all: $(BINARIES) 24puzzle.a

.o:
	@echo "CCLD	$@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

24puzzle.a: $(OBJ)
	@echo "AR	$@"
	@ar -src $@ $?

util/rankgen: util/rankgen.o
ranktbl.c: util/rankgen
	@echo "RANKGEN	$@"
	@util/rankgen >$@

test/indextest: test/indextest.o 24puzzle.a
test/tiletest: test/tiletest.o 24puzzle.a
test/ranktest: test/ranktest.o 24puzzle.a
test/qualitytest: test/qualitytest.o 24puzzle.a
cmd/genpdb: cmd/genpdb.o 24puzzle.a
cmd/verifypdb: cmd/verifypdb.o 24puzzle.a
cmd/reducepdb: cmd/reducepdb.o 24puzzle.a
cmd/diffcode: cmd/diffcode.o 24puzzle.a
cmd/pdbstats: cmd/pdbstats.o 24puzzle.a
	@echo "CCLD	$@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) -lm

.c.o:
	@echo "CC	$<"
	@$(CC) $(COPTS) $(CFLAGS) -c -o $@ $<

clean:
	@echo "CLEAN"
	@rm -f *.a *.o test/*.o cmd/*.o util/*.o ranktbl.c $(BINARIES)

.PHONY: all clean
