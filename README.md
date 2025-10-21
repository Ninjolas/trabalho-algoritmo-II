# Trabalho I: Arquivos de Dados com Índices Parciais
## Algoritmos e Estruturas de Dados II

**Aluno(s): Nícolas Grillo Groff e Eduardo Santiago Bearzi** [Seu Nome ou Nomes da Dupla]

## Objetivo

[cite_start]Implementação de um sistema de gerenciamento de arquivos de dados utilizando organização sequencial-indexada, incluindo a criação de arquivos de dados binários ordenados, índices parciais, e operações de consulta, inserção e remoção[cite: 3].

## Contexto do Trabalho

[cite_start]O projeto utiliza um dataset público de compras realizadas em uma loja online de joias de médio porte, abrangendo o período de dezembro de 2018 a dezembro de 2021[cite: 20]. [cite_start]Cada linha do dataset original representa um item de produto comprado[cite: 21].

[cite_start]**Dataset Original:** [E-commerce purchase history from jewelry store](https://www.kaggle.com/datasets/mkechinov/ecommerce-purchase-history-from-jewelry-store/data) [cite: 22]

## Arquivos Gerados

O programa processa o arquivo `jewelry.csv` e gera os seguintes arquivos binários:

### 1. Arquivos de Dados (`.bin`)

[cite_start]Os arquivos de dados contêm registros de **tamanho fixo** [cite: 37][cite_start], ordenados pelo campo chave[cite: 27, 32]. [cite_start]Campos textuais são preenchidos com espaços à direita para garantir o tamanho fixo[cite: 38]. [cite_start]Cada registro termina com `\n`[cite: 39]. A remoção é lógica (campo `ativo`).

* **`produtos.bin`:** Catálogo de produtos únicos extraídos do CSV.
    * **Chave Primária:** `product_id` (int)
    * **Ordenação:** Ordenado por `product_id`.
    * **Estrutura do Registro:**
        * `int product_id`
        * `char brand[50]`
        * `double price`
        * `char category_alias[100]`
        * `char ativo` ('S' ou 'N')
        * `char newline` ('\n')

* **`compras.bin`:** Lista de pedidos únicos (baseado no `order_id`) extraídos do CSV. *Observação: linhas do CSV com o mesmo `order_id` são tratadas como um único pedido, e apenas a primeira ocorrência é mantida durante o pré-processamento para simplificar a chave primária.*
    * **Chave Primária:** `order_id` (long long)
    * **Ordenação:** Ordenado por `order_id`.
    * **Estrutura do Registro:**
        * `long long order_id`
        * `long long product_id`
        * `long long user_id`
        * `char order_datetime[30]` (Formato: `YYYY-MM-DD HH:MM:SS UTC`)
        * `int quantity`
        * `char ativo` ('S' ou 'N')
        * `char newline` ('\n')

### 2. Arquivos de Índice (`.idx`)

[cite_start]Arquivos de índice parcial para a chave primária de cada arquivo de dados[cite: 46, 48]. Contêm um par (chave, offset) para cada bloco de `BLOCO_INDICE` (definido como 100 no código) registros ativos no arquivo de dados correspondente.

* **`produtos_idx.bin`:** Índice para `produtos.bin`.
    * **Estrutura:** Sequência de `IndiceProduto { int chave; long offset; }`.

* **`compras_idx.bin`:** Índice para `compras.bin`.
    * **Estrutura:** Sequência de `IndiceCompra { long long chave; long offset; }`.

## Funcionalidades Implementadas

O programa oferece um menu principal para gerenciar Produtos ou Compras, com as seguintes opções para cada módulo:

1.  **Mostrar Todos:** Lista todos os registros ativos do arquivo `.bin` correspondente.
2.  **Inserir:** Permite adicionar um novo registro. O arquivo `.bin` é recarregado, reordenado e reescrito. [cite_start]O índice é marcado para reconstrução[cite: 53].
3.  [cite_start]**Remover:** Marca um registro como inativo (remoção lógica)[cite: 52]. O índice é marcado para reconstrução.
4.  [cite_start]**Consultar (Binária):** Busca um registro pelo ID usando pesquisa binária diretamente no arquivo `.bin`[cite: 44, 45].
5.  [cite_start]**Consultar (Com Índice):** Busca um registro pelo ID usando o arquivo de índice `.idx` (com busca binária no índice em RAM) e, em seguida, busca sequencial no bloco correspondente do arquivo `.bin`[cite: 49].
6.  **Recriar do CSV:** Apaga o arquivo `.bin` atual e o recria a partir do `jewelry.csv` original, reordenando e removendo duplicatas. O índice também é recriado.

[cite_start]Além disso, há um menu de **Consultas Específicas** [cite: 31] que responde às seguintes perguntas:
* Qual o produto mais caro?
* Qual o valor total vendido (calculado buscando o preço de cada produto vendido)?

## Como Compilar e Executar

### Pré-requisitos
* Compilador C (como GCC)
* Arquivo `jewelry.csv` na mesma pasta do código fonte.

### Compilação
```bash
gcc teu_arquivo.c -o programa -lm -Wall -Wextra -pedantic# trabalho-algoritmo-II
