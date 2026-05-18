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
 * analyzer_par_atomic.c Versão paralela com #pragma omp atomic
 *
 * Estratégia:
 *   - A tabela hash é construída sequencialmente a partir do manifest.txt.
 *   - O arquivo de log é lido inteiramente para um vetor de linhas em memória.
 *   - O processamento do vetor é paralelizado com #pragma omp parallel for.
 *   - O incremento de hit_count é protegido por #pragma omp atomic update,
 */

#define MAX_LINE_LENGTH 1024
#define TABLE_SIZE      131071  //primo sugerido

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        return 1;
    }

    const char *log_filename = argv[1];
    char line[MAX_LINE_LENGTH];

    // 1. Inicializa a Tabela Hash
    printf("Inicializando a Tabela Hash (size=%d)...\n", TABLE_SIZE);
    HashTable *ht = ht_create(TABLE_SIZE);
    if (!ht) {
        fprintf(stderr, "Erro ao criar a tabela hash.\n");
        return 1;
    }

    // 2. Carrega manifest.txt
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

    // 3. Carrega todas as linhas do log em memória, vetor de ponteiros
    printf("Lendo log em memória: %s...\n", log_filename);
    FILE *log_file = fopen(log_filename, "r");
    if (!log_file) {
        perror("Erro ao abrir arquivo de log");
        ht_destroy(ht);
        return 1;
    }

    // Alocamento dinamico
    size_t capacity = 1024 * 1024; // começa com 1 M de slots
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
                // lbera o que já foi alocado
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

    // 4. Processa as linhas em paralelo atomic update 
    printf("Processando em paralelo (atomic) com %d thread(s)...\n",
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

        // uso do ht_get() para leitura concorrente 
        CacheNode *node = ht_get(ht, url_extraida);
        if (node != NULL) {

            //Garante que o incremento seja uma operação indivisível, threads que acessam nós DIFERENTES não se bloqueiam.
            #pragma omp atomic update
            node->hit_count++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    printf("Tempo de processamento: %.4f segundos\n", elapsed);

    // 5. Libera vetor de linhas, salva resultados
    for (size_t i = 0; i < num_lines; i++) free(lines[i]);
    free(lines);

    printf("Salvando results.csv...\n");
    ht_save_results(ht, "results.csv");
    ht_destroy(ht);

    printf("Concluído!\n");
    return 0;
}
