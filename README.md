# Trabalho I: Arquivos de Dados com Índices Parciais
## Algoritmos e Estruturas de Dados II

**Alunos:** Nícolas Grillo Groff e Eduardo Santiago Bearzi

## Objetivo

[cite_start]Implementação de um sistema de gerenciamento de arquivos de dados utilizando organização sequencial-indexada[cite: 3]. [cite_start]O projeto envolve a criação de arquivos de dados binários ordenados e arquivos de índice parcial [cite: 3][cite_start], além de funcionalidades para consulta, inserção e remoção de registros[cite: 50].

## Contexto do Trabalho

[cite_start]O sistema gerencia dados extraídos de um dataset público de compras em uma loja online de joias (dez/2018 a dez/2021)[cite: 20]. [cite_start]O dataset original, `jewelry.csv`[cite: 22], contém informações detalhadas de cada item comprado por pedido.

[cite_start]**Dataset Original:** [E-commerce purchase history from jewelry store](https://www.kaggle.com/datasets/mkechinov/ecommerce-purchase-history-from-jewelry-store/data) [cite: 22]

## Arquivos Gerados

O programa utiliza o `jewelry.csv` como entrada e gera/manipula os seguintes arquivos binários:

### 1. Arquivos de Dados (`.bin`)

[cite_start]Contêm registros de **tamanho fixo** [cite: 37][cite_start], ordenados pela chave primária[cite: 27]. [cite_start]Campos de texto são preenchidos com espaços à direita (`pad_string`) para manter o tamanho fixo[cite: 38]. Cada registro termina com `\n`[cite: 39]. A remoção é implementada logicamente através do campo `ativo` ('S' ou 'N')[cite: 52].

* **`produtos.bin`:** Armazena um catálogo de produtos únicos extraídos do CSV.
    * **Chave Primária:** `product_id` (int), permite valores positivos, negativos ou zero (desde que únicos).
    * [cite_start]**Ordenação:** Ordenado crescentemente por `product_id`[cite: 32].
    * **Estrutura do Registro (`Produto`):**
        * `int product_id`
        * `char brand[50]`
        * `double price`
        * `char category_alias[100]`
        * `char ativo`
        * `char newline`

* **`compras.bin`:** Armazena uma lista de pedidos únicos extraídos do CSV, baseados no `order_id`. *Nota: Apenas `order_id`s positivos são mantidos do CSV, e apenas a primeira linha encontrada para cada `order_id` único (após ordenação) é gravada para simplificar a chave.*
    * **Chave Primária:** `order_id` (long long).
    * [cite_start]**Ordenação:** Ordenado crescentemente por `order_id`[cite: 32].
    * **Estrutura do Registro (`Compra`):**
        * `long long order_id`
        * `long long product_id`
        * `long long user_id`
        * `char order_datetime[30]` (Formato: `YYYY-MM-DD HH:MM:SS UTC`)
        * `int quantity`
        * `char ativo`
        * `char newline`

### 2. Arquivos de Índice (`.idx`)

[cite_start]Arquivos de índice parcial para a chave primária de cada arquivo de dados[cite: 46, 48]. Um par `(chave, offset)` é armazenado para cada bloco de `BLOCO_INDICE` (definido como 100) registros *ativos* no arquivo de dados.

* **`produtos_idx.bin`:** Índice para `produtos.bin`.
    * **Estrutura:** Sequência de `IndiceProduto { int chave; long offset; }`.

* **`compras_idx.bin`:** Índice para `compras.bin`.
    * **Estrutura:** Sequência de `IndiceCompra { long long chave; long offset; }`.

## Funcionalidades Implementadas

O programa apresenta um menu principal com acesso aos módulos de gerenciamento de **Produtos** e **Compras**, e um módulo de **Consultas Específicas**.

### Módulos de Gerenciamento (Produtos e Compras):

1.  **Mostrar todos:** Exibe todos os registros ativos (`ativo == 'S'`) do respectivo arquivo `.bin`.
2.  **Inserir:** Permite adicionar um novo registro.
    * Verifica se a chave primária já existe e está ativa.
    * *Para Compras:* Valida se o `product_id` informado existe e está ativo no `produtos.bin`. Valida o formato da data/hora (`YYYY-MM-DD HH:MM:SS`) e adiciona " UTC" automaticamente. Valida se a quantidade é positiva.
    * O arquivo `.bin` é lido para a memória, o novo registro é adicionado, o array é reordenado com `qsort`, e o arquivo `.bin` é reescrito por completo.
    * [cite_start]O índice correspondente é marcado para reconstrução automática[cite: 53].
3.  [cite_start]**Remover:** Permite marcar um registro como inativo (remoção lógica), alterando o campo `ativo` para 'N'[cite: 52].
    * Utiliza a `pesquisa_binaria` para encontrar o registro.
    * [cite_start]O índice correspondente é marcado para reconstrução automática[cite: 53].
4.  [cite_start]**Consultar (binária):** Busca um registro pela chave primária utilizando a função genérica `pesquisa_binaria`, que opera diretamente no arquivo `.bin` com `fseek`[cite: 44, 45].
5.  [cite_start]**Consultar (com índice):** Busca um registro pela chave primária utilizando o índice parcial[cite: 49].
    * O arquivo `.idx` é carregado na RAM.
    * [cite_start]Realiza busca binária no índice em RAM para encontrar o bloco correto[cite: 49].
    * [cite_start]Utiliza `fseek` para posicionar no bloco dentro do arquivo `.bin`[cite: 49].
    * Realiza busca sequencial *apenas* dentro daquele bloco (até `BLOCO_INDICE` registros).
6.  **Recriar do CSV:** Apaga o arquivo `.bin` atual e o recria a partir do `jewelry.csv`.
    * [cite_start]Lê o CSV, carrega para a RAM, ordena com `qsort`[cite: 33].
    * Remove duplicatas baseadas na chave primária (`product_id` para produtos, `order_id` para compras).
    * Grava o novo arquivo `.bin`.
    * O índice correspondente é marcado para reconstrução automática. Pede confirmação antes de executar.

### Consultas Específicas:

1.  **Produto mais caro:** Varre o arquivo `produtos.bin` sequencialmente para encontrar o produto ativo com o maior preço.
2.  **Valor total vendido:** Itera sobre o arquivo `compras.bin`. [cite_start]Para cada compra ativa, busca o preço do produto correspondente no `produtos.bin` usando `pesquisa_binaria` (sem carregar a lista de produtos na RAM ) e acumula o valor (`preco * quantidade`).

## Como Compilar e Executar

### Pré-requisitos
* Compilador C (GCC recomendado).
* Arquivo `jewelry.csv` na mesma pasta do código fonte ou executável.

### Compilação (Exemplo com GCC)
```bash
gcc arquivo.c -o trabalho_aed2 -lm -Wall -Wextra -pedantic
