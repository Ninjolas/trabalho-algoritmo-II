#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // Para offsetof
#include <stdint.h> // Para int64_t
#include <limits.h> // Para LLONG_MIN e INT_MIN
#include <ctype.h>  // Para isdigit() na validao de data

// --- DEFINES ---
const char* ARQ_CSV = "jewelry.csv";
const char* ARQ_PRODUTOS_BIN = "produtos.bin";
const char* ARQ_PRODUTOS_IDX = "produtos_idx.bin";
const char* ARQ_COMPRAS_BIN = "compras.bin";
const char* ARQ_COMPRAS_IDX = "compras_idx.bin";

#define TAM_BRAND 50
#define TAM_CATEGORY 100
#define TAM_DATETIME 30
#define BLOCO_INDICE 100 // Define o tamanho do bloco para o indice parcial

// --- ESTRUTURAS ---
typedef struct {
    int64_t product_id;
    char brand[TAM_BRAND];
    double price;
    char category_alias[TAM_CATEGORY];
    char ativo; // 'S' para ativo, 'N' para removido (remocao logica)
    char newline;
} Produto;

typedef struct {
    int64_t chave; // product_id
    long offset;   // Posicao (em bytes) do registro no arquivo .bin
} IndiceProduto;

typedef struct {
    long long order_id;
    int64_t product_id;
    long long user_id;
    char order_datetime[TAM_DATETIME];
    int quantity;
    char ativo; // 'S' para ativo, 'N' para removido (remocao logica)
    char newline;
} Compra;

typedef struct {
    long long chave; // order_id
    long offset;    // Posicao (em bytes) do registro no arquivo .bin
} IndiceCompra;

// --- FUNÇÕES AUXILIARES COMUNS ---

/**
 * @brief Garante que uma string tenha um tamanho fixo, preenchendo com espacos.
 * Isso e essencial para que todos os registros no arquivo binario tenham
 * o mesmo tamanho, permitindo calculos de offset (posicao) confiaveis.
 * @param str A string a ser preenchida.
 * @param tam O tamanho final desejado (incluindo o \0).
 */
void pad_string(char *str, int tam) {
    int len = (int)strlen(str);
    if (len >= tam) {
        str[tam - 1] = '\0'; // Trunca se for maior
    } else {
        // Preenche com espacos
        memset(str + len, ' ', tam - len - 1);
        str[tam - 1] = '\0'; // Garante o terminador nulo
    }
}

/**
 * @brief Le um inteiro do stdin com validacao de tipo e limpeza de buffer.
 */
int ler_inteiro(const char* prompt) {
    int valor;
    printf("%s", prompt);
    while (scanf("%d", &valor) != 1) {
        while (getchar() != '\n'); // Limpa buffer
        printf("Entrada invalida. %s", prompt);
    }
    while (getchar() != '\n'); // Limpa buffer
    return valor;
}

/**
 * @brief Le um long long do stdin com validacao de tipo e limpeza de buffer.
 */
long long ler_long_long(const char* prompt) {
    long long valor;
    printf("%s", prompt);
    while (scanf("%lld", &valor) != 1) {
        while (getchar() != '\n'); // Limpa buffer
        printf("Entrada invalida. %s", prompt);
    }
    while (getchar() != '\n'); // Limpa buffer
    return valor;
}

/**
 * @brief Le um double do stdin com validacao de tipo e limpeza de buffer.
 */
double ler_double(const char* prompt) {
    double valor;
    printf("%s", prompt);
    while (scanf("%lf", &valor) != 1) {
        while (getchar() != '\n'); // Limpa buffer
        printf("Entrada invalida. %s", prompt);
    }
    while (getchar() != '\n'); // Limpa buffer
    return valor;
}

/**
 * @brief Le uma string do stdin usando fgets e remove o \n final.
 */
void ler_string(const char* prompt, char* buffer, int tamanho) {
    printf("%s", prompt);
    fgets(buffer, tamanho, stdin);
    buffer[strcspn(buffer, "\n")] = 0; // Remove o \n
}

/**
 * Valida o formato bsico da data/hora (YYYY-MM-DD HH:MM:SS)
 * e adiciona " UTC" no final se o formato estiver correto e houver espaço.
 * Retorna 1 se o formato for válido, 0 caso contrário.
 */
int validar_e_formatar_data(char* buffer, int tamanho_buffer) {
    int ano, mes, dia, hora, min, seg;
    char espaco;

    // 1. Tenta "escanear" a string no formato esperado
    if (sscanf(buffer, "%d-%d-%d%c%d:%d:%d",
               &ano, &mes, &dia, &espaco, &hora, &min, &seg) == 7 && espaco == ' ')
    {
        // 2. Verificao basica dos ranges
        if (ano >= 0 && mes >= 1 && mes <= 12 && dia >= 1 && dia <= 31 &&
            hora >= 0 && hora <= 23 && min >= 0 && min <= 59 && seg >= 0 && seg <= 59)
        {
            // 3. Verifica se a string tem EXATAMENTE 19 caracteres
            if (strlen(buffer) == 19) {
                 // 4. Verifica espaco para adicionar " UTC" (4 chars + \0 = 5)
                 if (19 + 4 < tamanho_buffer) {
                     strcat(buffer, " UTC"); // Adiciona " UTC"
                     return 1; // Valido e UTC adicionado
                 } else {
                     printf("AVISO: Buffer pequeno demais para adicionar UTC.\n");
                     return 1; // Valido, mas sem UTC
                 }
            }
        }
    }

    return 0;
}

// --- FUNCOES DE COMPARACAO (para qsort e bsearch) ---
int comparar_produto(const void* a, const void* b) {
    int64_t id_a = ((Produto*)a)->product_id;
    int64_t id_b = ((Produto*)b)->product_id;
    return (id_a > id_b) - (id_a < id_b);
}

// Compara uma struct Produto com uma chave (int64_t)
int comparar_produto_chave(const void* a, const void* b) {
    int64_t id_a = ((Produto*)a)->product_id;
    int64_t id_b = *(int64_t*)b;
    return (id_a > id_b) - (id_a < id_a);
}

int comparar_compra(const void* a, const void* b) {
    long long id_a = ((Compra*)a)->order_id;
    long long id_b = ((Compra*)b)->order_id;
    return (id_a > id_b) - (id_a < id_b);
}

// Compara uma struct Compra com uma chave (long long)
int comparar_compra_chave(const void* a, const void* b) {
    long long id_a = ((Compra*)a)->order_id;
    long long id_b = *(long long*)b;
    return (id_a > id_b) - (id_a < id_b);
}

// --- FUNCOES DE EXTRACAO DE CHAVE (para o indice generico) ---
int64_t extrai_chave_produto(const void* reg) {
    return ((Produto*)reg)->product_id;
}

long long extrai_chave_compra(const void* reg) {
    return ((Compra*)reg)->order_id;
}

// --- FUNCOES GENERICAS PARA ARQUIVOS ---

/**
 * @brief Cria um arquivo de indice parcial (sequencial-indexado).
 * Ele le o arquivo de dados .bin e, a cada 'BLOCO_INDICE' registros ATIVOS,
 * ele grava a chave e o offset (posicao) atual no arquivo .idx.
 *
 * @param arq_dados Caminho do arquivo binario de dados (ex: "produtos.bin").
 * @param arq_indice Caminho do arquivo de indice a ser criado (ex: "produtos_idx.bin").
 * @param tam_registro Tamanho da struct de dados (ex: sizeof(Produto)).
 * @param tam_indice Tamanho da struct de indice (ex: sizeof(IndiceProduto)).
 * @param extrai_chave_i64 Ponteiro de funcao para extrair chave int64_t.
 * @param extrai_chave_ll Ponteiro de funcao para extrair chave long long.
 * @param is_long_long Flag (1 ou 0) para saber qual funcao de extracao usar.
 * @param offset_ativo Posicao (offsetof) do campo 'ativo' na struct.
 */
void criar_indice(const char *arq_dados, const char *arq_indice,
                  size_t tam_registro, size_t tam_indice,
                  int64_t (*extrai_chave_i64)(const void*),
                  long long (*extrai_chave_ll)(const void*),
                  int is_long_long,
                  size_t offset_ativo) {
    FILE *f_dados = fopen(arq_dados, "rb");
    FILE *f_indice = fopen(arq_indice, "wb");
    if (!f_dados || !f_indice) { /* ... (erro) ... */ return; }

    void *registro = malloc(tam_registro);
    if (!registro) { /* ... (erro) ... */ fclose(f_dados); fclose(f_indice); return; }

    long offset = 0;
    int contador_registros_ativos = 0; // So conta registros marcados com 'S'

    while (fread(registro, tam_registro, 1, f_dados) == 1) {
        // Verifica se o registro esta ativo antes de considera-lo para o indice
        if (((char*)registro)[offset_ativo] == 'S') {

            // Se for o primeiro registro de um bloco, grava no indice
            if (contador_registros_ativos % BLOCO_INDICE == 0) {
                if (is_long_long) {
                    IndiceCompra idx = {extrai_chave_ll(registro), offset};
                    fwrite(&idx, tam_indice, 1, f_indice);
                } else {
                    IndiceProduto idx = {extrai_chave_i64(registro), offset};
                    fwrite(&idx, tam_indice, 1, f_indice);
                }
            }
            contador_registros_ativos++;
        }
        // Atualiza o offset para a posicao do PROXIMO registro
        offset = ftell(f_dados);
    }

    free(registro);
    fclose(f_dados);
    fclose(f_indice);
    printf("Indice criado com %d entradas.\n", (contador_registros_ativos + BLOCO_INDICE - 1) / BLOCO_INDICE);
}


/**
 * @brief Realiza uma pesquisa binaria DIRETAMENTE NO ARQUIVO binario.
 * Nao carrega o arquivo para a RAM. Usa fseek para pular entre os registros.
 *
 * @param arq_bin Caminho do arquivo binario de dados.
 * @param tam_registro Tamanho da struct de dados (ex: sizeof(Produto)).
 * @param comparador Ponteiro de funcao de comparacao (ex: comparar_produto_chave).
 * @param chave_busca Ponteiro para a chave (ID) que estamos buscando.
 * @param offset_ativo Posicao (offsetof) do campo 'ativo' na struct.
 * @return long
 * - Retorna o OFFSET (posicao em bytes) se encontrar e o registro estiver ATIVO ('S').
 * - Retorna -1 se nao encontrar o registro.
 * - Retorna -2 se encontrar, mas o registro estiver REMOVIDO ('N').
 */
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

        // Pula o cursor do arquivo para a posicao do registro do "meio"
        if (fseek(fbin, meio * tam_registro, SEEK_SET) != 0) { free(registro); fclose(fbin); return -1; }

        // Le apenas UM registro (o do "meio")
        if (fread(registro, tam_registro, 1, fbin) != 1) { free(registro); fclose(fbin); return -1; }

        int cmp = comparador(registro, chave_busca);

        if (cmp == 0) {
            // Encontrou a chave! Agora verifica se esta ativa.
            char ativo = ((char*)registro)[offset_ativo];
            long result = (ativo == 'S') ? (meio * tam_registro) : -2; // -2 = removido
            free(registro);
            fclose(fbin);
            return result;
        }

        (cmp < 0) ? (inicio = meio + 1) : (fim = meio - 1);
    }

    free(registro);
    fclose(fbin);
    return -1; // Nao encontrou
}


// --- FUNCOES ESPECIFICAS PRODUTOS ---

/**
 * @brief Le o CSV, ordena em RAM e grava o arquivo .bin inicial de produtos.
 * Este e o unico momento (alem da insercao) em que muitos dados
 * sao mantidos em RAM, para permitir a ordenacao inicial com qsort.
 * Tambem remove duplicatas de product_id durante a gravacao.
 */
void pre_processar_produtos(const char *csv_path, const char *bin_path) {
    printf("Pre-processando PRODUTOS de %s...\n", csv_path);
    FILE *fcsv = fopen(csv_path, "r");
    if (!fcsv) { printf("ERRO: Nao foi possivel abrir CSV %s\n", csv_path); return; }

    char linha[2048];
    if (!fgets(linha, sizeof(linha), fcsv)) { fclose(fcsv); return; } // Ignora cabealho

    int capacidade = 100000, n_produtos = 0;
    Produto* produtos = malloc(capacidade * sizeof(Produto));
    if (!produtos) { printf("ERRO: Falha ao alocar memoria.\n"); fclose(fcsv); return; }

    // 1. Le CSV para a RAM
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
                case 2: p.product_id = atoll(token); break;
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

    // 2. Ordena na RAM usando qsort
    qsort(produtos, n_produtos, sizeof(Produto), comparar_produto);

    // 3. Grava no arquivo .bin, pulando duplicatas
    FILE *fbin = fopen(bin_path, "wb");
    if (fbin) {
        int n_unicos = 0;
        int64_t ultimo_id = LLONG_MIN;
        for (int i = 0; i < n_produtos; i++) {
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

/**
 * @brief Le o arquivo .bin sequencialmente e imprime todos os produtos ATIVOS.
 */
void mostrar_produtos(const char *arq_bin) {
    FILE *fbin = fopen(arq_bin, "rb");
    if (!fbin) { printf("ERRO ao abrir %s\n", arq_bin); return; }

    Produto p;
    int contador = 0;
    printf("\n--- PRODUTOS ATIVOS ---\n");
    while (fread(&p, sizeof(Produto), 1, fbin) == 1) {
        if (p.ativo == 'S') {
            // Logica para "trim" (remover espacos) antes de imprimir
            char brand_trim[TAM_BRAND+1]={0};
            char category_trim[TAM_CATEGORY+1]={0};
            strncpy(brand_trim, p.brand, TAM_BRAND);
            strncpy(category_trim, p.category_alias, TAM_CATEGORY);
            for(int i = strlen(brand_trim)-1; i >=0 && brand_trim[i] == ' '; i--) brand_trim[i] = '\0';
            for(int i = strlen(category_trim)-1; i >=0 && category_trim[i] == ' '; i--) category_trim[i] = '\0';

            printf("ID: %lld | Brand: %s | Price: %.2f | Category: %s\n",
                   p.product_id, brand_trim, p.price, category_trim);
            contador++;
        }
    }
    printf("Total: %d produtos\n", contador);
    fclose(fbin);
}

/**
 * @brief Insere um novo produto no arquivo binario.
 * ESTRATEGIA: Para manter o arquivo 100% ordenado (necessario para a
 * pesquisa_binaria funcionar), esta funcao:
 * 1. Le o arquivo .bin INTEIRO para a RAM.
 * 2. Adiciona o novo registro no array em RAM.
 * 3. Re-ordena o array INTEIRO com qsort.
 * 4. Re-escreve o arquivo .bin INTEIRO a partir da RAM.
 * Esta e uma operacao custosa, mas garante a consistencia da ordenacao.
 * @return 1 se foi inserido, 0 se houve erro (ex: ID ja existe).
 */
int inserir_produto(const char *arq_bin) {
    Produto p_novo;
    printf("\n--- INSERIR NOVO PRODUTO ---\n");

    p_novo.product_id = ler_long_long("Digite o product_id: ");

    // 1. Verifica se a chave ja existe (usando a pesquisa binaria)
    int64_t chave = p_novo.product_id;
    if (pesquisa_binaria(arq_bin, sizeof(Produto), comparar_produto_chave, &chave, offsetof(Produto, ativo)) >= 0) {
        printf("ERRO: product_id %lld ja existe!\n", p_novo.product_id);
        return 0;
    }

    ler_string("Digite a brand: ", p_novo.brand, TAM_BRAND);
    p_novo.price = ler_double("Digite o preco: ");
    ler_string("Digite a categoria: ", p_novo.category_alias, TAM_CATEGORY);

    pad_string(p_novo.brand, TAM_BRAND);
    pad_string(p_novo.category_alias, TAM_CATEGORY);
    p_novo.ativo = 'S';
    p_novo.newline = '\n';

    // 2. Le o arquivo atual para a RAM
    FILE *fbin = fopen(arq_bin, "rb");
    long n_registros = 0;
    Produto *produtos = NULL;

    if (fbin) {
        fseek(fbin, 0, SEEK_END);
        long tamanho_arquivo = ftell(fbin);
        if (tamanho_arquivo > 0 && tamanho_arquivo % sizeof(Produto) == 0) {
             n_registros = tamanho_arquivo / sizeof(Produto);
             produtos = malloc((n_registros + 1) * sizeof(Produto)); // Aloca espaco para +1
             if (!produtos) { printf("ERRO ao alocar memoria.\n"); fclose(fbin); return 0; }
             fseek(fbin, 0, SEEK_SET);
             fread(produtos, sizeof(Produto), n_registros, fbin);
        }
        fclose(fbin);
    }
    if (!produtos) { // Caso o arquivo esteja vazio ou nao exista
         n_registros = 0;
         produtos = malloc(sizeof(Produto));
         if (!produtos) { printf("ERRO ao alocar memoria.\n"); return 0; }
    }

    // 3. Adiciona o novo registro e re-ordena
    produtos[n_registros] = p_novo;
    n_registros++;
    qsort(produtos, n_registros, sizeof(Produto), comparar_produto);

    // 4. Re-escreve o arquivo inteiro
    fbin = fopen(arq_bin, "wb");
    if (!fbin) { printf("ERRO ao abrir arquivo para escrita.\n"); free(produtos); return 0; }

    fwrite(produtos, sizeof(Produto), n_registros, fbin);
    fclose(fbin);
    free(produtos);

    printf("Produto %lld inserido com sucesso!\n", p_novo.product_id);
    return 1; // Retorna 1 para sinalizar que o indice precisa ser reconstruido
}

/**
 * @brief Realiza a remocao logica de um produto.
 * Ele nao apaga o registro do arquivo. Apenas encontra o registro
 * usando a pesquisa_binaria e altera o campo 'ativo' de 'S' para 'N'.
 * Esta e uma operacao muito rapida (O(log N) + escrita).
 * @return 1 se foi removido, 0 se houve erro (ex: nao encontrado).
 */
int remover_produto(const char *arq_bin) {
    printf("\n--- REMOVER PRODUTO ---\n");
    int64_t id = ler_long_long("Digite o product_id para remover: ");

    int64_t chave = id;
    long offset = pesquisa_binaria(arq_bin, sizeof(Produto), comparar_produto_chave, &chave, offsetof(Produto, ativo));

    if (offset == -1) { printf("Produto %lld nao encontrado.\n", id); return 0; }
    if (offset == -2) { printf("Produto %lld ja esta removido.\n", id); return 0; }

    // Encontrou e esta ativo (offset >= 0)
    FILE *fbin = fopen(arq_bin, "r+b"); // Abre para LEITURA e ESCRITA
    if (!fbin) { printf("ERRO ao abrir arquivo para remocao.\n"); return 0; }

    // Pula o cursor direto para o campo 'ativo' do registro encontrado
    fseek(fbin, offset + offsetof(Produto, ativo), SEEK_SET);

    // Sobrescreve apenas aquele byte
    fwrite("N", sizeof(char), 1, fbin);
    fclose(fbin);

    printf("Produto %lld removido logicamente.\n", id);
    return 1; // Retorna 1 para sinalizar que o indice precisa ser reconstruido
}

/**
 * @brief Consulta um produto usando a pesquisa binaria direta no arquivo.
 */
void consultar_produto(const char *arq_bin) {
    printf("\n--- CONSULTAR PRODUTO ---\n");
    int64_t id = ler_long_long("Digite o product_id para consultar: ");

    int64_t chave = id;
    long offset = pesquisa_binaria(arq_bin, sizeof(Produto), comparar_produto_chave, &chave, offsetof(Produto, ativo));

    if (offset == -1) { printf("Produto %lld nao encontrado.\n", id); }
    else if (offset == -2) { printf("Produto %lld existe mas foi removido.\n", id); }
    else {
        // Encontrou e esta ativo, le o registro completo
        FILE *fbin = fopen(arq_bin, "rb");
        if (!fbin) { printf("ERRO ao abrir arquivo para leitura.\n"); return; }
        fseek(fbin, offset, SEEK_SET);
        Produto p;
        fread(&p, sizeof(Produto), 1, fbin);

        // Logica de "trim" para imprimir
        char brand_trim[TAM_BRAND+1]={0};
        char category_trim[TAM_CATEGORY+1]={0};
        strncpy(brand_trim, p.brand, TAM_BRAND);
        strncpy(category_trim, p.category_alias, TAM_CATEGORY);
        for(int i = strlen(brand_trim)-1; i >=0 && brand_trim[i] == ' '; i--) brand_trim[i] = '\0';
        for(int i = strlen(category_trim)-1; i >=0 && category_trim[i] == ' '; i--) category_trim[i] = '\0';

        printf("\n--- PRODUTO ENCONTRADO ---\n");
        printf("ID: %lld\nBrand: %s\nPrice: %.2f\nCategory: %s\n",
               p.product_id, brand_trim, p.price, category_trim);
        fclose(fbin);
    }
}

// --- FUNCOES ESPECIFICAS COMPRAS ---

/**
 * @brief Le o CSV, ordena em RAM e grava o arquivo .bin inicial de compras.
 * Mesma logica do pre_processar_produtos.
 */
void pre_processar_compras(const char *csv_path, const char *bin_path) {
    printf("Pre-processando COMPRAS de %s...\n", csv_path);
    FILE *fcsv = fopen(csv_path, "r");
    if (!fcsv) { printf("ERRO: Nao foi possivel abrir CSV %s\n", csv_path); return; }

    char linha[2048];
    if (!fgets(linha, sizeof(linha), fcsv)) { fclose(fcsv); return; } // Ignora cabealho

    int capacidade = 100000, n_compras = 0;
    Compra* compras = malloc(capacidade * sizeof(Compra));
    if (!compras) { printf("ERRO: Falha ao alocar memoria.\n"); fclose(fcsv); return; }

    // 1. Le CSV para a RAM
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
        if(c.order_id > 0) { // Ignora registros sem ID de compra
            compras[n_compras++] = c;
        }
    }

    fclose(fcsv);
    printf("%d registros lidos para compras.\n", n_compras);

    // 2. Ordena na RAM
    qsort(compras, n_compras, sizeof(Compra), comparar_compra);

    // 3. Grava no .bin, pulando duplicatas
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

/**
 * @brief Le o arquivo .bin sequencialmente e imprime todas as compras ATIVAS.
 */
void mostrar_compras(const char *arq_bin) {
    FILE *fbin = fopen(arq_bin, "rb");
    if (!fbin) { printf("ERRO ao abrir %s\n", arq_bin); return; }

    Compra c;
    int contador = 0;
    printf("\n--- COMPRAS ATIVAS ---\n");
    while (fread(&c, sizeof(Compra), 1, fbin) == 1) {
        if (c.ativo == 'S') {
            // "Trim"
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

/**
 * @brief Insere uma nova compra no arquivo binario.
 * Utiliza a mesma estrategia de "ler tudo -> re-ordenar -> gravar tudo"
 * da funcao inserir_produto, para manter o arquivo ordenado.
 * Tambem valida se o product_id informado existe no arquivo de produtos.
 * @return 1 se foi inserido, 0 se houve erro.
 */
int inserir_compra(const char *arq_bin) {
    Compra c_nova;
    printf("\n--- INSERIR NOVA COMPRA ---\n");

    c_nova.order_id = ler_long_long("Digite o order_id: ");
     if (c_nova.order_id <= 0) {
        printf("ERRO: ID do pedido deve ser positivo.\n");
        return 0;
    }

    // 1. Verifica duplicidade de ID da compra
    long long chave = c_nova.order_id;
    if (pesquisa_binaria(arq_bin, sizeof(Compra), comparar_compra_chave, &chave, offsetof(Compra, ativo)) >= 0) {
        printf("ERRO: order_id %lld ja existe!\n", c_nova.order_id);
        return 0;
    }

    c_nova.product_id = ler_long_long("Digite o product_id: ");
    // 2. VALIDA CHAVE ESTRANGEIRA (Product ID)
    int64_t chave_prod = c_nova.product_id;
    if (pesquisa_binaria(ARQ_PRODUTOS_BIN, sizeof(Produto), comparar_produto_chave, &chave_prod, offsetof(Produto, ativo)) < 0) {
        printf("ERRO: product_id %lld nao encontrado ou inativo no cadastro de produtos. Insercao cancelada.\n", c_nova.product_id);
        return 0;
    }

    c_nova.user_id = ler_long_long("Digite o user_id: ");

    // 3. Valida formato da data
    while (1) {
        ler_string("Digite a data/hora (YYYY-MM-DD HH:MM:SS): ", c_nova.order_datetime, TAM_DATETIME);
        if (validar_e_formatar_data(c_nova.order_datetime, TAM_DATETIME)) {
            break;
        } else {
            printf("ERRO: Formato de data/hora invalido. Use YYYY-MM-DD HH:MM:SS.\n");
        }
    }

    c_nova.quantity = ler_inteiro("Digite a quantidade: ");
     if (c_nova.quantity <= 0) {
        printf("ERRO: Quantidade deve ser positiva.\n");
        return 0;
    }

    pad_string(c_nova.order_datetime, TAM_DATETIME);
    c_nova.ativo = 'S';
    c_nova.newline = '\n';

    // 4. Le arquivo atual para RAM
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

    // 5. Adiciona e re-ordena
    compras[n_registros] = c_nova;
    n_registros++;
    qsort(compras, n_registros, sizeof(Compra), comparar_compra);

    // 6. Re-escreve arquivo
    fbin = fopen(arq_bin, "wb");
    if (!fbin) { printf("ERRO ao abrir arquivo para escrita.\n"); free(compras); return 0; }

    fwrite(compras, sizeof(Compra), n_registros, fbin);
    fclose(fbin);
    free(compras);

    printf("Compra %lld inserida com sucesso!\n", c_nova.order_id);
    return 1; // Sinaliza para reconstruir indice
}

/**
 * @brief Realiza a remocao logica de uma compra (marca ativo = 'N').
 * Mesma logica do remover_produto.
 * @return 1 se foi removido, 0 se houve erro.
 */
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
    return 1; // Sinaliza para reconstruir indice
}

/**
 * @brief Consulta uma compra usando a pesquisa binaria direta no arquivo.
 */
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

        // "Trim"
        char datetime_trim[TAM_DATETIME+1]={0};
        strncpy(datetime_trim, c.order_datetime, TAM_DATETIME);
        for(int i = strlen(datetime_trim)-1; i >=0 && datetime_trim[i] == ' '; i--) datetime_trim[i] = '\0';

        printf("\n--- COMPRA ENCONTRADA ---\n");
        printf("Order ID: %lld\nProduct ID: %lld\nUser ID: %lld\nQuantity: %d\nDate: %s\n",
               c.order_id, c.product_id, c.user_id, c.quantity, datetime_trim);
        fclose(fbin);
    }
}

// --- CONSULTAS COM INDICE ---

/**
 * @brief Consulta um produto usando o arquivo de indice parcial.
 * ETAPA 1: Carrega o arquivo .idx (pequeno) para a RAM.
 * ETAPA 2: Faz uma busca binaria no array de indices (RAM) para achar o BLOCO
 * onde o registro *deveria* estar.
 * ETAPA 3: Da fseek no arquivo .bin (grande) para o inicio daquele bloco.
 * ETAPA 4: Faz uma busca SEQUENCIAL lendo no maximo 'BLOCO_INDICE' registros
 * dentro daquele bloco ate achar a chave.
 */
void consultar_produto_com_indice(const char *arq_indice, const char *arq_dados) {
    printf("\n--- CONSULTAR PRODUTO COM INDICE ---\n");
    int64_t id = ler_long_long("Digite o product_id para buscar: ");

    // ETAPA 1: Carrega o indice para a RAM
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

    // ETAPA 2: Busca binaria no indice (RAM) para achar o bloco
    int inicio = 0, fim = n_indices - 1;
    int idx_bloco = -1;
    while (inicio <= fim) {
        int meio = inicio + (fim - inicio) / 2;
        if (indices[meio].chave <= id) {
            // Encontramos um bloco cuja chave e <= a buscada.
            // Este e um *candidato* a ser o bloco certo.
            idx_bloco = meio;
            inicio = meio + 1; // Tenta achar um bloco "mais proximo"
        } else {
            fim = meio - 1;
        }
    }

    if (idx_bloco == -1) {
        // ID buscado e menor que a chave do primeiro bloco
        printf("Produto %lld nao encontrado (fora do range do indice).\n", id);
        free(indices);
        return;
    }

    // ETAPA 3: Acessa o arquivo de dados
    FILE *f_dados = fopen(arq_dados, "rb");
    if (!f_dados) { printf("ERRO ao abrir arquivo de dados %s.\n", arq_dados); free(indices); return; }

    // Pula para o inicio do bloco encontrado
    fseek(f_dados, indices[idx_bloco].offset, SEEK_SET);

    // ETAPA 4: Busca sequencial dentro do bloco
    Produto p;
    int encontrado = 0;
    for (int i = 0; i < BLOCO_INDICE; i++) {
        if (fread(&p, sizeof(Produto), 1, f_dados) != 1) break; // Fim do arquivo

        if (p.product_id == id) {
            if (p.ativo == 'S') {
                // "Trim"
                char brand_trim[TAM_BRAND+1]={0};
                char category_trim[TAM_CATEGORY+1]={0};
                strncpy(brand_trim, p.brand, TAM_BRAND);
                strncpy(category_trim, p.category_alias, TAM_CATEGORY);
                for(int j = strlen(brand_trim)-1; j >=0 && brand_trim[j] == ' '; j--) brand_trim[j] = '\0';
                for(int j = strlen(category_trim)-1; j >=0 && category_trim[j] == ' '; j--) category_trim[j] = '\0';

                printf("\n--- PRODUTO ENCONTRADO (via indice) ---\n");
                printf("ID: %lld | Brand: %s | Price: %.2f | Category: %s\n",
                       p.product_id, brand_trim, p.price, category_trim);
                encontrado = 1;
            } else {
                printf("Produto %lld existe mas foi removido.\n", id);
                encontrado = 2; // Encontrado mas removido
            }
            break; // Para a busca sequencial
        }

        // Otimizacao: Se passamos da chave, nao precisamos ler o resto do bloco
        if (p.product_id > id) break;
    }

    if (encontrado == 0) {
        printf("Produto %lld nao encontrado no bloco verificado.\n", id);
    }

    fclose(f_dados);
    free(indices);
}

/**
 * @brief Consulta uma compra usando o arquivo de indice parcial.
 * Mesma logica da 'consultar_produto_com_indice'.
 */
void consultar_compra_com_indice(const char *arq_indice, const char *arq_dados) {
    printf("\n--- CONSULTAR COMPRA COM INDICE ---\n");
    long long id = ler_long_long("Digite o order_id para buscar: ");

    // ETAPA 1: Carrega o indice para a RAM
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

    // ETAPA 2: Busca binaria no indice (RAM) para achar o bloco
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

    // ETAPA 3: Acessa o arquivo de dados
    FILE *f_dados = fopen(arq_dados, "rb");
    if (!f_dados) { printf("ERRO ao abrir arquivo de dados %s.\n", arq_dados); free(indices); return; }

    fseek(f_dados, indices[idx_bloco].offset, SEEK_SET);

    // ETAPA 4: Busca sequencial dentro do bloco
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

// --- CONSULTAS ESPECIFICAS ---

/**
 * @brief Encontra o produto mais caro fazendo uma varredura sequencial
 * no arquivo de produtos.
 */
void consulta_produto_mais_caro() {
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
        // "Trim"
        char brand_trim[TAM_BRAND+1]={0};
        char category_trim[TAM_CATEGORY+1]={0};
        strncpy(brand_trim, mais_caro.brand, TAM_BRAND);
        strncpy(category_trim, mais_caro.category_alias, TAM_CATEGORY);
        for(int i = strlen(brand_trim)-1; i >=0 && brand_trim[i] == ' '; i--) brand_trim[i] = '\0';
        for(int i = strlen(category_trim)-1; i >=0 && category_trim[i] == ' '; i--) category_trim[i] = '\0';

        printf("\n--- PRODUTO MAIS CARO ---\n");
        printf("ID: %lld | Brand: %s | Price: %.2f | Category: %s\n",
               mais_caro.product_id, brand_trim, mais_caro.price, category_trim);
    } else {
        printf("Nenhum produto ativo encontrado.\n");
    }
}

/**
 * @brief Calcula o valor total vendido.
 * Esta funcao simula um "JOIN" de banco de dados manualmente.
 * 1. Le o arquivo de compras sequencialmente.
 * 2. Para cada compra ativa, ela usa a 'pesquisa_binaria' (rapida, O(logN))
 * para encontrar o preco do produto correspondente no arquivo de produtos.
 * 3. Multiplica preco * quantidade e soma ao total.
 */
void consulta_valor_total_vendido() {
    FILE *f_comp = fopen(ARQ_COMPRAS_BIN, "rb");
    if (!f_comp) { printf("ERRO ao abrir arquivo de compras %s\n", ARQ_COMPRAS_BIN); return; }

    printf("Calculando valor total vendido (pode demorar)...\n");

    double total = 0;
    Compra c;
    int compras_contadas = 0;
    int produtos_nao_encontrados = 0;

    // 1. Varre o arquivo de compras
    while (fread(&c, sizeof(Compra), 1, f_comp) == 1) {
        if (c.ativo != 'S') continue;

        int64_t id_produto_busca = (int64_t)c.product_id;

        // 2. Para cada compra, faz uma pesquisa binaria no arquivo de produtos
        long offset_prod = pesquisa_binaria(
            ARQ_PRODUTOS_BIN, sizeof(Produto), comparar_produto_chave,
            &id_produto_busca, offsetof(Produto, ativo)
        );

        if (offset_prod >= 0) {
            // Se encontrou o produto e ele esta ativo, busca o preco
            FILE *f_prod_leitura = fopen(ARQ_PRODUTOS_BIN, "rb");
            if (f_prod_leitura) {
                Produto p_temp;
                fseek(f_prod_leitura, offset_prod, SEEK_SET);
                if(fread(&p_temp, sizeof(Produto), 1, f_prod_leitura) == 1) {
                    // 3. Soma ao total
                    total += p_temp.price * c.quantity;
                    compras_contadas++;
                } else {
                    produtos_nao_encontrados++;
                }
                fclose(f_prod_leitura);
            } else {
                 produtos_nao_encontrados++;
            }
        } else {
             // Produto nao encontrado ou removido (offset -1 ou -2)
             produtos_nao_encontrados++;
        }
    }
    fclose(f_comp);

    printf("\n--- VALOR TOTAL VENDIDO ---\n");
    printf("Total: R$ %.2f\n", total);
    printf("(Calculado a partir de %d compras validas. %d produtos/compras nao encontrados/invalidos/removidos)\n",
           compras_contadas, produtos_nao_encontrados);
}

// --- MENUS ---
void menu_consultas() {
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
    int opcao;
    // Flag para controlar a necessidade de reconstruir o indice
    int reconstruir = 0;

    // Verifica se os arquivos .bin e .idx existem na inicializacao
    FILE *f = fopen(ARQ_PRODUTOS_BIN, "rb");
    if (!f) {
        printf("Arquivo %s nao encontrado. Pre-processando...\n", ARQ_PRODUTOS_BIN);
        pre_processar_produtos(ARQ_CSV, ARQ_PRODUTOS_BIN);
        reconstruir = 1; // Precisa criar o indice pela primeira vez
    } else {
        fclose(f);
    }

    f = fopen(ARQ_PRODUTOS_IDX, "rb");
    if (!f) {
        if (fopen(ARQ_PRODUTOS_BIN,"rb") != NULL) { // So reconstroi se o .bin existir
             printf("Arquivo de indice %s nao encontrado.\n", ARQ_PRODUTOS_IDX);
             reconstruir = 1; // Precisa criar o indice
        }
    } else {
        fclose(f);
    }

    do {
        // Se a flag 'reconstruir' foi ativada (na inicializacao ou
        // apos uma insercao/remocao), o indice e recriado.
        if (reconstruir) {
            printf("\n(Sistema: Reconstruindo indice %s...)\n", ARQ_PRODUTOS_IDX);
            criar_indice(ARQ_PRODUTOS_BIN, ARQ_PRODUTOS_IDX, sizeof(Produto), sizeof(IndiceProduto),
                         extrai_chave_produto, NULL, 0, offsetof(Produto, ativo));
            reconstruir = 0; // Zera a flag
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
                // Se a insercao foi bem-sucedida, ativa a flag
                if (inserir_produto(ARQ_PRODUTOS_BIN)) reconstruir = 1;
                break;
            case 3:
                // Se a remocao foi bem-sucedida, ativa a flag
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
                    reconstruir = 1; // Ativa a flag
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
    int opcao;
    // Flag para controlar a necessidade de reconstruir o indice
    int reconstruir = 0;

    // Verifica se os arquivos .bin e .idx existem na inicializacao
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
        // Logica de reconstrucao, igual ao menu_produtos
        if (reconstruir) {
            printf("\n(Sistema: Reconstruindo indice %s...)\n", ARQ_COMPRAS_IDX);
            criar_indice(ARQ_COMPRAS_BIN, ARQ_COMPRAS_IDX, sizeof(Compra), sizeof(IndiceCompra),
                         NULL, extrai_chave_compra, 1, offsetof(Compra, ativo));
            reconstruir = 0; // Zera a flag
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
                // Se a insercao foi bem-sucedida, ativa a flag
                if (inserir_compra(ARQ_COMPRAS_BIN)) reconstruir = 1;
                break;
            case 3:
                // Se a remocao foi bem-sucedida, ativa a flag
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
                    reconstruir = 1; // Ativa a flag
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
