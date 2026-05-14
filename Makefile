# =============================================================================
# Makefile — Analisador de Cache de CDN com OpenMP
# Computação Paralela — Prof. Dr. Jean M. Laine
# =============================================================================

CC     = gcc
CFLAGS = -O2 -fopenmp -Wall -Wextra

# Objeto compartilhado da tabela hash
HT_OBJ = hash_table.c

TARGETS = analyzer_seq \
          analyzer_par_atomic \
          analyzer_par_critical \
          analyzer_par_lock \
          analyzer_par_atomic_padded

.PHONY: all clean

# ------------------------------------------------------------------------------
# Alvo padrão: compila tudo
# ------------------------------------------------------------------------------
all: $(TARGETS)

# ------------------------------------------------------------------------------
# Sequencial (baseline)
# ------------------------------------------------------------------------------
analyzer_seq: analyzer_seq.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_seq.c $(HT_OBJ) -o analyzer_seq

# ------------------------------------------------------------------------------
# Paralelo — atomic (granularidade fina)
# ------------------------------------------------------------------------------
analyzer_par_atomic: analyzer_par_atomic.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_par_atomic.c $(HT_OBJ) -o analyzer_par_atomic

# ------------------------------------------------------------------------------
# Paralelo — critical (granularidade grossa)
# ------------------------------------------------------------------------------
analyzer_par_critical: analyzer_par_critical.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_par_critical.c $(HT_OBJ) -o analyzer_par_critical

# ------------------------------------------------------------------------------
# Paralelo — bucket lock (granularidade média)
# ------------------------------------------------------------------------------
analyzer_par_lock: analyzer_par_lock.c $(HT_OBJ)
	$(CC) $(CFLAGS) analyzer_par_lock.c $(HT_OBJ) -o analyzer_par_lock

# ------------------------------------------------------------------------------
# Paralelo — atomic + padding (experimento C: false sharing)
# NÃO linka com hash_table.c — reimplementa CacheNode com padding internamente
# ------------------------------------------------------------------------------
analyzer_par_atomic_padded: analyzer_par_atomic_padded.c
	$(CC) $(CFLAGS) analyzer_par_atomic_padded.c -o analyzer_par_atomic_padded

# ------------------------------------------------------------------------------
# Limpeza
# ------------------------------------------------------------------------------
clean:
	rm -f $(TARGETS)

# ------------------------------------------------------------------------------
# Ajuda rápida
# ------------------------------------------------------------------------------
help:
	@echo "Alvos disponíveis:"
	@echo "  make                        — compila todos os executáveis"
	@echo "  make clean                  — remove executáveis"
	@echo ""
	@echo "Executáveis gerados:"
	@echo "  ./analyzer_seq                    <log>"
	@echo "  ./analyzer_par_atomic             <log>"
	@echo "  ./analyzer_par_critical           <log>"
	@echo "  ./analyzer_par_lock               <log>"
	@echo "  ./analyzer_par_atomic_padded      <log>"
	@echo ""
	@echo "Exemplos de uso:"
	@echo "  export OMP_NUM_THREADS=8"
	@echo "  time ./analyzer_par_atomic log_distribuido.txt"
	@echo ""
	@echo "Validação de corretude:"
	@echo "  sort results.csv > sorted_res.csv"
	@echo "  sort gabarito_distribuido.csv > sorted_gab.csv"
	@echo "  diff -s sorted_res.csv sorted_gab.csv"
	@echo ""
	@echo "Profiling (perf):"
	@echo "  perf stat -e instructions,cycles ./analyzer_par_atomic log_concorrente.txt"
	@echo ""
	@echo "Profiling (cachegrind):"
	@echo "  valgrind --tool=cachegrind ./analyzer_par_atomic_padded log_concorrente.txt"
