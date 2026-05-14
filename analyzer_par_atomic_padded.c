//Henrique Ferreira Marciano RA:10439797
//Pedro Casas Pequeno Junior RA:10437031
//Pedro Henrique Saraiva Arruda RA:10437747

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

/*
 * analyzer_par_atomic_padded.c
 * Replica da versão atomic, mas utiliza uma estrutura CacheNode
 * com padding para eliminar o efeito de false sharing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

#define MAX_LINE_LENGTH 1024
#define TABLE_SIZE      131071


// CacheNode com padding 
typedef struct CacheNode {
    char              *url;
    long               hit_count;
    struct CacheNode  *next;
    long               padding[5]; // alinha cada nó em 64 bytes
} CacheNode;

typedef struct {
    size_t      size;
    CacheNode **table;
} HashTable;

//Funções auxiliares

static size_t hash_djb2(const char *str, size_t size) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash % size;
}

static HashTable *ht_create(size_t size) {
    HashTable *ht = malloc(sizeof(HashTable));
    if (!ht) { perror("ht_create"); exit(EXIT_FAILURE); }
    ht->table = calloc(size, sizeof(CacheNode *));
    if (!ht->table) { perror("ht_create buckets"); exit(EXIT_FAILURE); }
    ht->size = size;
    return ht;
}

static void ht_put(HashTable *ht, const char *url) {
    size_t idx = hash_djb2(url, ht->size);
    CacheNode *cur = ht->table[idx];
    while (cur) {
        if (strcmp(cur->url, url) == 0) return;
        cur = cur->next;
    }
    CacheNode *node = malloc(sizeof(CacheNode));
    if (!node) { perror("ht_put malloc"); exit(EXIT_FAILURE); }
    node->url = strdup(url);
    node->hit_count = 0;
    node->next = ht->table[idx];
    ht->table[idx] = node;
}

static CacheNode *ht_get(HashTable *ht, const char *url) {
    size_t idx = hash_djb2(url, ht->size);
    CacheNode *cur = ht->table[idx];
    while (cur) {
        if (strcmp(cur->url, url) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

static void ht_save_results(HashTable *ht, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { perror("ht_save_results"); return; }
    for (size_t i = 0; i < ht->size; i++) {
        CacheNode *cur = ht->table[i];
        while (cur) {
            fprintf(fp, "%s,%ld\n", cur->url, cur->hit_count);
            cur = cur->next;
        }
    }
    fclose(fp);
}

static void ht_destroy(HashTable *ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->size; i++) {
        CacheNode *cur = ht->table[i];
        while (cur) {
            CacheNode *nxt = cur->next;
            free(cur->url);
            free(cur);
            cur = nxt;
        }
    }
    free(ht->table);
    free(ht);
}

// main
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        return 1;
    }

    const char *log_filename = argv[1];
    char line[MAX_LINE_LENGTH];

    printf("=== analyzer_par_atomic_padded (Experimento C: False Sharing) ===\n");
    printf("sizeof(CacheNode) = %zu bytes\n", sizeof(CacheNode));

    // 1. Tabela Hash
    printf("Inicializando a Tabela Hash (size=%d)...\n", TABLE_SIZE);
    HashTable *ht = ht_create(TABLE_SIZE);

    // 2. Manifest
    printf("Carregando manifest.txt...\n");
    FILE *manifest = fopen("manifest.txt", "r");
    if (!manifest) { perror("manifest.txt"); ht_destroy(ht); return 1; }
    while (fgets(line, sizeof(line), manifest)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != '\0') ht_put(ht, line);
    }
    fclose(manifest);

    // 3. Carrega log em memória
    printf("Lendo log em memória: %s...\n", log_filename);
    FILE *log_file = fopen(log_filename, "r");
    if (!log_file) { perror("log"); ht_destroy(ht); return 1; }

    size_t capacity = 1024 * 1024, num_lines = 0;
    char **lines = malloc(capacity * sizeof(char *));
    if (!lines) { perror("malloc lines"); fclose(log_file); ht_destroy(ht); return 1; }

    while (fgets(line, sizeof(line), log_file)) {
        if (num_lines == capacity) {
            capacity *= 2;
            char **tmp = realloc(lines, capacity * sizeof(char *));
            if (!tmp) { perror("realloc"); break; }
            lines = tmp;
        }
        lines[num_lines++] = strdup(line);
    }
    fclose(log_file);
    printf("Total de linhas carregadas: %zu\n", num_lines);

    // 4. Processa em paralelo
    printf("Processando em paralelo (atomic + padded) com %d thread(s)...\n",
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

        CacheNode *node = ht_get(ht, url_extraida);
        if (node != NULL) {
            #pragma omp atomic update
            node->hit_count++;
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
