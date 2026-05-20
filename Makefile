#Henrique Ferreira Marciano RA:10439797
#Pedro Casas Pequeno Junior RA:10437031
#Pedro Henrique Saraiva Arruda RA:10437747

CC     = gcc
CFLAGS = -O2 -fopenmp -Wall -Wextra

HT_OBJ = hash_table.c

TARGETS = analyzer_seq \
          analyzer_par_atomic \
          analyzer_par_critical \
          analyzer_par_lock \
          analyzer_par_atomic_padded

.PHONY: all clean

# compila tudo
all: $(TARGETS)

# sequencial
analyzer_seq: analyzer_seq.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_seq.c $(HT_OBJ) -o analyzer_seq

# atomic
analyzer_par_atomic: analyzer_par_atomic.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_par_atomic.c $(HT_OBJ) -o analyzer_par_atomic

# critical
analyzer_par_critical: analyzer_par_critical.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_par_critical.c $(HT_OBJ) -o analyzer_par_critical

# lock
analyzer_par_lock: analyzer_par_lock.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_par_lock.c $(HT_OBJ) -o analyzer_par_lock

# padded
analyzer_par_atomic_padded: analyzer_par_atomic_padded.c
	$(CC) $(CFLAGS) analyzer_par_atomic_padded.c -o analyzer_par_atomic_padded

# clean
clean:
	rm -f $(TARGETS)