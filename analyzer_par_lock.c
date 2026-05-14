//Henrique Ferreira Marciano RA:10439797
//Pedro Casas Pequeno Junior RA:10437031
//Pedro Henrique Saraiva Arruda RA:10437747

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "hash_table.h"

/*
 * analyzer_par_lock.c Versão paralela com Bucket Locking
 *
 * Estratégia:
 *   - Um array de omp_lock_t com exatamente TABLE_SIZE posições é criado,
 *     um lock por bucket da tabela hash.
 *   - Ao atualizar hit_count, a thread:
 *       1. Calcula o índice do bucket (mesma função hash usada pela tabela)
 *       2. Adquire omp_lock_t[bucket]
 *       3. Atualiza o contador
 *       4. Libera o lock
 *   - Threads que acessam URLs mapeadas para buckets DIFERENTES executam
 *     em paralelo sem nenhuma contenção entre si.
 */

#define MAX_LINE_LENGTH 1024
#define TABLE_SIZE      131071

// Replica da função hash interna do hash_table.c 
// Necessária para calcular o índice do bucket externamente.

static size_t hash_djb2_local(const char *str, size_t size) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % size;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        return 1;
    }

    const char *log_filename = argv[1];
    char line[MAX_LINE_LENGTH];

    // 1. Tabela Hash
    printf("Inicializando a Tabela Hash (size=%d)...\n", TABLE_SIZE);
    HashTable *ht = ht_create(TABLE_SIZE);
    if (!ht) {
        fprintf(stderr, "Erro ao criar a tabela hash.\n");
        return 1;
    }

    // 2. Manifest
    printf("Carregando manifest.txt...\n");
    FILE *manifest = fopen("manifest.txt", "r");
    if (!manifest) {
        perror("Erro ao abrir manifest.txt");
        ht_destroy(ht);
        return 1;
    }
    while (fgets(line, sizeof(line), manifest)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != '\0')
            ht_put(ht, line);
    }
    fclose(manifest);

    // 3. Inicializa o array de locks (um por bucket)
    printf("Inicializando %d bucket locks...\n", TABLE_SIZE);
    omp_lock_t *locks = malloc(sizeof(omp_lock_t) * TABLE_SIZE);
    if (!locks) {
        perror("Erro ao alocar array de locks");
        ht_destroy(ht);
        return 1;
    }
    for (int i = 0; i < TABLE_SIZE; i++)
        omp_init_lock(&locks[i]);

    // 4. Carrega log em memória
    printf("Lendo log em memória: %s...\n", log_filename);
    FILE *log_file = fopen(log_filename, "r");
    if (!log_file) {
        perror("Erro ao abrir arquivo de log");
        for (int i = 0; i < TABLE_SIZE; i++) omp_destroy_lock(&locks[i]);
        free(locks);
        ht_destroy(ht);
        return 1;
    }

    size_t capacity = 1024 * 1024;
    size_t num_lines = 0;
    char **lines = malloc(capacity * sizeof(char *));
    if (!lines) {
        perror("Erro ao alocar vetor de linhas");
        fclose(log_file);
        for (int i = 0; i < TABLE_SIZE; i++) omp_destroy_lock(&locks[i]);
        free(locks);
        ht_destroy(ht);
        return 1;
    }

    while (fgets(line, sizeof(line), log_file)) {
        if (num_lines == capacity) {
            capacity *= 2;
            char **tmp = realloc(lines, capacity * sizeof(char *));
            if (!tmp) {
                perror("Erro ao realocar vetor de linhas");
                fclose(log_file);
                for (size_t i = 0; i < num_lines; i++) free(lines[i]);
                free(lines);
                for (int i = 0; i < TABLE_SIZE; i++) omp_destroy_lock(&locks[i]);
                free(locks);
                ht_destroy(ht);
                return 1;
            }
            lines = tmp;
        }
        lines[num_lines] = strdup(line);
        if (!lines[num_lines]) {
            perror("Erro ao duplicar linha");
            fclose(log_file);
            for (size_t i = 0; i < num_lines; i++) free(lines[i]);
            free(lines);
            for (int i = 0; i < TABLE_SIZE; i++) omp_destroy_lock(&locks[i]);
            free(locks);
            ht_destroy(ht);
            return 1;
        }
        num_lines++;
    }
    fclose(log_file);
    printf("Total de linhas carregadas: %zu\n", num_lines);

    // 5. Processa em paralelo — bucket locking
    printf("Processando em paralelo (bucket lock) com %d thread(s)...\n",
           omp_get_max_threads());

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    #pragma omp parallel for schedule(dynamic, 4096) private(line)
    for (size_t i = 0; i < num_lines; i++) {
        char url_extraida[256];

        char *inicio = strstr(lines[i], "GET ");
        if (inicio == NULL) continue;
        inicio += 4;

        char *fim = strstr(inicio, " HTTP");
        if (fim == NULL) continue;

        size_t tamanho = (size_t)(fim - inicio);
        if (tamanho == 0 || tamanho >= sizeof(url_extraida)) continue;

        memcpy(url_extraida, inicio, tamanho);
        url_extraida[tamanho] = '\0';

        /*
         * Fluxo do bucket locking
         *   1. Calcula o bucket da URL
         *   2. Adquire o lock daquele bucket
         *   3. Busca o nó e incrementa hit_count
         *   4. Libera o lock
         * Threads em buckets diferentes NÃO se bloqueiam mutuamente.
         */
        size_t bucket = hash_djb2_local(url_extraida, TABLE_SIZE);

        omp_set_lock(&locks[bucket]);
        CacheNode *node = ht_get(ht, url_extraida);
        if (node != NULL)
            node->hit_count++;
        omp_unset_lock(&locks[bucket]);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    printf("Tempo de processamento: %.4f segundos\n", elapsed);

    // 6. Destrói locks, salva e libera
    for (int i = 0; i < TABLE_SIZE; i++)
        omp_destroy_lock(&locks[i]);
    free(locks);

    for (size_t i = 0; i < num_lines; i++) free(lines[i]);
    free(lines);

    printf("Salvando results.csv...\n");
    ht_save_results(ht, "results.csv");
    ht_destroy(ht);

    printf("Concluído!\n");
    return 0;
}
