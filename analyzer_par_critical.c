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
 * analyzer_par_critical.c Versão paralela com #pragma omp critical
 * Estratégia:
 *   - Idêntica ao atomic em estrutura, mas o incremento é protegido por
 *     uma seção crítica global (#pragma omp critical).
 *   - Todas as threads disputam o MESMO lock implícito, o que provoca
 *     serialização forte — especialmente sob alta contenção (hotspots).
 */

#define MAX_LINE_LENGTH 1024
#define TABLE_SIZE      131071

clock_gettime(CLOCK_MONOTONIC, &t_start);

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

    
    // 3. Carrega log em memória
    printf("Lendo log em memória: %s...\n", log_filename);
    FILE *log_file = fopen(log_filename, "r");
    if (!log_file) {
        perror("Erro ao abrir arquivo de log");
        ht_destroy(ht);
        return 1;
    }

    size_t capacity = 1024 * 1024;
    size_t num_lines = 0;
    char **lines = malloc(capacity * sizeof(char *));
    if (!lines) {
        perror("Erro ao alocar vetor de linhas");
        fclose(log_file);
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
            ht_destroy(ht);
            return 1;
        }
        num_lines++;
    }
    fclose(log_file);
    printf("Total de linhas carregadas: %zu\n", num_lines);

    
    // 4. Processa em paralelo — critical (granularidade grossa)
    printf("Processando em paralelo (critical) com %d thread(s)...\n",
           omp_get_max_threads());

    struct timespec t_start, t_end;

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

        CacheNode *node = ht_get(ht, url_extraida);
        if (node != NULL) {

             // Seção crítica global, apenas UMA thread por vez pode entrar,independentemente de qual nó está sendo acessado.
            #pragma omp critical
            {
                node->hit_count++;
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    printf("Tempo de processamento: %.4f segundos\n", elapsed);

    
    // 5. Salva e libera
    
    for (size_t i = 0; i < num_lines; i++) free(lines[i]);
    free(lines);

    printf("Salvando results.csv...\n");
    ht_save_results(ht, "results.csv");
    ht_destroy(ht);

    printf("Concluído!\n");
    return 0;
}
