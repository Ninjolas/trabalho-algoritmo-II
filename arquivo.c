#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // Para offsetof
#include <stdint.h> // Para int64_t
#include <limits.h> // Para LLONG_MIN e INT_MIN
#include <ctype.h>  // Para isdigit() na validação de data

// --- DEFINIÇÕES ---
const char* ARQ_CSV = "jewelry.csv";
const char* ARQ_PRODUTOS_BIN = "produtos.bin";
const char* ARQ_PRODUTOS_IDX = "produtos_idx.bin";
const char* ARQ_COMPRAS_BIN = "compras.bin";
const char* ARQ_COMPRAS_IDX = "compras_idx.bin";

#define TAM_BRAND 50
#define TAM_CATEGORY 100
#define TAM_DATETIME 30
#define BLOCO_INDICE 100

// --- ESTRUTURAS ---
typedef struct {
    int product_id;
    char brand[TAM_BRAND];
    double price;
    char category_alias[TAM_CATEGORY];
    char ativo;
    char newline;
} Produto;

typedef struct {
    int chave;
    long offset;
} IndiceProduto;

typedef struct {
    long long order_id;
    long long product_id;
    long long user_id;
    char order_datetime[TAM_DATETIME];
    int quantity;
    char ativo;
    char newline;
} Compra;

typedef struct {
    long long chave;
    long offset;
} IndiceCompra;

// --- FUNÇÕES AUXILIARES COMUNS ---
void pad_string(char *str, int tam) {
    int len = (int)strlen(str);
    if (len >= tam) {
        str[tam - 1] = '\0';
    } else {
        memset(str + len, ' ', tam - len - 1);
        str[tam - 1] = '\0';
    }
}

int ler_inteiro(const char* prompt) {
    int valor;
    printf("%s", prompt);
    while (scanf("%d", &valor) != 1) {
        while (getchar() != '\n');
        printf("Entrada invalida. %s", prompt);
    }
    while (getchar() != '\n');
    return valor;
}

long long ler_long_long(const char* prompt) {
    long long valor;
    printf("%s", prompt);
    while (scanf("%lld", &valor) != 1) {
        while (getchar() != '\n');
        printf("Entrada invalida. %s", prompt);
    }
    while (getchar() != '\n');
    return valor;
}

double ler_double(const char* prompt) {
    double valor;
    printf("%s", prompt);
    while (scanf("%lf", &valor) != 1) {
        while (getchar() != '\n');
        printf("Entrada invalida. %s", prompt);
    }
    while (getchar() != '\n');
    return valor;
}

void ler_string(const char* prompt, char* buffer, int tamanho) {
    printf("%s", prompt);
    fgets(buffer, tamanho, stdin);
    buffer[strcspn(buffer, "\n")] = 0;
}

/**
 * Valida o formato básico da data/hora (YYYY-MM-DD HH:MM:SS)
 * e adiciona " UTC" no final se o formato estiver correto e houver espaço.
 * Retorna 1 se o formato for válido, 0 caso contrário.
 */
int validar_e_formatar_data(char* buffer, int tamanho_buffer) {
    int ano, mes, dia, hora, min, seg;
    char espaco; // Para capturar o espaço entre data e hora

    // 1. Tenta "escanear" a string no formato esperado
    if (sscanf(buffer, "%d-%d-%d%c%d:%d:%d",
               &ano, &mes, &dia, &espaco, &hora, &min, &seg) == 7 && espaco == ' ')
    {
        // 2. Verificação básica dos ranges
        if (ano >= 0 && mes >= 1 && mes <= 12 && dia >= 1 && dia <= 31 &&
            hora >= 0 && hora <= 23 && min >= 0 && min <= 59 && seg >= 0 && seg <= 59)
        {
            // 3. Verifica se a string tem EXATAMENTE 19 caracteres
            if (strlen(buffer) == 19) {
                 // 4. Verifica espaço para adicionar " UTC" (4 chars + \0 = 5)
                 if (19 + 4 < tamanho_buffer) {
                     strcat(buffer, " UTC"); // Adiciona " UTC"
                     return 1; // Válido e UTC adicionado
                 } else {
                     printf("AVISO: Buffer pequeno demais para adicionar UTC.\n");
                     return 1; // Válido, mas sem UTC
                 }
            }
        }
    }
    // Formato ou ranges inválidos
    return 0;
}


// --- FUNÇÕES DE COMPARAÇÃO ---
int comparar_produto(const void* a, const void* b) {
    int id_a = ((Produto*)a)->product_id;
    int id_b = ((Produto*)b)->product_id;
    return (id_a > id_b) - (id_a < id_b);
}

int comparar_produto_chave(const void* a, const void* b) {
    int id_a = ((Produto*)a)->product_id;
    int id_b = *(int*)b;
    return (id_a > id_b) - (id_a < id_b);
}

int comparar_compra(const void* a, const void* b) {
    long long id_a = ((Compra*)a)->order_id;
    long long id_b = ((Compra*)b)->order_id;
    return (id_a > id_b) - (id_a < id_b);
}

int comparar_compra_chave(const void* a, const void* b) {
    long long id_a = ((Compra*)a)->order_id;
    long long id_b = *(long long*)b;
    return (id_a > id_b) - (id_a < id_b);
}

// --- FUNÇÕES DE EXTRAÇÃO DE CHAVE ---
int extrai_chave_produto(const void* reg) {
    return ((Produto*)reg)->product_id;
}

long long extrai_chave_compra(const void* reg) {
    return ((Compra*)reg)->order_id;
}

// --- FUNÇÕES GENÉRICAS PARA ARQUIVOS ---

void criar_indice(const char *arq_dados, const char *arq_indice,
                  size_t tam_registro, size_t tam_indice,
                  int (*extrai_chave)(const void*),
                  long long (*extrai_chave_ll)(const void*),
                  int is_long_long,
                  size_t offset_ativo) {
    FILE *f_dados = fopen(arq_dados, "rb");
    FILE *f_indice = fopen(arq_indice, "wb");
    if (!f_dados || !f_indice) { /* ... (erro) ... */ return; }

    void *registro = malloc(tam_registro);
    if (!registro) { /* ... (erro) ... */ fclose(f_dados); fclose(f_indice); return; }
    long offset = 0;
    int contador_registros = 0;

    while (fread(registro, tam_registro, 1, f_dados) == 1) {
        if (((char*)registro)[offset_ativo] == 'S') {
            if (contador_registros % BLOCO_INDICE == 0) {
                if (is_long_long) {
                    IndiceCompra idx = {extrai_chave_ll(registro), offset};
                    fwrite(&idx, tam_indice, 1, f_indice);
                } else {
                    IndiceProduto idx = {extrai_chave(registro), offset};
                    fwrite(&idx, tam_indice, 1, f_indice);
                }
            }
            contador_registros++;
        }
        offset = ftell(f_dados);
    }

    free(registro);
    fclose(f_dados);
    fclose(f_indice);
    printf("Indice criado com %d entradas.\n", (contador_registros + BLOCO_INDICE - 1) / BLOCO_INDICE);
}

long pesquisa_binaria(const char *arq_bin, size_t tam_registro,
                      int (*comparador)(const void*, const void*),
                      const void *chave_busca,
                      size_t offset_ativo) {
    FILE *fbin = fopen(arq_bin, "rb");
    if (!fbin) return -1;

    fseek(fbin, 0, SEEK_END);
    long tamanho_arquivo = ftell(fbin);
    if (tamanho_arquivo <= 0 || tamanho_arquivo % tam_registro != 0) { fclose(fbin); return -1; }
    long num_registros = tamanho_arquivo / tam_registro;

    long inicio = 0, fim = num_registros - 1;
    void *registro = malloc(tam_registro);
    if (!registro) { fclose(fbin); return -1; }

    while (inicio <= fim) {
        long meio = inicio + (fim - inicio) / 2;
        if (fseek(fbin, meio * tam_registro, SEEK_SET) != 0) { free(registro); fclose(fbin); return -1; }

        if (fread(registro, tam_registro, 1, fbin) != 1) { free(registro); fclose(fbin); return -1; }

        int cmp = comparador(registro, chave_busca);
        if (cmp == 0) {
            char ativo = ((char*)registro)[offset_ativo];
            long result = (ativo == 'S') ? meio * tam_registro : -2;
            free(registro);
            fclose(fbin);
            return result;
        }
        (cmp < 0) ? (inicio = meio + 1) : (fim = meio - 1);
    }

    free(registro);
    fclose(fbin);
    return -1;
}

// --- FUNÇÕES ESPECÍFICAS PRODUTOS ---
void pre_processar_produtos(const char *csv_path, const char *bin_path) {
    // (Código original OK, com correção para IDs negativos/zero)
    printf("Pre-processando PRODUTOS de %s...\n", csv_path);
    FILE *fcsv = fopen(csv_path, "r");
    if (!fcsv) { printf("ERRO: Nao foi possivel abrir CSV %s\n", csv_path); return; }

    char linha[2048];
    if (!fgets(linha, sizeof(linha), fcsv)) { fclose(fcsv); return; } // Ignora cabeçalho

    int capacidade = 100000, n_produtos = 0;
    Produto* produtos = malloc(capacidade * sizeof(Produto));
    if (!produtos) { printf("ERRO: Falha ao alocar memoria.\n"); fclose(fcsv); return; }

    while (fgets(linha, sizeof(linha), fcsv)) {
        if (n_produtos >= capacidade) {
            capacidade *= 2;
            Produto* temp = realloc(produtos, capacidade * sizeof(Produto));
            if (!temp) { printf("ERRO: Falha ao realocar memoria.\n"); free(produtos); fclose(fcsv); return; }
            produtos = temp;
        }

        Produto p = {0};
        char *token = strtok(linha, ",");
        for (int i = 0; token != NULL && i < 9; i++) {
            switch(i) {
                case 2: p.product_id = atoi(token); break;
                case 5: strncpy(p.category_alias, token, TAM_CATEGORY-1); p.category_alias[TAM_CATEGORY-1] = '\0'; break;
                case 6: strncpy(p.brand, token, TAM_BRAND-1); p.brand[TAM_BRAND-1] = '\0'; break;
                case 7: p.price = atof(token); break;
            }
            token = strtok(NULL, ",");
        }
        p.category_alias[TAM_CATEGORY-1] = '\0';
        p.brand[TAM_BRAND-1] = '\0';

        pad_string(p.brand, TAM_BRAND);
        pad_string(p.category_alias, TAM_CATEGORY);
        p.ativo = 'S';
        p.newline = '\n';
        produtos[n_produtos++] = p;
    }

    fclose(fcsv);
    printf("%d registros lidos para produtos.\n", n_produtos);

    qsort(produtos, n_produtos, sizeof(Produto), comparar_produto);

    FILE *fbin = fopen(bin_path, "wb");
    if (fbin) {
        int n_unicos = 0;
        int ultimo_id = INT_MIN; // Usa INT_MIN para permitir IDs negativos/zero
        for (int i = 0; i < n_produtos; i++) {
            // Remove 'product_id > 0', permite qualquer ID não duplicado
            if (produtos[i].product_id != ultimo_id) {
                fwrite(&produtos[i], sizeof(Produto), 1, fbin);
                ultimo_id = produtos[i].product_id;
                n_unicos++;
            }
        }
        fclose(fbin);
        printf("%s criado com %d produtos unicos.\n", bin_path, n_unicos);
    } else {
         printf("ERRO: Nao foi possivel criar o arquivo binario %s.\n", bin_path);
    }
    free(produtos);
}

void mostrar_produtos(const char *arq_bin) {
    // (Código original OK)
    FILE *fbin = fopen(arq_bin, "rb");
    if (!fbin) { printf("ERRO ao abrir %s\n", arq_bin); return; }

    Produto p;
    int contador = 0;
    printf("\n--- PRODUTOS ATIVOS ---\n");
    while (fread(&p, sizeof(Produto), 1, fbin) == 1) {
        if (p.ativo == 'S') {
            char brand_trim[TAM_BRAND+1]={0};
            char category_trim[TAM_CATEGORY+1]={0};
            strncpy(brand_trim, p.brand, TAM_BRAND);
            strncpy(category_trim, p.category_alias, TAM_CATEGORY);
            for(int i = strlen(brand_trim)-1; i >=0 && brand_trim[i] == ' '; i--) brand_trim[i] = '\0';
            for(int i = strlen(category_trim)-1; i >=0 && category_trim[i] == ' '; i--) category_trim[i] = '\0';

            printf("ID: %d | Brand: %s | Price: %.2f | Category: %s\n",
                   p.product_id, brand_trim, p.price, category_trim);
            contador++;
        }
    }
    printf("Total: %d produtos\n", contador);
    fclose(fbin);
}

int inserir_produto(const char *arq_bin) {
    Produto p_novo;
    printf("\n--- INSERIR NOVO PRODUTO ---\n");

    p_novo.product_id = ler_inteiro("Digite o product_id: ");
    // CORREÇÃO: Removida a checagem 'p_novo.product_id <= 0'

    int chave = p_novo.product_id;
    if (pesquisa_binaria(arq_bin, sizeof(Produto), comparar_produto_chave, &chave, offsetof(Produto, ativo)) >= 0) {
        printf("ERRO: product_id %d ja existe!\n", p_novo.product_id);
        return 0;
    }

    // (Resto da lógica OK)
    ler_string("Digite a brand: ", p_novo.brand, TAM_BRAND);
    p_novo.price = ler_double("Digite o preco: ");
    ler_string("Digite a categoria: ", p_novo.category_alias, TAM_CATEGORY);

    pad_string(p_novo.brand, TAM_BRAND);
    pad_string(p_novo.category_alias, TAM_CATEGORY);
    p_novo.ativo = 'S';
    p_novo.newline = '\n';

    FILE *fbin = fopen(arq_bin, "rb");
    long n_registros = 0;
    Produto *produtos = NULL;

    if (fbin) {
        fseek(fbin, 0, SEEK_END);
        long tamanho_arquivo = ftell(fbin);
        if (tamanho_arquivo > 0 && tamanho_arquivo % sizeof(Produto) == 0) {
             n_registros = tamanho_arquivo / sizeof(Produto);
             produtos = malloc((n_registros + 1) * sizeof(Produto));
             if (!produtos) { printf("ERRO ao alocar memoria.\n"); fclose(fbin); return 0; }
             fseek(fbin, 0, SEEK_SET);
             fread(produtos, sizeof(Produto), n_registros, fbin);
        }
        fclose(fbin);
    }
    if (!produtos) {
         n_registros = 0;
         produtos = malloc(sizeof(Produto));
         if (!produtos) { printf("ERRO ao alocar memoria.\n"); return 0; }
    }

    produtos[n_registros] = p_novo;
    n_registros++;
    qsort(produtos, n_registros, sizeof(Produto), comparar_produto);

    fbin = fopen(arq_bin, "wb");
    if (!fbin) { printf("ERRO ao abrir arquivo para escrita.\n"); free(produtos); return 0; }

    fwrite(produtos, sizeof(Produto), n_registros, fbin);
    fclose(fbin);
    free(produtos);

    printf("Produto %d inserido com sucesso!\n", p_novo.product_id);
    return 1;
}

int remover_produto(const char *arq_bin) {
    printf("\n--- REMOVER PRODUTO ---\n");
    int id = ler_inteiro("Digite o product_id para remover: ");

    int chave = id;
    long offset = pesquisa_binaria(arq_bin, sizeof(Produto), comparar_produto_chave, &chave, offsetof(Produto, ativo));

    if (offset == -1) { printf("Produto %d nao encontrado.\n", id); return 0; }
    if (offset == -2) { printf("Produto %d ja esta removido.\n", id); return 0; }

    FILE *fbin = fopen(arq_bin, "r+b");
    if (!fbin) { printf("ERRO ao abrir arquivo para remocao.\n"); return 0; }
    fseek(fbin, offset + offsetof(Produto, ativo), SEEK_SET);
    fwrite("N", sizeof(char), 1, fbin);
    fclose(fbin);

    printf("Produto %d removido logicamente.\n", id);
    return 1;
}

void consultar_produto(const char *arq_bin) {
    printf("\n--- CONSULTAR PRODUTO ---\n");
    int id = ler_inteiro("Digite o product_id para consultar: ");

    int chave = id;
    long offset = pesquisa_binaria(arq_bin, sizeof(Produto), comparar_produto_chave, &chave, offsetof(Produto, ativo));

    if (offset == -1) { printf("Produto %d nao encontrado.\n", id); }
    else if (offset == -2) { printf("Produto %d existe mas foi removido.\n", id); }
    else {
        FILE *fbin = fopen(arq_bin, "rb");
        if (!fbin) { printf("ERRO ao abrir arquivo para leitura.\n"); return; }
        fseek(fbin, offset, SEEK_SET);
        Produto p;
        fread(&p, sizeof(Produto), 1, fbin);

        char brand_trim[TAM_BRAND+1]={0};
        char category_trim[TAM_CATEGORY+1]={0};
        strncpy(brand_trim, p.brand, TAM_BRAND);
        strncpy(category_trim, p.category_alias, TAM_CATEGORY);
        for(int i = strlen(brand_trim)-1; i >=0 && brand_trim[i] == ' '; i--) brand_trim[i] = '\0';
        for(int i = strlen(category_trim)-1; i >=0 && category_trim[i] == ' '; i--) category_trim[i] = '\0';

        printf("\n--- PRODUTO ENCONTRADO ---\n");
        printf("ID: %d\nBrand: %s\nPrice: %.2f\nCategory: %s\n",
               p.product_id, brand_trim, p.price, category_trim);
        fclose(fbin);
    }
}

// --- FUNÇÕES ESPECÍFICAS COMPRAS ---
void pre_processar_compras(const char *csv_path, const char *bin_path) {
    // (Código original OK, com pequenas melhorias)
    printf("Pre-processando COMPRAS de %s...\n", csv_path);
    FILE *fcsv = fopen(csv_path, "r");
    if (!fcsv) { printf("ERRO: Nao foi possivel abrir CSV %s\n", csv_path); return; }

    char linha[2048];
    if (!fgets(linha, sizeof(linha), fcsv)) { fclose(fcsv); return; } // Ignora cabeçalho

    int capacidade = 100000, n_compras = 0;
    Compra* compras = malloc(capacidade * sizeof(Compra));
    if (!compras) { printf("ERRO: Falha ao alocar memoria.\n"); fclose(fcsv); return; }

    while (fgets(linha, sizeof(linha), fcsv)) {
        if (n_compras >= capacidade) {
            capacidade *= 2;
            Compra* temp = realloc(compras, capacidade * sizeof(Compra));
             if (!temp) { printf("ERRO: Falha ao realocar memoria.\n"); free(compras); fclose(fcsv); return; }
             compras = temp;
        }

        Compra c = {0};
        char *token = strtok(linha, ",");
        for (int i = 0; token != NULL && i < 10; i++) {
            switch(i) {
                case 0: strncpy(c.order_datetime, token, TAM_DATETIME-1); c.order_datetime[TAM_DATETIME-1]='\0'; break;
                case 1: c.order_id = atoll(token); break;
                case 2: c.product_id = atoll(token); break;
                case 3: c.quantity = atoi(token); break;
                case 8: c.user_id = atoll(token); break;
            }
            token = strtok(NULL, ",");
        }
        c.order_datetime[TAM_DATETIME-1] = '\0';

        pad_string(c.order_datetime, TAM_DATETIME);
        c.ativo = 'S';
        c.newline = '\n';
        // Mantendo order_id positivo como chave primária (decisão de design)
        if(c.order_id > 0) {
            compras[n_compras++] = c;
        }
    }

    fclose(fcsv);
    printf("%d registros lidos para compras.\n", n_compras);

    qsort(compras, n_compras, sizeof(Compra), comparar_compra);

    FILE *fbin = fopen(bin_path, "wb");
    if (fbin) {
        int n_unicos = 0;
        long long ultimo_id = LLONG_MIN;
        for (int i = 0; i < n_compras; i++) {
            if (compras[i].order_id > 0 && compras[i].order_id != ultimo_id) {
                fwrite(&compras[i], sizeof(Compra), 1, fbin);
                ultimo_id = compras[i].order_id;
                n_unicos++;
            }
        }
        fclose(fbin);
        printf("%s criado com %d compras unicas.\n", bin_path, n_unicos);
    } else {
         printf("ERRO: Nao foi possivel criar o arquivo binario %s.\n", bin_path);
    }
    free(compras);
}

void mostrar_compras(const char *arq_bin) {
    // (Código original OK)
    FILE *fbin = fopen(arq_bin, "rb");
    if (!fbin) { printf("ERRO ao abrir %s\n", arq_bin); return; }

    Compra c;
    int contador = 0;
    printf("\n--- COMPRAS ATIVAS ---\n");
    while (fread(&c, sizeof(Compra), 1, fbin) == 1) {
        if (c.ativo == 'S') {
            char datetime_trim[TAM_DATETIME+1]={0};
            strncpy(datetime_trim, c.order_datetime, TAM_DATETIME);
            for(int i = strlen(datetime_trim)-1; i >=0 && datetime_trim[i] == ' '; i--) datetime_trim[i] = '\0';

            printf("Order: %lld | Product: %lld | User: %lld | Qty: %d | Date: %s\n",
                   c.order_id, c.product_id, c.user_id, c.quantity, datetime_trim);
            contador++;
        }
    }
    printf("Total: %d compras\n", contador);
    fclose(fbin);
}

int inserir_compra(const char *arq_bin) {
    Compra c_nova;
    printf("\n--- INSERIR NOVA COMPRA ---\n");

    c_nova.order_id = ler_long_long("Digite o order_id: ");
     if (c_nova.order_id <= 0) {
        printf("ERRO: ID do pedido deve ser positivo.\n");
        return 0;
    }

    long long chave = c_nova.order_id;
    if (pesquisa_binaria(arq_bin, sizeof(Compra), comparar_compra_chave, &chave, offsetof(Compra, ativo)) >= 0) {
        printf("ERRO: order_id %lld ja existe!\n", c_nova.order_id);
        return 0;
    }

    c_nova.product_id = ler_long_long("Digite o product_id: ");
    // --- INÍCIO DA VALIDAÇÃO DE EXISTÊNCIA DO PRODUTO ---
    int chave_prod = (int)c_nova.product_id; // Assumindo IDs de produto cabem em int
    // Verifica se produto existe E está ativo no arquivo de produtos
    if (pesquisa_binaria(ARQ_PRODUTOS_BIN, sizeof(Produto), comparar_produto_chave, &chave_prod, offsetof(Produto, ativo)) < 0) {
        printf("ERRO: product_id %lld nao encontrado ou inativo no cadastro de produtos. Insercao cancelada.\n", c_nova.product_id);
        return 0; // Impede a inserção
    }
    // --- FIM DA VALIDAÇÃO ---

    c_nova.user_id = ler_long_long("Digite o user_id: ");

    // --- INÍCIO DA VALIDAÇÃO DE DATA ---
    while (1) {
        ler_string("Digite a data/hora (YYYY-MM-DD HH:MM:SS): ", c_nova.order_datetime, TAM_DATETIME);
        if (validar_e_formatar_data(c_nova.order_datetime, TAM_DATETIME)) {
            break; // Sai do loop se a data for válida
        } else {
            printf("ERRO: Formato de data/hora invalido. Use YYYY-MM-DD HH:MM:SS.\n");
        }
    }
    // --- FIM DA VALIDAÇÃO DE DATA ---

    c_nova.quantity = ler_inteiro("Digite a quantidade: ");
     if (c_nova.quantity <= 0) {
        printf("ERRO: Quantidade deve ser positiva.\n");
        return 0;
    }

    // O " UTC" já foi adicionado pela função validar_e_formatar_data
    pad_string(c_nova.order_datetime, TAM_DATETIME);
    c_nova.ativo = 'S';
    c_nova.newline = '\n';

    // (O resto da função continua igual: carregar, inserir, qsort, gravar...)
    FILE *fbin = fopen(arq_bin, "rb");
    long n_registros = 0;
    Compra *compras = NULL;

    if (fbin) {
        fseek(fbin, 0, SEEK_END);
        long tamanho_arquivo = ftell(fbin);
        if (tamanho_arquivo > 0 && tamanho_arquivo % sizeof(Compra) == 0) {
             n_registros = tamanho_arquivo / sizeof(Compra);
             compras = malloc((n_registros + 1) * sizeof(Compra));
             if (!compras) { printf("ERRO ao alocar memoria.\n"); fclose(fbin); return 0; }
             fseek(fbin, 0, SEEK_SET);
             fread(compras, sizeof(Compra), n_registros, fbin);
        }
        fclose(fbin);
    }
    if (!compras) {
         n_registros = 0;
         compras = malloc(sizeof(Compra));
         if (!compras) { printf("ERRO ao alocar memoria.\n"); return 0; }
    }

    compras[n_registros] = c_nova;
    n_registros++;
    qsort(compras, n_registros, sizeof(Compra), comparar_compra);

    fbin = fopen(arq_bin, "wb");
    if (!fbin) { printf("ERRO ao abrir arquivo para escrita.\n"); free(compras); return 0; }

    fwrite(compras, sizeof(Compra), n_registros, fbin);
    fclose(fbin);
    free(compras);

    printf("Compra %lld inserida com sucesso!\n", c_nova.order_id);
    return 1;
}

int remover_compra(const char *arq_bin) {
    printf("\n--- REMOVER COMPRA ---\n");
    long long id = ler_long_long("Digite o order_id para remover: ");

    long long chave = id;
    long offset = pesquisa_binaria(arq_bin, sizeof(Compra), comparar_compra_chave, &chave, offsetof(Compra, ativo));

    if (offset == -1) { printf("Compra %lld nao encontrada.\n", id); return 0; }
    if (offset == -2) { printf("Compra %lld ja esta removida.\n", id); return 0; }

    FILE *fbin = fopen(arq_bin, "r+b");
    if (!fbin) { printf("ERRO ao abrir arquivo para remocao.\n"); return 0; }
    fseek(fbin, offset + offsetof(Compra, ativo), SEEK_SET);
    fwrite("N", sizeof(char), 1, fbin);
    fclose(fbin);

    printf("Compra %lld removida logicamente.\n", id);
    return 1;
}

void consultar_compra(const char *arq_bin) {
    printf("\n--- CONSULTAR COMPRA ---\n");
    long long id = ler_long_long("Digite o order_id para consultar: ");

    long long chave = id;
    long offset = pesquisa_binaria(arq_bin, sizeof(Compra), comparar_compra_chave, &chave, offsetof(Compra, ativo));

    if (offset == -1) { printf("Compra %lld nao encontrada.\n", id); }
    else if (offset == -2) { printf("Compra %lld existe mas foi removida.\n", id); }
    else {
        FILE *fbin = fopen(arq_bin, "rb");
        if (!fbin) { printf("ERRO ao abrir arquivo para leitura.\n"); return; }
        fseek(fbin, offset, SEEK_SET);
        Compra c;
        fread(&c, sizeof(Compra), 1, fbin);

        char datetime_trim[TAM_DATETIME+1]={0};
        strncpy(datetime_trim, c.order_datetime, TAM_DATETIME);
        for(int i = strlen(datetime_trim)-1; i >=0 && datetime_trim[i] == ' '; i--) datetime_trim[i] = '\0';

        printf("\n--- COMPRA ENCONTRADA ---\n");
        printf("Order ID: %lld\nProduct ID: %lld\nUser ID: %lld\nQuantity: %d\nDate: %s\n",
               c.order_id, c.product_id, c.user_id, c.quantity, datetime_trim);
        fclose(fbin);
    }
}

// --- CONSULTAS COM ÍNDICE ---

void consultar_produto_com_indice(const char *arq_indice, const char *arq_dados) {
    printf("\n--- CONSULTAR PRODUTO COM INDICE ---\n");
    int id = ler_inteiro("Digite o product_id para buscar: ");

    FILE *f_idx = fopen(arq_indice, "rb");
    if (!f_idx) { printf("ERRO: Indice %s nao encontrado.\n", arq_indice); return; }

    fseek(f_idx, 0, SEEK_END);
    long tam_idx = ftell(f_idx);
     if (tam_idx <= 0 || tam_idx % sizeof(IndiceProduto) != 0) {
         printf("ERRO: Arquivo de indice %s invalido ou vazio.\n", arq_indice);
         fclose(f_idx);
         return;
    }
    int n_indices = tam_idx / sizeof(IndiceProduto);
    fseek(f_idx, 0, SEEK_SET);

    IndiceProduto *indices = malloc(tam_idx);
    if (!indices) { printf("ERRO ao alocar memoria para indice.\n"); fclose(f_idx); return; }

    fread(indices, sizeof(IndiceProduto), n_indices, f_idx);
    fclose(f_idx);

    int inicio = 0, fim = n_indices - 1;
    int idx_bloco = -1;
    while (inicio <= fim) {
        int meio = inicio + (fim - inicio) / 2;
        if (indices[meio].chave <= id) {
            idx_bloco = meio;
            inicio = meio + 1;
        } else {
            fim = meio - 1;
        }
    }

    if (idx_bloco == -1) {
        printf("Produto %d nao encontrado (fora do range do indice).\n", id);
        free(indices);
        return;
    }

    FILE *f_dados = fopen(arq_dados, "rb");
    if (!f_dados) { printf("ERRO ao abrir arquivo de dados %s.\n", arq_dados); free(indices); return; }

    fseek(f_dados, indices[idx_bloco].offset, SEEK_SET);

    Produto p;
    int encontrado = 0;
    for (int i = 0; i < BLOCO_INDICE; i++) {
        if (fread(&p, sizeof(Produto), 1, f_dados) != 1) break;
        if (p.product_id == id) {
            if (p.ativo == 'S') {
                char brand_trim[TAM_BRAND+1]={0};
                char category_trim[TAM_CATEGORY+1]={0};
                strncpy(brand_trim, p.brand, TAM_BRAND);
                strncpy(category_trim, p.category_alias, TAM_CATEGORY);
                for(int j = strlen(brand_trim)-1; j >=0 && brand_trim[j] == ' '; j--) brand_trim[j] = '\0';
                for(int j = strlen(category_trim)-1; j >=0 && category_trim[j] == ' '; j--) category_trim[j] = '\0';

                printf("\n--- PRODUTO ENCONTRADO (via indice) ---\n");
                printf("ID: %d | Brand: %s | Price: %.2f | Category: %s\n",
                       p.product_id, brand_trim, p.price, category_trim);
                encontrado = 1;
            } else {
                printf("Produto %d existe mas foi removido.\n", id);
                encontrado = 2;
            }
            break;
        }
        if (p.product_id > id) break;
    }

    if (encontrado == 0) {
        printf("Produto %d nao encontrado no bloco verificado.\n", id);
    }

    fclose(f_dados);
    free(indices);
}

void consultar_compra_com_indice(const char *arq_indice, const char *arq_dados) {
    printf("\n--- CONSULTAR COMPRA COM INDICE ---\n");
    long long id = ler_long_long("Digite o order_id para buscar: ");

    FILE *f_idx = fopen(arq_indice, "rb");
    if (!f_idx) { printf("ERRO: Indice %s nao encontrado.\n", arq_indice); return; }

    fseek(f_idx, 0, SEEK_END);
    long tam_idx = ftell(f_idx);
     if (tam_idx <= 0 || tam_idx % sizeof(IndiceCompra) != 0) {
         printf("ERRO: Arquivo de indice %s invalido ou vazio.\n", arq_indice);
         fclose(f_idx);
         return;
    }
    int n_indices = tam_idx / sizeof(IndiceCompra);
    fseek(f_idx, 0, SEEK_SET);

    IndiceCompra *indices = malloc(tam_idx);
    if (!indices) { printf("ERRO ao alocar memoria para indice.\n"); fclose(f_idx); return; }

    fread(indices, sizeof(IndiceCompra), n_indices, f_idx);
    fclose(f_idx);

    int inicio = 0, fim = n_indices - 1;
    int idx_bloco = -1;
    while (inicio <= fim) {
        int meio = inicio + (fim - inicio) / 2;
        if (indices[meio].chave <= id) {
            idx_bloco = meio;
            inicio = meio + 1;
        } else {
            fim = meio - 1;
        }
    }

    if (idx_bloco == -1) {
        printf("Compra %lld nao encontrada (fora do range do indice).\n", id);
        free(indices);
        return;
    }

    FILE *f_dados = fopen(arq_dados, "rb");
    if (!f_dados) { printf("ERRO ao abrir arquivo de dados %s.\n", arq_dados); free(indices); return; }

    fseek(f_dados, indices[idx_bloco].offset, SEEK_SET);

    Compra c;
    int encontrado = 0;
    for (int i = 0; i < BLOCO_INDICE; i++) {
        if (fread(&c, sizeof(Compra), 1, f_dados) != 1) break;
        if (c.order_id == id) {
            if (c.ativo == 'S') {
                char datetime_trim[TAM_DATETIME+1]={0};
                strncpy(datetime_trim, c.order_datetime, TAM_DATETIME);
                for(int j = strlen(datetime_trim)-1; j >=0 && datetime_trim[j] == ' '; j--) datetime_trim[j] = '\0';

                printf("\n--- COMPRA ENCONTRADA (via indice) ---\n");
                printf("Order: %lld | Product: %lld | User: %lld | Qty: %d | Date: %s\n",
                       c.order_id, c.product_id, c.user_id, c.quantity, datetime_trim);
                encontrado = 1;
            } else {
                printf("Compra %lld existe mas foi removida.\n", id);
                encontrado = 2;
            }
            break;
        }
        if (c.order_id > id) break;
    }

    if (encontrado == 0) {
        printf("Compra %lld nao encontrada no bloco verificado.\n", id);
    }

    fclose(f_dados);
    free(indices);
}

// --- CONSULTAS ESPECÍFICAS ---
void consulta_produto_mais_caro() {
    // (Código original OK)
    FILE *f = fopen(ARQ_PRODUTOS_BIN, "rb");
    if (!f) { printf("ERRO ao abrir %s\n", ARQ_PRODUTOS_BIN); return; }
    Produto p, mais_caro = {0};
    double max_preco = -1;
    int encontrado = 0;

    while (fread(&p, sizeof(Produto), 1, f) == 1) {
        if (p.ativo == 'S') {
            if (!encontrado || p.price > max_preco) {
                 max_preco = p.price;
                 mais_caro = p;
                 encontrado = 1;
            }
        }
    }
    fclose(f);
    if (encontrado) {
        char brand_trim[TAM_BRAND+1]={0};
        char category_trim[TAM_CATEGORY+1]={0};
        strncpy(brand_trim, mais_caro.brand, TAM_BRAND);
        strncpy(category_trim, mais_caro.category_alias, TAM_CATEGORY);
        for(int i = strlen(brand_trim)-1; i >=0 && brand_trim[i] == ' '; i--) brand_trim[i] = '\0';
        for(int i = strlen(category_trim)-1; i >=0 && category_trim[i] == ' '; i--) category_trim[i] = '\0';

        printf("\n--- PRODUTO MAIS CARO ---\n");
        printf("ID: %d | Brand: %s | Price: %.2f | Category: %s\n",
               mais_caro.product_id, brand_trim, mais_caro.price, category_trim);
    } else {
        printf("Nenhum produto ativo encontrado.\n");
    }
}

void consulta_valor_total_vendido() {
    FILE *f_comp = fopen(ARQ_COMPRAS_BIN, "rb");
    if (!f_comp) { printf("ERRO ao abrir arquivo de compras %s\n", ARQ_COMPRAS_BIN); return; }

    printf("Calculando valor total vendido (pode demorar)...\n");

    double total = 0;
    Compra c;
    int compras_contadas = 0;
    int produtos_nao_encontrados = 0;

    while (fread(&c, sizeof(Compra), 1, f_comp) == 1) {
        if (c.ativo != 'S') continue;

        int id_produto_busca = (int)c.product_id;
        // Permite IDs negativos/zero se tiverem sido permitidos na inserção
        // if (id_produto_busca <= 0 && id_produto_busca != INT_MIN) { // Se não quiser permitir neg/zero aqui
        //     produtos_nao_encontrados++;
        //     continue;
        // }


        long offset_prod = pesquisa_binaria(
            ARQ_PRODUTOS_BIN, sizeof(Produto), comparar_produto_chave,
            &id_produto_busca, offsetof(Produto, ativo)
        );

        if (offset_prod >= 0) {
            FILE *f_prod_leitura = fopen(ARQ_PRODUTOS_BIN, "rb");
            if (f_prod_leitura) {
                Produto p_temp;
                fseek(f_prod_leitura, offset_prod, SEEK_SET);
                if(fread(&p_temp, sizeof(Produto), 1, f_prod_leitura) == 1) {
                    total += p_temp.price * c.quantity;
                    compras_contadas++;
                } else {
                    produtos_nao_encontrados++;
                }
                fclose(f_prod_leitura);
            } else {
                 produtos_nao_encontrados++;
            }
        } else { // Inclui -1 (não encontrado) e -2 (removido)
             produtos_nao_encontrados++;
        }
    }
    fclose(f_comp);

    printf("\n--- VALOR TOTAL VENDIDO ---\n");
    printf("Total: R$ %.2f\n", total);
    printf("(Calculado a partir de %d compras válidas. %d produtos não encontrados/inválidos/removidos)\n",
           compras_contadas, produtos_nao_encontrados);
}

// --- MENUS ---
void menu_consultas() {
    // (Código original OK)
    int opcao;
    do {
        printf("\n--- CONSULTAS ESPECIFICAS ---\n");
        printf("1. Produto mais caro\n");
        printf("2. Valor total vendido\n");
        printf("3. Voltar\n");
        opcao = ler_inteiro("Opcao: ");

        switch (opcao) {
            case 1: consulta_produto_mais_caro(); break;
            case 2: consulta_valor_total_vendido(); break;
            case 3: break;
            default: printf("Opcao invalida\n");
        }
    } while (opcao != 3);
}

void menu_produtos() {
    int opcao, reconstruir = 0;
    FILE *f = fopen(ARQ_PRODUTOS_BIN, "rb");
    if (!f) {
        printf("Arquivo %s nao encontrado. Pre-processando...\n", ARQ_PRODUTOS_BIN);
        pre_processar_produtos(ARQ_CSV, ARQ_PRODUTOS_BIN);
        reconstruir = 1;
    } else {
        fclose(f);
    }

    f = fopen(ARQ_PRODUTOS_IDX, "rb");
    if (!f) {
        if (fopen(ARQ_PRODUTOS_BIN,"rb") != NULL) {
             printf("Arquivo de indice %s nao encontrado.\n", ARQ_PRODUTOS_IDX);
             reconstruir = 1;
        }
    } else {
        fclose(f);
    }

    do {
        if (reconstruir) {
            printf("\n(Sistema: Reconstruindo indice %s...)\n", ARQ_PRODUTOS_IDX);
            criar_indice(ARQ_PRODUTOS_BIN, ARQ_PRODUTOS_IDX, sizeof(Produto), sizeof(IndiceProduto),
                         extrai_chave_produto, NULL, 0, offsetof(Produto, ativo));
            reconstruir = 0;
        }

        printf("\n--- MENU PRODUTOS ---\n");
        printf("1. Mostrar todos\n");
        printf("2. Inserir produto\n");
        printf("3. Remover produto\n");
        printf("4. Consultar produto (binaria)\n");
        printf("5. Consultar produto (com indice)\n");
        printf("6. Recriar do CSV\n");
        printf("7. Voltar\n");
        opcao = ler_inteiro("Opcao: ");

        switch (opcao) {
            case 1: mostrar_produtos(ARQ_PRODUTOS_BIN); break;
            case 2:
                if (inserir_produto(ARQ_PRODUTOS_BIN)) reconstruir = 1;
                break;
            case 3:
                if (remover_produto(ARQ_PRODUTOS_BIN)) reconstruir = 1;
                break;
            case 4: consultar_produto(ARQ_PRODUTOS_BIN); break;
            case 5: consultar_produto_com_indice(ARQ_PRODUTOS_IDX, ARQ_PRODUTOS_BIN); break;
             case 6: {
                 printf("ATENCAO: Isso vai apagar os dados atuais e recriar a partir do CSV.\n");
                 printf("Tem certeza (s/n)? ");
                 char resp = getchar();
                 while (getchar() != '\n');
                 if (resp == 's' || resp == 'S') {
                    pre_processar_produtos(ARQ_CSV, ARQ_PRODUTOS_BIN);
                    reconstruir = 1;
                 } else {
                    printf("Operacao cancelada.\n");
                 }
                 break;
            }
            case 7: break;
            default: printf("Opcao invalida\n");
        }
    } while (opcao != 7);
}

void menu_compras() {
    int opcao, reconstruir = 0;
    FILE *f = fopen(ARQ_COMPRAS_BIN, "rb");
    if (!f) {
        printf("Arquivo %s nao encontrado. Pre-processando...\n", ARQ_COMPRAS_BIN);
        pre_processar_compras(ARQ_CSV, ARQ_COMPRAS_BIN);
        reconstruir = 1;
    } else {
        fclose(f);
    }

     f = fopen(ARQ_COMPRAS_IDX, "rb");
    if (!f) {
        if (fopen(ARQ_COMPRAS_BIN,"rb") != NULL) {
             printf("Arquivo de indice %s nao encontrado.\n", ARQ_COMPRAS_IDX);
             reconstruir = 1;
        }
    } else {
        fclose(f);
    }

    do {
        if (reconstruir) {
            printf("\n(Sistema: Reconstruindo indice %s...)\n", ARQ_COMPRAS_IDX);
            criar_indice(ARQ_COMPRAS_BIN, ARQ_COMPRAS_IDX, sizeof(Compra), sizeof(IndiceCompra),
                         NULL, extrai_chave_compra, 1, offsetof(Compra, ativo));
            reconstruir = 0;
        }

        printf("\n--- MENU COMPRAS ---\n");
        printf("1. Mostrar todas\n");
        printf("2. Inserir compra\n");
        printf("3. Remover compra\n");
        printf("4. Consultar compra (binaria)\n");
        printf("5. Consultar compra (com indice)\n");
        printf("6. Recriar do CSV\n");
        printf("7. Voltar\n");
        opcao = ler_inteiro("Opcao: ");

        switch (opcao) {
            case 1: mostrar_compras(ARQ_COMPRAS_BIN); break;
            case 2:
                if (inserir_compra(ARQ_COMPRAS_BIN)) reconstruir = 1;
                break;
            case 3:
                if (remover_compra(ARQ_COMPRAS_BIN)) reconstruir = 1;
                break;
            case 4: consultar_compra(ARQ_COMPRAS_BIN); break;
            case 5: consultar_compra_com_indice(ARQ_COMPRAS_IDX, ARQ_COMPRAS_BIN); break;
             case 6: {
                 printf("ATENCAO: Isso vai apagar os dados atuais e recriar a partir do CSV.\n");
                 printf("Tem certeza (s/n)? ");
                 char resp = getchar();
                 while (getchar() != '\n');
                 if (resp == 's' || resp == 'S') {
                    pre_processar_compras(ARQ_CSV, ARQ_COMPRAS_BIN);
                    reconstruir = 1;
                 } else {
                    printf("Operacao cancelada.\n");
                 }
                 break;
            }
            case 7: break;
            default: printf("Opcao invalida\n");
        }
    } while (opcao != 7);
}

int main() {
    // (Código original OK)
    printf("=== Sistema de Arquivos: Produtos e Compras ===\n");

    int opcao;
    do {
        printf("\n--- MENU PRINCIPAL ---\n");
        printf("1. Gerenciar Produtos\n");
        printf("2. Gerenciar Compras\n");
        printf("3. Consultas Especificas\n");
        printf("4. Sair\n");
        opcao = ler_inteiro("Opcao: ");

        switch (opcao) {
            case 1: menu_produtos(); break;
            case 2: menu_compras(); break;
            case 3: menu_consultas(); break;
            case 4: printf("Saindo...\n"); break;
            default: printf("Opcao invalida\n");
        }
    } while (opcao != 4);

    return 0;
}
