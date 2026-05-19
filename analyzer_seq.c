//Henrique Ferreira Marciano RA:10439797
//Pedro Casas Pequeno Junior RA:10437031
//Pedro Henrique Saraiva Arruda RA:10437747

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hash_table.h"

//execução sequencial: inicializa a tabela hash com o manifesto e processa os logs linearmente.
//contabiliza os acessos de cada URL e exporta o relatório final para results.csv.

#define MAX_LINE_LENGTH 1024

//define usando primo sugerido
#define TABLE_SIZE 131071

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_log>\n", argv[0]);
        return 1;
    }

    const char *log_filename = argv[1];
    char line[MAX_LINE_LENGTH];

    //inicializa a Tabela Hash 
    printf("Inicializando a Tabela Hash (size=%d)...\n", TABLE_SIZE);
    HashTable *ht = ht_create(TABLE_SIZE);
    if (!ht) {
        fprintf(stderr, "Erro ao criar a tabela hash.\n");
        return 1;
    }

    //le manifest.txt e popula a tabela
    printf("Carregando manifest.txt...\n");
    FILE *manifest = fopen("manifest.txt", "r");
    if (!manifest) {
        perror("Erro ao abrir manifest.txt");
        ht_destroy(ht);
        return 1;
    }

    while (fgets(line, sizeof(line), manifest)) {
        line[strcspn(line, "\n")] = '\0'; /* remove \n */
        if (line[0] != '\0')
            ht_put(ht, line);
    }
    fclose(manifest);

    //le o log e processa os acessos
    printf("Processando log: %s...\n", log_filename);
    FILE *log_file = fopen(log_filename, "r");
    if (!log_file) {
        perror("Erro ao abrir arquivo de log");
        ht_destroy(ht);
        return 1;
    }

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    while (fgets(line, sizeof(line), log_file)) {
        char url_extraida[256];

        //extrai URL: "GET <url> HTTP"
        char *inicio = strstr(line, "GET ");
        if (inicio == NULL) continue;
        inicio += 4; // pula "GET "

        char *fim = strstr(inicio, " HTTP");
        if (fim == NULL) continue;

        size_t tamanho = (size_t)(fim - inicio);
        if (tamanho == 0 || tamanho >= sizeof(url_extraida))
            continue;

        memcpy(url_extraida, inicio, tamanho);
        url_extraida[tamanho] = '\0';

        //busca e incrementa
        CacheNode *node = ht_get(ht, url_extraida);
        if (node != NULL)
            node->hit_count++;
    }
    fclose(log_file);

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    printf("Tempo de processamento: %.4f segundos\n", elapsed);

    //salva resultados e libera memória
    printf("Salvando results.csv...\n");
    ht_save_results(ht, "results.csv");
    ht_destroy(ht);

    printf("Concluído!\n");
    return 0;
}
