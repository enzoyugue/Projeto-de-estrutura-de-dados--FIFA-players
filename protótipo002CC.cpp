/*
 * ============================================================
 *  SISTEMA DE GERENCIAMENTO DE JOGADORES FIFA 22
 *  Projeto de Estrutura de Dados 2026 — UNESP
 *  Aluno: Enzo Asseituno Yugue
 * ============================================================
 *  Estruturas de Dados Utilizadas:
 *
 *  1. Lista Encadeada Simples
 *     Repositório primário de todos os dados. Permite varredura
 *     sequencial O(N) para filtragens, estatísticas e Top 11.
 *
 *  2. Tabela Hash com Separate Chaining
 *     Motor de acesso direto aos registros. Tamanho primo (20011)
 *     minimiza colisões, garantindo busca amortizada O(1).
 *
 *  3. Pilha (Stack — LIFO)
 *     Usada para inverter a exibição do Top 11 por liga.
 *     Captura jogadores em ordem crescente e desempilha em
 *     ordem decrescente com custo O(1) por operação.
 *
 *  4. Árvore Trie (Árvore Prefixada)
 *     Motor de autocompletar: busca por prefixo em O(L),
 *     onde L é o comprimento da string digitada.
 *     Remoção é lógica (fimDePalavra = 0) — não libera memória,
 *     preservando caminhos partilhados por outros nomes.
 *
 *  5. Counting Bloom Filter (Otimização avançada)
 *     Filtro probabilístico com vetor de contadores short (16 bits).
 *     Substitui o vetor de bits do Bloom clássico, viabilizando
 *     remoção por decremento. Custo: ~390 KB vs ~25 KB do clássico.
 *     Atua como "escudo": rejeita chaves inexistentes em O(1)
 *     antes de consultar Hash ou Trie.
 *
 *  Módulo de Simulação: 5 restrições adversas injetáveis sob demanda
 *  (R2, R9, R12, R16, R21) para benchmark em condições hostis.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   /* clock(), CLOCKS_PER_SEC — medição de tempo de CPU */

/* ============================================================
 *  CONSTANTES GLOBAIS
 * ============================================================ */

/* Número de buckets da Tabela Hash.
 * Valor primo reduz colisões: o operador módulo (%) sobre primos
 * distribui os restos de forma mais uniforme, evitando clustering. */
#define HASH_SIZE 20011

/* Número de posições lógicas do Counting Bloom Filter.
 * Cada posição armazena um contador short (2 bytes), portanto
 * o consumo total é: 200.000 * 2 bytes = ~390 KB. */
#define BLOOM_SIZE 200000

/* Tamanho físico do array de contadores do Bloom Filter.
 * No Counting Bloom Filter, BLOOM_ARRAY_SIZE == BLOOM_SIZE,
 * pois cada posição lógica corresponde a exatamente 1 contador. */
#define BLOOM_ARRAY_SIZE BLOOM_SIZE


/* ============================================================
 *  STRUCTS DE DOMÍNIO
 * ============================================================ */

/* Representa um jogador com os campos extraídos do CSV players_22. */
typedef struct {
    char nome[100];
    char time[100];
    char liga[50];
    int  idade;
    int  overall;   /* pontuação geral do jogador (0–99) */
    int  salario;   /* salário em EUR */
} Jogador;


/* ============================================================
 *  STRUCT DE CONTROLO DA SIMULAÇÃO
 *  Cada campo é uma flag (0 = desligado, 1 = ligado) que ativa
 *  uma restrição artificial para fins de benchmark pedagógico.
 * ============================================================ */
typedef struct {
    int r2_memoria;    /* R2  — limita o carregamento a 5.000 registros */
    int r9_cpu;        /* R9  — insere busy-wait antes de cada busca na Hash */
    int r12_latencia;  /* R12 — adiciona atraso de 1 ms simulando rede lenta */
    int r16_dados;     /* R16 — corrompe 10% dos registros (Overall = 999) */
    int r21_algoritmo; /* R21 — desliga Hash e Bloom, forçando busca linear O(N) */
} Simulacao;


/* ============================================================
 *  STRUCTS DAS ESTRUTURAS DE DADOS
 * ============================================================ */

/* Nó da Pilha (Stack — LIFO: Last In, First Out).
 * Usada para inverter a exibição do Top 11 por liga:
 * empilha em ordem crescente, desempilha em decrescente. */
typedef struct NoPilha {
    Jogador         jogador;
    struct NoPilha* prox;   /* ponteiro para o nó anterior (topo anterior) */
} NoPilha;

/* Nó da Lista Encadeada Simples.
 * Repositório primário: guarda todos os jogadores carregados do CSV.
 * Inserção em O(1) na cabeça; busca em O(N) por varredura sequencial. */
typedef struct NoLista {
    Jogador         jogador;
    struct NoLista* prox;
} NoLista;

/* Nó da Tabela Hash (Separate Chaining).
 * Cada bucket é uma lista encadeada de nós que colidiram no mesmo índice.
 * O fator de carga ideal mantém as cadeias curtas, preservando O(1) amortizado. */
typedef struct NoHash {
    Jogador        jogador;
    struct NoHash* prox;
} NoHash;

/* Nó da Árvore Trie (Árvore Prefixada).
 * Cada nó possui 128 ponteiros filho — um por caractere ASCII.
 * fimDePalavra = 1 indica que a string que chega até este nó é uma chave válida.
 * Custo espacial: cada nó ocupa sizeof(NoTrie) = 128 * 8 bytes (ponteiros) + int ≈ 1032 bytes. */
typedef struct NoTrie {
    struct NoTrie* filhos[128];
    int            fimDePalavra;
} NoTrie;

/* Counting Bloom Filter.
 * Substitui o vetor de bits (unsigned char) do Bloom clássico por contadores short.
 * Isso permite remoção segura: ao remover um elemento, decrementam-se os contadores
 * das 3 posições calculadas pelas funções hash. Sem risco de corromper outros elementos,
 * pois o contador só chega a zero quando nenhum elemento mais usa aquela posição.
 * Trade-off: ~390 KB em vez de ~25 KB do filtro clássico de bits. */
typedef struct {
    short contadores[BLOOM_ARRAY_SIZE];
} BloomFilter;

/* Estatísticas acumuladas por liga (5 grandes ligas europeias). */
typedef struct {
    char      nome_liga[50];
    long long soma_overall;
    long long soma_idade;
    long long soma_salario;
    int       contador;
} InfoLiga;

/* Estatísticas globais de todos os jogadores do sistema. */
typedef struct {
    long long soma_overall;
    long long soma_idade;
    long long soma_salario;
    int       contador;
} EstatisticasGlobais;


/* ============================================================
 *  DECLARAÇÕES ANTECIPADAS (Forward Declarations)
 *  Necessárias em C quando uma função é chamada antes de sua
 *  definição no arquivo-fonte.
 * ============================================================ */
void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie,
                     BloomFilter* bf, EstatisticasGlobais* stats,
                     InfoLiga* ligas, Simulacao* sim);
void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie,
                      BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas);
int  removerDaLista(NoLista** cabeca, const char* nome);
void exibirEstatisticas(EstatisticasGlobais* stats, InfoLiga* ligas);
long contarNosTrie(NoTrie* no);
void analisarColisoesHash(NoHash** tabela);
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie);
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela, NoTrie* trie,
                               BloomFilter* bf, int n);
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel);


/* ============================================================
 *  IMPLEMENTAÇÃO: PILHA (STACK — LIFO)
 * ============================================================ */

/* Empilha um jogador no topo da pilha.
 * Custo: O(1) — apenas alocação e ajuste de ponteiros. */
void push(NoPilha** topo, Jogador jog) {
    NoPilha* novo = (NoPilha*)malloc(sizeof(NoPilha));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox = *topo;  /* novo nó aponta para o topo anterior */
    *topo = novo;        /* topo agora aponta para o novo nó */
}

/* Desempilha e retorna o jogador do topo da pilha.
 * Custo: O(1) — apenas leitura e ajuste de ponteiros.
 * Propriedade LIFO: o último empilhado é o primeiro a sair,
 * invertendo automaticamente a ordem de inserção. */
Jogador pop(NoPilha** topo) {
    Jogador jog = {"", "", "", 0, 0, 0};
    if (*topo == NULL) return jog;
    NoPilha* temp = *topo;
    jog = temp->jogador;
    *topo = temp->prox;
    free(temp);
    return jog;
}


/* ============================================================
 *  IMPLEMENTAÇÃO: LISTA ENCADEADA SIMPLES
 * ============================================================ */

/* Insere um jogador na cabeça da lista.
 * Custo: O(1) — inserção sempre no início, sem percorrer a lista. */
void inserirLista(NoLista** cabeca, Jogador jog) {
    NoLista* novo = (NoLista*)malloc(sizeof(NoLista));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox = *cabeca;
    *cabeca = novo;
}

/* Libera toda a memória alocada pela lista encadeada.
 * Percorre nó a nó, liberando cada um antes de avançar. */
void liberarLista(NoLista** cabeca) {
    NoLista* atual = *cabeca;
    while (atual != NULL) {
        NoLista* temp = atual;
        atual = atual->prox;
        free(temp);
    }
    *cabeca = NULL;
}

/* Remove um jogador da lista pelo nome.
 * Custo: O(N) no pior caso — percorre toda a lista até encontrar.
 * Retorna 1 se removido, 0 se não encontrado. */
int removerDaLista(NoLista** cabeca, const char* nome) {
    NoLista* atual = *cabeca;
    NoLista* anterior = NULL;
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0) {
            /* Reconecta os vizinhos, excluindo o nó atual */
            if (anterior == NULL) *cabeca = atual->prox;
            else anterior->prox = atual->prox;
            free(atual);
            return 1;
        }
        anterior = atual;
        atual = atual->prox;
    }
    return 0;
}


/* ============================================================
 *  IMPLEMENTAÇÃO: TABELA HASH COM SEPARATE CHAINING
 * ============================================================ */

/* Função de dispersão djb2 adaptada.
 * Combina deslocamento de bits e adição para gerar hashes bem distribuídos.
 * O módulo por HASH_SIZE (primo) garante distribuição quase uniforme. */
unsigned int funcaoHash(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

/* Insere um jogador na tabela hash pelo nome.
 * Colisões são resolvidas por encadeamento (chaining):
 * o novo nó é inserido na cabeça da lista do bucket. */
void inserirHash(NoHash** tabela, Jogador jog) {
    unsigned int idx = funcaoHash(jog.nome);
    NoHash* novo = (NoHash*)malloc(sizeof(NoHash));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox = tabela[idx];
    tabela[idx] = novo;
}

/* Busca um jogador na tabela hash.
 * Custo amortizado: O(1) — com fator de carga baixo, as cadeias são curtas.
 * Contém ganchos para as simulações R9 (CPU busy-wait) e R12 (latência). */
Jogador* buscarHash(NoHash** tabela, const char* nome, Simulacao* sim) {
    /* [SIMULAÇÃO R9] — Estrangulamento de CPU.
     * Injeta 50.000 operações de ponto flutuante antes da busca real,
     * simulando um processador sobrecarregado (thermal throttling).
     * Demonstra que O(1) depende de ciclos de CPU disponíveis. */
    if (sim != NULL && sim->r9_cpu) {
        volatile double dummy = 0.0;
        for (int i = 0; i < 50000; i++) dummy += 1.1;
    }

    /* [SIMULAÇÃO R12] — Latência de rede/disco.
     * Força a thread a aguardar 1 ms antes de consultar o dado,
     * simulando um servidor remoto com alta latência.
     * O Counting Bloom Filter é imune a este atraso (opera localmente). */
    if (sim != NULL && sim->r12_latencia) {
        clock_t start = clock();
        while (clock() < start + (1 * CLOCKS_PER_SEC / 1000));
    }

    unsigned int idx = funcaoHash(nome);
    NoHash* atual = tabela[idx];
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0) return &(atual->jogador);
        atual = atual->prox;
    }
    return NULL;
}

/* Remove um jogador da tabela hash pelo nome.
 * Percorre a cadeia do bucket e reconecta os vizinhos ao excluir o nó. */
int removerHash(NoHash** tabela, const char* nome) {
    unsigned int idx = funcaoHash(nome);
    NoHash* atual = tabela[idx];
    NoHash* ant = NULL;
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0) {
            if (ant == NULL) tabela[idx] = atual->prox;
            else ant->prox = atual->prox;
            free(atual);
            return 1;
        }
        ant = atual;
        atual = atual->prox;
    }
    return 0;
}

/* Libera toda a memória alocada pela tabela hash.
 * Percorre todos os HASH_SIZE buckets e libera cada cadeia. */
void liberarHash(NoHash** tabela) {
    for (int i = 0; i < HASH_SIZE; i++) {
        NoHash* atual = tabela[i];
        while (atual != NULL) {
            NoHash* temp = atual;
            atual = atual->prox;
            free(temp);
        }
        tabela[i] = NULL;
    }
}


/* ============================================================
 *  IMPLEMENTAÇÃO: ÁRVORE TRIE (ÁRVORE PREFIXADA)
 * ============================================================ */

/* Cria e inicializa um novo nó da Trie.
 * Todos os 128 filhos começam como NULL (sem filhos).
 * fimDePalavra = 0 indica que este nó não termina nenhuma chave válida. */
NoTrie* criarNoTrie() {
    NoTrie* n = (NoTrie*)malloc(sizeof(NoTrie));
    if (n) {
        n->fimDePalavra = 0;
        for (int i = 0; i < 128; i++) n->filhos[i] = NULL;
    }
    return n;
}

/* Insere uma string na Trie, criando nós conforme necessário.
 * Custo: O(L), onde L é o comprimento da string.
 * Cada caractere mapeia diretamente para um índice filho (valor ASCII). */
void inserirTrie(NoTrie* raiz, const char* chave) {
    NoTrie* atual = raiz;
    for (int i = 0; chave[i] != '\0'; i++) {
        unsigned char c = (unsigned char)chave[i];
        if (c >= 128) continue;  /* ignora caracteres fora da tabela ASCII básica */
        if (atual->filhos[c] == NULL) atual->filhos[c] = criarNoTrie();
        atual = atual->filhos[c];
    }
    atual->fimDePalavra = 1;  /* marca o nó terminal como fim de palavra válida */
}

/* Remove logicamente uma chave da Trie.
 * Apenas altera fimDePalavra = 0, sem liberar memória.
 * Isso preserva "galhos" partilhados por outras strings e evita
 * o custo de reconfiguração de ponteiros em remoções físicas.
 * Limitação conhecida: nós "mortos" permanecem alocados em RAM. */
void desmarcarTrie(NoTrie* raiz, const char* chave) {
    NoTrie* atual = raiz;
    for (int i = 0; chave[i] != '\0'; i++) {
        unsigned char c = (unsigned char)chave[i];
        if (c >= 128 || atual->filhos[c] == NULL) return;
        atual = atual->filhos[c];
    }
    atual->fimDePalavra = 0;
}

/* Busca recursivamente a primeira palavra completa a partir de um nó.
 * Acumula o sufixo caractere a caractere até encontrar um fimDePalavra = 1.
 * Retorna 1 se encontrado, 0 caso contrário. */
int buscarPrimeiraPalavra(NoTrie* no, char* sufixo, int profundidade,
                           char* resultado, const char* prefixo) {
    if (no == NULL) return 0;
    if (no->fimDePalavra) {
        sufixo[profundidade] = '\0';
        sprintf(resultado, "%s%s", prefixo, sufixo);
        return 1;
    }
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) {
            sufixo[profundidade] = (char)i;
            if (buscarPrimeiraPalavra(no->filhos[i], sufixo, profundidade + 1,
                                       resultado, prefixo)) return 1;
        }
    }
    return 0;
}

/* Recurso de autocompletar: dado um prefixo, retorna o nome completo mais próximo.
 * Navega pela Trie até o fim do prefixo (O(L)), depois busca o primeiro filho válido.
 * Retorna 1 se encontrou uma conclusão, 0 se o prefixo não existe na Trie. */
int autocompletarTrie(NoTrie* raiz, const char* prefixo, char* resultado) {
    NoTrie* atual = raiz;
    for (int i = 0; prefixo[i] != '\0'; i++) {
        unsigned char c = (unsigned char)prefixo[i];
        if (c >= 128 || atual->filhos[c] == NULL) return 0;
        atual = atual->filhos[c];
    }
    char sufixo[256];
    return buscarPrimeiraPalavra(atual, sufixo, 0, resultado, prefixo);
}

/* Libera recursivamente todos os nós da Trie (remoção física).
 * Chamada apenas no encerramento do programa ou no recarregamento. */
void liberarTrie(NoTrie* no) {
    if (no == NULL) return;
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) liberarTrie(no->filhos[i]);
    }
    free(no);
}

/* Conta o total de nós alocados na Trie.
 * Usado no cálculo de uso de memória do benchmark. */
long contarNosTrie(NoTrie* no) {
    if (no == NULL) return 0;
    long total = 1;
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) total += contarNosTrie(no->filhos[i]);
    }
    return total;
}


/* ============================================================
 *  IMPLEMENTAÇÃO: COUNTING BLOOM FILTER
 *
 *  Diferença do Bloom clássico: em vez de um vetor de bits,
 *  usa um vetor de contadores short (16 bits por posição).
 *  Inserção: incrementa (+1) os 3 contadores calculados pelas funções hash.
 *  Verificação: se qualquer contador for 0, a chave NÃO existe (garantia absoluta).
 *  Remoção: decrementa (-1) os 3 contadores — possível porque os contadores
 *  chegam a zero apenas quando nenhum elemento mais usa aquela posição.
 *
 *  Custo espacial: 200.000 * sizeof(short) = 200.000 * 2 = ~390 KB.
 *  Custo temporal de todas as operações: O(1) — apenas 3 cálculos de hash.
 * ============================================================ */

/* Aloca e inicializa o Counting Bloom Filter com todos os contadores a zero. */
BloomFilter* criarBloomFilter() {
    BloomFilter* bf = (BloomFilter*)malloc(sizeof(BloomFilter));
    if (bf != NULL) memset(bf->contadores, 0, BLOOM_ARRAY_SIZE * sizeof(short));
    return bf;
}

/* Segunda função hash: algoritmo sdbm.
 * Usada em conjunto com djb2 e hash_custom para minimizar colisões triplas.
 * Três hashes independentes reduzem a taxa de falsos positivos do filtro. */
unsigned int hash_sdbm(const char* str) {
    unsigned long hash = 0;
    int c;
    while ((c = (unsigned char)*str++)) hash = c + (hash << 6) + (hash << 16) - hash;
    return hash % BLOOM_SIZE;
}

/* Terceira função hash: polinomial custom (base 31).
 * Padrão utilizado em linguagens como Java para hashing de strings. */
unsigned int hash_custom(const char* str) {
    unsigned long hash = 0;
    int c;
    while ((c = (unsigned char)*str++)) hash = (hash * 31) + c;
    return hash % BLOOM_SIZE;
}

/* Insere uma chave no Counting Bloom Filter.
 * Calcula 3 posições via funções hash distintas e incrementa cada contador.
 * O limite 32767 protege contra overflow do tipo short (máx. 2^15 - 1). */
void inserirBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);

    /* Incrementa os 3 contadores; proteção contra overflow de short */
    if (bf->contadores[h1] < 32767) bf->contadores[h1]++;
    if (bf->contadores[h2] < 32767) bf->contadores[h2]++;
    if (bf->contadores[h3] < 32767) bf->contadores[h3]++;
}

/* Verifica se uma chave pode existir no sistema.
 * Se qualquer contador for 0, a chave DEFINITIVAMENTE não existe (sem falso negativo).
 * Se todos os contadores forem > 0, a chave PROVAVELMENTE existe (pode haver falso positivo).
 * Este filtro atua como escudo: descarta buscas inúteis antes de consultar Hash ou Trie. */
int verificarBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);

    if (bf->contadores[h1] == 0) return 0;
    if (bf->contadores[h2] == 0) return 0;
    if (bf->contadores[h3] == 0) return 0;

    return 1;  /* pode existir — confirmar na Hash se necessário */
}

/* Remove uma chave do Counting Bloom Filter.
 * Decrementa os 3 contadores das posições correspondentes.
 * Proteção contra underflow: só decrementa se o contador for > 0.
 * Esta operação é IMPOSSÍVEL no Bloom Filter clássico de bits,
 * pois desligar um bit corromperia outras chaves que compartilham a posição.
 * O uso de contadores short é exatamente o que viabiliza esta operação. */
void removerBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);

    if (bf->contadores[h1] > 0) bf->contadores[h1]--;
    if (bf->contadores[h2] > 0) bf->contadores[h2]--;
    if (bf->contadores[h3] > 0) bf->contadores[h3]--;
}


/* ============================================================
 *  OPERAÇÕES ANALÍTICAS E SIMULAÇÕES
 * ============================================================ */

/* [SIMULAÇÃO R16] — Injeção de ruído e corrupção de dados.
 * Corrompe aleatoriamente 10% dos registros da Lista Encadeada,
 * atribuindo Overall = 999 e Salário = -50.000.
 * A função filtrarJogadores() foi preparada para ignorar estes valores. */
void corromperDados(NoLista* cabeca) {
    srand(time(NULL));
    int afetados = 0;
    NoLista* atual = cabeca;
    while (atual != NULL) {
        if (rand() % 10 == 0) {  /* 10% de probabilidade de corrupção */
            atual->jogador.overall = 999;
            atual->jogador.salario = -50000;
            afetados++;
        }
        atual = atual->prox;
    }
    printf("[SIMULACAO] Alerta: %d registos sofreram corrupcao (Anomalias inseridas)!\n", afetados);
}

/* Filtragem paramétrica tolerante a falhas.
 * Permite ao usuário definir critérios de filtro (Overall ou Salário mínimo).
 * Ao filtrar por Overall, rejeita automaticamente valores > 99 (dados corrompidos pela R16).
 * Não modifica a estrutura física — apenas exibe os resultados válidos. */
void filtrarJogadores(NoLista* cabeca) {
    if (cabeca == NULL) return;
    int criterio, valorLimite;

    printf("\n=== FILTRAGEM (Tolerante a Falhas R16) ===\n");
    printf("1. Filtrar por Overall Minimo\n");
    printf("2. Filtrar por Salario Minimo\n");
    printf("Escolha: ");

    if (scanf("%d", &criterio) != 1) { while(getchar() != '\n'); return; }

    if (criterio == 1) printf("Overall minimo pretendido: ");
    else if (criterio == 2) printf("Salario minimo (EUR): ");
    else return;

    if (scanf("%d", &valorLimite) != 1) { while(getchar() != '\n'); return; }
    getchar();

    printf("\n%-25s | %-7s | %-10s\n", "Nome", "Overall", "Salario");
    printf("--------------------------------------------------\n");

    NoLista* atual = cabeca;
    int encontrados = 0;
    while (atual != NULL) {
        int atende = 0;
        /* Defesa ativa contra dados inválidos: Overall > 99 é sinal de corrupção (R16) */
        if (criterio == 1 && atual->jogador.overall >= valorLimite
                          && atual->jogador.overall <= 99) atende = 1;
        if (criterio == 2 && atual->jogador.salario >= valorLimite) atende = 1;

        if (atende) {
            printf("%-25s | %-7d | EUR %-6d\n",
                   atual->jogador.nome, atual->jogador.overall, atual->jogador.salario);
            encontrados++;
        }
        atual = atual->prox;
    }
    printf("Total encontrados (Ignorando corrompidos): %d\n", encontrados);
}

/* Agrupamento e Ordenação: Top 11 por Liga usando a Pilha.
 * Para cada liga, varre a Lista Encadeada e mantém um vetor dos 11 maiores overalls.
 * Em seguida, empilha os 11 jogadores (ordem crescente) e desempilha (ordem decrescente).
 * A propriedade LIFO da Pilha inverte a sequência automaticamente em O(1) por operação. */
void exibirTop11PorLiga(NoLista* cabeca, InfoLiga* ligas) {
    if (cabeca == NULL) return;
    for (int L = 0; L < 5; L++) {
        char* nomeDaLiga = ligas[L].nome_liga;
        if (ligas[L].contador == 0) continue;

        Jogador top11[11];
        for (int i = 0; i < 11; i++) top11[i].overall = -1;

        NoLista* atual = cabeca;
        while (atual != NULL) {
            /* Proteção R16: ignora jogadores com overall > 99 (dados corrompidos) */
            if (strcmp(atual->jogador.liga, nomeDaLiga) == 0
                    && atual->jogador.overall <= 99) {
                for (int i = 0; i < 11; i++) {
                    if (atual->jogador.overall > top11[i].overall) {
                        for (int j = 0; j < i; j++) top11[j] = top11[j + 1];
                        top11[i] = atual->jogador;
                        break;
                    }
                }
            }
            atual = atual->prox;
        }

        /* Empilha os 11 jogadores em ordem crescente de overall */
        NoPilha* pilhaTop11 = NULL;
        for (int i = 0; i < 11; i++) {
            if (top11[i].overall != -1) push(&pilhaTop11, top11[i]);
        }

        /* Desempilha em ordem decrescente (LIFO inverte automaticamente) */
        printf("\n=== TOP 11 DA PILHA - %s ===\n", nomeDaLiga);
        int posicao = 1;
        while (pilhaTop11 != NULL) {
            Jogador j = pop(&pilhaTop11);
            printf("%2d. %-20s | Overall: %d | Idade: %d\n",
                   posicao++, j.nome, j.overall, j.idade);
        }
    }
}

/* Cálculo Estatístico Global: médias de overall, idade e salário.
 * Exibe resultados para o total de jogadores e para cada uma das 5 grandes ligas.
 * Complexidade: O(1) — os somatórios já são acumulados na inserção. */
void exibirEstatisticas(EstatisticasGlobais* stats, InfoLiga* ligas) {
    printf("\n======================================================\n");
    printf("           ESTATISTICAS GERAIS E POR LIGA              \n");
    printf("======================================================\n");

    if (stats->contador == 0) {
        printf("Nenhum jogador carregado.\n");
        return;
    }

    printf("\n>> GERAL (%d jogadores no sistema)\n", stats->contador);
    printf(" - Overall medio: %.2f\n", (double)stats->soma_overall / stats->contador);
    printf(" - Idade media:   %.2f\n", (double)stats->soma_idade / stats->contador);
    printf(" - Salario medio: EUR %.2f\n", (double)stats->soma_salario / stats->contador);

    printf("\n>> POR LIGA (Top 5 Europeias)\n");
    printf("%-28s | %-9s | %-7s | %-6s | %-14s\n",
           "Liga", "Jogador.", "Overall", "Idade", "Salario Medio");
    printf("-----------------------------------------------------------------------\n");
    for (int i = 0; i < 5; i++) {
        if (ligas[i].contador == 0) {
            printf("%-28s | sem dados\n", ligas[i].nome_liga);
            continue;
        }
        double overallMedio  = (double)ligas[i].soma_overall  / ligas[i].contador;
        double idadeMedia    = (double)ligas[i].soma_idade    / ligas[i].contador;
        double salarioMedio  = (double)ligas[i].soma_salario  / ligas[i].contador;
        printf("%-28s | %-9d | %-7.2f | %-6.2f | EUR %-10.2f\n",
               ligas[i].nome_liga, ligas[i].contador,
               overallMedio, idadeMedia, salarioMedio);
    }
    printf("======================================================\n");
}


/* ============================================================
 *  MÓDULO DE BENCHMARKS
 * ============================================================ */

/* Benchmark de Escalabilidade: mede o tempo de varredura O(N)
 * da Lista Encadeada para diferentes tamanhos de N.
 * 200 repetições por tamanho para estabilizar a medição.
 * Comprova empiricamente a complexidade linear O(N). */
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel) {
    int tamanhosBase[] = {1000, 2500, 5000, 10000};
    int tamanhos[5];
    int numTamanhos = 0;

    for (int i = 0; i < 4; i++) {
        if (tamanhosBase[i] < totalDisponivel) tamanhos[numTamanhos++] = tamanhosBase[i];
    }
    tamanhos[numTamanhos++] = totalDisponivel;  /* inclui o tamanho real do dataset */

    printf("\n>> 3. ESCALABILIDADE (varredura O(N), 200 repeticoes por tamanho)\n");
    printf("%-15s | %15s\n", "Tamanho (N)", "Tempo (ms)");
    printf("----------------------------------------\n");

    for (int t = 0; t < numTamanhos; t++) {
        int limite = tamanhos[t];
        if (limite <= 0) continue;

        clock_t inicio = clock();
        for (int rep = 0; rep < 200; rep++) {
            NoLista* atual = lista;
            int contagem = 0;
            while (atual != NULL && contagem < limite) {
                contagem++;
                atual = atual->prox;
            }
        }
        clock_t fim = clock();
        double tempo = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
        printf("%-15d | %15.3f\n", limite, tempo);
    }
    printf("(O crescimento deve ser aproximadamente linear, coerente com O(N) da Lista)\n");
}

/* Benchmark de Inserção e Remoção: mede o custo combinado de inserir
 * e remover N registros sintéticos em todas as estruturas simultaneamente.
 * Valida o funcionamento da remoção no Counting Bloom Filter (decremento). */
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela, NoTrie* trie,
                               BloomFilter* bf, int n) {
    clock_t inicio, fim;

    Jogador* sinteticos = (Jogador*)malloc(n * sizeof(Jogador));
    if (sinteticos == NULL) {
        printf("\n[ERRO] Sem memoria para o benchmark de insercao/remocao.\n");
        return;
    }

    /* Gera N jogadores sintéticos com nomes únicos para evitar colisões de chave */
    for (int i = 0; i < n; i++) {
        sprintf(sinteticos[i].nome, "BenchPlayer_%d", i);
        strcpy(sinteticos[i].time, "BenchTeam");
        strcpy(sinteticos[i].liga, "BenchLeague");
        sinteticos[i].idade   = 20;
        sinteticos[i].overall = 70;
        sinteticos[i].salario = 10000;
    }

    /* INSERÇÃO em todas as estruturas simultaneamente */
    inicio = clock();
    for (int i = 0; i < n; i++) {
        inserirLista(lista, sinteticos[i]);
        inserirHash(tabela, sinteticos[i]);
        inserirTrie(trie, sinteticos[i].nome);
        inserirBloom(bf, sinteticos[i].nome);
    }
    fim = clock();
    double tempoInsercao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    /* REMOÇÃO em todas as estruturas — inclui decremento no Counting Bloom Filter */
    inicio = clock();
    for (int i = 0; i < n; i++) {
        removerDaLista(lista, sinteticos[i].nome);
        removerHash(tabela, sinteticos[i].nome);
        desmarcarTrie(trie, sinteticos[i].nome);  /* exclusão lógica na Trie */
        removerBloom(bf, sinteticos[i].nome);      /* decremento de contadores no Bloom */
    }
    fim = clock();
    double tempoRemocao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    printf("\n>> 4. TEMPO DE INSERCAO E REMOCAO (%d registos sinteticos)\n", n);
    printf(" - Insercao (Lista+Hash+Trie+Bloom): %10.3f ms  (%.5f ms/registo)\n",
           tempoInsercao, tempoInsercao / n);
    printf(" - Remocao  (Lista+Hash+Trie+Bloom): %10.3f ms  (%.5f ms/registo)\n",
           tempoRemocao, tempoRemocao / n);
    printf("(Os registos sinteticos sao removidos ao final; o dataset real nao e alterado.\n");
    printf(" A Trie mantem nos 'mortos' dos prefixos testados, pois desmarcarTrie() nao\n");
    printf(" libera memoria - limitacao conhecida e documentada da estrutura.)\n");

    free(sinteticos);
}

/* Análise de Colisões da Tabela Hash.
 * Calcula fator de carga, total de colisões, maior cadeia e cadeia média.
 * Verifica empiricamente a eficácia do tamanho primo (HASH_SIZE = 20011). */
void analisarColisoesHash(NoHash** tabela) {
    int bucketsOcupados = 0;
    int totalElementos  = 0;
    int maiorCadeia     = 0;

    for (int i = 0; i < HASH_SIZE; i++) {
        int tamanhoCadeia = 0;
        NoHash* atual = tabela[i];
        while (atual != NULL) { tamanhoCadeia++; atual = atual->prox; }
        if (tamanhoCadeia > 0) {
            bucketsOcupados++;
            totalElementos += tamanhoCadeia;
            if (tamanhoCadeia > maiorCadeia) maiorCadeia = tamanhoCadeia;
        }
    }

    /* Colisões = elementos que caíram em buckets já ocupados */
    int colisoesTotais = totalElementos - bucketsOcupados;
    double fatorCarga  = (double)totalElementos / HASH_SIZE;
    double cadeiaMedia = bucketsOcupados > 0
                         ? (double)totalElementos / bucketsOcupados : 0.0;

    printf("\n>> 5. TAXA DE COLISAO DA TABELA HASH (%d posicoes)\n", HASH_SIZE);
    printf(" - Elementos inseridos:        %d\n", totalElementos);
    printf(" - Posicoes (buckets) usadas:  %d\n", bucketsOcupados);
    printf(" - Fator de carga (load factor): %.4f\n", fatorCarga);
    printf(" - Colisoes totais (elementos extras em buckets ja ocupados): %d\n", colisoesTotais);
    printf(" - Maior cadeia (pior caso de busca): %d elementos\n", maiorCadeia);
    printf(" - Tamanho medio de cadeia (so buckets ocupados): %.2f\n", cadeiaMedia);
}

/* Cálculo de Uso de Memória Estimado.
 * Estima o consumo real de cada estrutura com base no número de elementos
 * e no tamanho em bytes de cada nó/posição. */
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie) {
    long memLista     = (long)totalJogadores * (long)sizeof(NoLista);
    long memHashNos   = (long)totalJogadores * (long)sizeof(NoHash);
    long memHashTabela = (long)HASH_SIZE * (long)sizeof(NoHash*);
    long nosTrie      = contarNosTrie(raizTrie);
    long memTrie      = nosTrie * (long)sizeof(NoTrie);
    /* Counting Bloom Filter: BLOOM_ARRAY_SIZE posições * sizeof(short) = 2 bytes cada */
    long memBloom     = (long)BLOOM_ARRAY_SIZE * (long)sizeof(short);
    long total        = memLista + memHashNos + memHashTabela + memTrie + memBloom;

    printf("\n>> 6. USO DE MEMORIA ESTIMADO (%d jogadores carregados)\n", totalJogadores);
    printf("%-24s | %14s | %10s\n", "Estrutura", "Bytes", "KB");
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "Lista Encadeada",     memLista,     memLista     / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (nos)",   memHashNos,   memHashNos   / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (vetor)", memHashTabela, memHashTabela / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Trie",                memTrie,      memTrie      / 1024.0);
    printf("  (Trie possui %ld nos alocados; cada no = %zu bytes devido aos 128 ponteiros)\n",
           nosTrie, sizeof(NoTrie));
    printf("%-24s | %14ld | %10.1f\n", "Counting Bloom Filter", memBloom,   memBloom     / 1024.0);
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "TOTAL ESTIMADO",      total,        total        / 1024.0);
}

/* Módulo central de benchmarks.
 * Executa 6 testes sequenciais com as restrições ativas no momento:
 *   1. Tempo de busca bem-sucedida: Lista O(N) vs Hash O(1)
 *   2. Rejeição de chave inexistente: Hash vs Counting Bloom Filter
 *   3. Escalabilidade da Lista Encadeada com N crescente
 *   4. Tempo de inserção e remoção em todas as estruturas
 *   5. Análise de colisões da Tabela Hash
 *   6. Estimativa de uso de memória RAM */
void executarBenchmarks(NoLista* lista, NoHash** tabela, NoTrie* raizTrie,
                        BloomFilter* bf, Simulacao* sim, int totalJogadores) {
    clock_t inicio, fim;
    double tempo_lista, tempo_hash, tempo_bloom_falso, tempo_hash_falso;

    /* Com latência R12 ativa, reduz para 100 iterações para evitar congelamento */
    int iteracoes = sim->r12_latencia ? 100 : 10000;

    char* nomeBusca = "L. Messi";
    char* nomeFalso = "Jogador Inexistente 999";

    printf("\n======================================================\n");
    printf("        BENCHMARKS DE DESEMPENHO (%d Iteracoes)       \n", iteracoes);
    if (sim->r9_cpu)        printf("        [!] AVISO: Estrangulamento de CPU ATIVO\n");
    if (sim->r12_latencia)  printf("        [!] AVISO: Latencia de Servidor ATIVA\n");
    if (sim->r21_algoritmo) printf("        [!] AVISO: Hash Desativada (Algoritmo Lento Forcado)\n");
    printf("======================================================\n");

    /* TESTE 1: Busca na Lista Encadeada — O(N) */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        NoLista* atual = lista;
        while (atual != NULL) {
            if (strcmp(atual->jogador.nome, nomeBusca) == 0) break;
            atual = atual->prox;
        }
    }
    fim = clock();
    tempo_lista = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    /* TESTE 2: Busca na Tabela Hash — O(1) amortizado.
     * Se R21 estiver ativa, a Hash é desligada e usa-se a Lista (degrada para O(N)). */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        if (sim->r21_algoritmo) {
            /* [R21] Modo Pânico: força pesquisa linear, ignorando a Hash */
            NoLista* atual = lista;
            while (atual != NULL) {
                if (strcmp(atual->jogador.nome, nomeBusca) == 0) break;
                atual = atual->prox;
            }
        } else {
            buscarHash(tabela, nomeBusca, sim);
        }
    }
    fim = clock();
    tempo_hash = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    /* TESTE: Rejeição de chave inexistente — Hash */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        buscarHash(tabela, nomeFalso, sim);
    }
    fim = clock();
    tempo_hash_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    /* TESTE: Rejeição de chave inexistente — Counting Bloom Filter.
     * O Bloom é imune a R9 e R12: opera exclusivamente em aritmética local (RAM),
     * sem acesso a ponteiros encadeados ou servidores remotos. */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        verificarBloom(bf, nomeFalso);
    }
    fim = clock();
    tempo_bloom_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    /* RESULTADOS */
    printf(">> 1. TEMPO DE BUSCA BEM-SUCEDIDA (\"%s\")\n", nomeBusca);
    printf(" - Lista Encadeada: %10.3f ms\n", tempo_lista);
    printf(" - Tabela Hash:     %10.3f ms\n", tempo_hash);

    printf("\n>> 2. REJEICAO DE DADO INEXISTENTE (\"%s\")\n", nomeFalso);
    printf(" - Tentar na Hash:  %10.3f ms\n", tempo_hash_falso);
    printf(" - Filtro de Bloom: %10.3f ms\n", tempo_bloom_falso);
    printf(" (Hash e Bloom respondem aqui a mesma pergunta - 'este nome existe?' - por isso\n");
    printf("  a comparacao e valida; o Bloom NAO substitui a Hash para recuperar os dados\n");
    printf("  completos do jogador, apenas para descartar buscas inuteis antecipadamente.)\n");

    benchmarkEscalabilidade(lista, totalJogadores);
    benchmarkInsercaoRemocao(&lista, tabela, raizTrie, bf,
                              sim->r12_latencia ? 200 : 2000);
    analisarColisoesHash(tabela);
    calcularUsoMemoria(totalJogadores, raizTrie);

    printf("======================================================\n");
}


/* ============================================================
 *  PARSER DO DATASET E GESTÃO DO SISTEMA
 * ============================================================ */

/* Verifica se um nome de liga pertence às 5 ligas monitoradas.
 * Retorna o índice (0–4) se encontrado, -1 caso contrário. */
int verificarLiga(char* nome, InfoLiga* ligas) {
    for (int i = 0; i < 5; i++) {
        if (strcmp(nome, ligas[i].nome_liga) == 0) return i;
    }
    return -1;
}

/* Libera toda a memória das estruturas e reinicia as estatísticas.
 * Chamada antes de recarregar o dataset ou ao encerrar o programa. */
void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie,
                      BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas) {
    liberarLista(lista);
    liberarHash(tabela);
    /* Libera os filhos da Trie recursivamente, preservando o nó raiz */
    for (int i = 0; i < 128; i++) {
        if (trie->filhos[i]) { liberarTrie(trie->filhos[i]); trie->filhos[i] = NULL; }
    }
    /* Zera os contadores do Counting Bloom Filter (sizeof(short) = 2 bytes) */
    memset(bf->contadores, 0, BLOOM_ARRAY_SIZE * sizeof(short));

    stats->contador = 0; stats->soma_idade = 0;
    stats->soma_overall = 0; stats->soma_salario = 0;
    for (int i = 0; i < 5; i++) {
        ligas[i].contador = 0; ligas[i].soma_overall = 0;
        ligas[i].soma_idade = 0; ligas[i].soma_salario = 0;
    }
}

/* Parser do arquivo CSV players_22.csv.
 * Lê linha por linha, trata aspas duplas e vírgulas internas,
 * e distribui cada jogador lido pelas 5 estruturas simultaneamente.
 * [R2] Se r2_memoria estiver ativo, para após 5.000 registros. */
void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie,
                     BloomFilter* bf, EstatisticasGlobais* stats,
                     InfoLiga* ligas, Simulacao* sim) {
    FILE* arquivo = fopen("players_22.csv", "r");
    if (!arquivo) {
        printf("\n[ERRO] O ficheiro 'players_22.csv' nao foi encontrado!\n");
        return;
    }

    char linha[10000];
    fgets(linha, sizeof(linha), arquivo);  /* descarta o cabeçalho */

    while (fgets(linha, sizeof(linha), arquivo)) {
        /* [SIMULAÇÃO R2] — Limitação de Memória: para ao atingir 5.000 registros */
        if (sim->r2_memoria && stats->contador >= 5000) break;

        int col = 0, j = 0, dentro_aspas = 0, i;
        char buffer[256];
        Jogador jog = {"", "Agente Livre", "Sem Liga", 0, 0, 0};

        /* Parser de CSV com suporte a campos entre aspas duplas */
        for (i = 0; linha[i] != '\0' && linha[i] != '\n'; i++) {
            if (linha[i] == '"') { dentro_aspas = !dentro_aspas; continue; }
            if (linha[i] == ',' && !dentro_aspas) {
                buffer[j] = '\0';
                /* Mapeamento das colunas relevantes do CSV */
                if      (col ==  2) strncpy(jog.nome,  buffer, 99);
                else if (col ==  5) jog.overall = atoi(buffer);
                else if (col ==  8) jog.salario = atoi(buffer);
                else if (col ==  9) jog.idade   = atoi(buffer);
                else if (col == 14) strncpy(jog.time,  buffer, 99);
                else if (col == 15) strncpy(jog.liga,  buffer, 49);
                j = 0; col++;
            } else if (j < 255) buffer[j++] = linha[i];
        }

        if (strlen(jog.nome) > 0) {
            /* Distribui o jogador pelas 5 estruturas simultaneamente */
            inserirLista(cabecaLista, jog);
            inserirHash(tabela, jog);
            inserirTrie(raizTrie, jog.nome);
            inserirBloom(bf, jog.nome);

            /* Acumula somatórios para cálculos estatísticos em O(1) posterior */
            stats->soma_overall += jog.overall;
            stats->soma_idade   += jog.idade;
            stats->soma_salario += jog.salario;
            stats->contador++;

            int idx_liga = verificarLiga(jog.liga, ligas);
            if (idx_liga != -1) {
                ligas[idx_liga].soma_overall += jog.overall;
                ligas[idx_liga].soma_idade   += jog.idade;
                ligas[idx_liga].soma_salario += jog.salario;
                ligas[idx_liga].contador++;
            }
        }
    }
    fclose(arquivo);
    printf("\n[SUCESSO] %d jogadores foram carregados!\n", stats->contador);
}


/* ============================================================
 *  PROGRAMA PRINCIPAL — LOOP DE MENU INTERATIVO
 * ============================================================ */
int main() {
    /* Inicialização das estruturas de dados */
    NoLista* listaJogadores = NULL;
    NoTrie*  raizTrie       = criarNoTrie();
    BloomFilter* filtroBloom = criarBloomFilter();
    NoHash* tabelaHash[HASH_SIZE];
    for (int i = 0; i < HASH_SIZE; i++) tabelaHash[i] = NULL;

    EstatisticasGlobais statsGlobais = {0, 0, 0, 0};
    InfoLiga ligas[5];
    char* nomes_ligas[] = {
        "English Premier League", "Spain Primera Division",
        "Italian Serie A", "German 1. Bundesliga", "French Ligue 1"
    };
    for (int i = 0; i < 5; i++) { strcpy(ligas[i].nome_liga, nomes_ligas[i]); }

    Simulacao sim = {0, 0, 0, 0, 0};  /* todas as restrições começam desligadas */

    printf("A ler a Base de Dados. Aguarde...\n");
    carregarDataset(&listaJogadores, tabelaHash, raizTrie,
                    filtroBloom, &statsGlobais, ligas, &sim);

    int  opcao = 0;
    char prefixo[100];
    char nomeCompleto[100];

    while (opcao != 9) {
        printf("\n=========================================\n");
        printf("       SISTEMA DE GERENCIAMENTO FIFA     \n");
        printf("=========================================\n");
        printf("1. Inserir Jogador (Manual)\n");
        printf("2. Buscar Jogador\n");
        printf("3. Remover Jogador\n");
        printf("4. Exibir Dados das Ligas e Gerais\n");
        printf("5. Agrupar e Exibir TOP 11 por Liga\n");
        printf("6. Filtrar Jogadores\n");
        printf("7. Executar Benchmarks\n");
        printf("8. Painel de Simulacao (RESTRICOES)\n");
        printf("9. Sair\n");
        printf("Escolha uma opcao: ");

        if (scanf("%d", &opcao) != 1) { while(getchar() != '\n'); continue; }
        getchar();

        switch (opcao) {
            case 1: {
                /* Inserção manual: propaga o novo jogador para todas as estruturas */
                Jogador novo = {"", "Manual", "N/A", 22, 0, 0};
                printf("Nome: ");
                fgets(novo.nome, sizeof(novo.nome), stdin);
                novo.nome[strcspn(novo.nome, "\n")] = 0;
                printf("Overall: "); scanf("%d", &novo.overall); getchar();

                inserirLista(&listaJogadores, novo);
                inserirHash(tabelaHash, novo);
                inserirTrie(raizTrie, novo.nome);
                inserirBloom(filtroBloom, novo.nome);

                statsGlobais.contador++;
                statsGlobais.soma_overall += novo.overall;
                statsGlobais.soma_idade   += novo.idade;
                statsGlobais.soma_salario += novo.salario;

                printf("[SUCESSO] Inserido!\n");
                break;
            }
            case 2: {
                /* Busca com autocompletar:
                 * 1. Trie completa o prefixo em O(L)
                 * 2. Counting Bloom rejeita inexistentes em O(1)
                 * 3. Hash confirma e retorna os dados em O(1)
                 * Se R21 estiver ativa, a Hash é ignorada e usa-se varredura linear. */
                printf("\n--- BUSCA RAPIDA ---\n");
                printf("Nome/Prefixo: ");
                fgets(prefixo, sizeof(prefixo), stdin);
                prefixo[strcspn(prefixo, "\n")] = 0;

                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);

                    if (!sim.r21_algoritmo && verificarBloom(filtroBloom, nomeCompleto) == 0) {
                        printf("[BLOOM FILTER] DEFINITIVAMENTE NAO EXISTE.\n");
                    } else {
                        Jogador* achado = NULL;
                        if (sim.r21_algoritmo) {
                            /* [R21] Modo Pânico: pesquisa linear forçada */
                            printf("[AVISO R21] A usar pesquisa linear...\n");
                            NoLista* atual = listaJogadores;
                            while (atual) {
                                if (strcmp(atual->jogador.nome, nomeCompleto) == 0) {
                                    achado = &(atual->jogador); break;
                                }
                                atual = atual->prox;
                            }
                        } else {
                            achado = buscarHash(tabelaHash, nomeCompleto, &sim);
                        }
                        if (achado)
                            printf("Nome: %s | Clube: %s | Overall: %d\n",
                                   achado->nome, achado->time, achado->overall);
                    }
                } else printf("[TRIE] Nao encontrado.\n");
                break;
            }
            case 3: {
                /* Remoção consistente: remove o jogador de TODAS as estruturas.
                 * Lista e Hash: remoção física (free).
                 * Trie: exclusão lógica (fimDePalavra = 0).
                 * Counting Bloom Filter: decremento dos 3 contadores. */
                printf("\n--- REMOCAO DE JOGADOR ---\n");
                printf("Nome/Prefixo: ");
                fgets(prefixo, sizeof(prefixo), stdin);
                prefixo[strcspn(prefixo, "\n")] = 0;

                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);

                    Jogador* alvo = buscarHash(tabelaHash, nomeCompleto, &sim);
                    if (alvo != NULL) {
                        Jogador copia = *alvo;  /* cópia antes da remoção da Hash */

                        int okLista = removerDaLista(&listaJogadores, nomeCompleto);
                        int okHash  = removerHash(tabelaHash, nomeCompleto);
                        desmarcarTrie(raizTrie, nomeCompleto);
                        removerBloom(filtroBloom, nomeCompleto);  /* decrementa contadores */

                        if (okLista && okHash) {
                            /* Atualiza somatórios para manter estatísticas consistentes */
                            statsGlobais.contador--;
                            statsGlobais.soma_overall -= copia.overall;
                            statsGlobais.soma_idade   -= copia.idade;
                            statsGlobais.soma_salario -= copia.salario;

                            int idx_liga = verificarLiga(copia.liga, ligas);
                            if (idx_liga != -1) {
                                ligas[idx_liga].contador--;
                                ligas[idx_liga].soma_overall -= copia.overall;
                                ligas[idx_liga].soma_idade   -= copia.idade;
                                ligas[idx_liga].soma_salario -= copia.salario;
                            }
                            printf("[SUCESSO] \"%s\" removido da Lista, Hash, Trie e Bloom Filter.\n",
                                   nomeCompleto);
                        } else {
                            printf("[ERRO] Inconsistencia: nao foi possivel remover de todas as estruturas.\n");
                        }
                    } else {
                        printf("[ERRO] Encontrado na Trie mas nao na Hash (estruturas inconsistentes).\n");
                    }
                } else {
                    printf("[TRIE] Nao encontrado.\n");
                }
                break;
            }
            case 4: exibirEstatisticas(&statsGlobais, ligas); break;
            case 5: exibirTop11PorLiga(listaJogadores, ligas);  break;
            case 6: filtrarJogadores(listaJogadores);            break;
            case 7: executarBenchmarks(listaJogadores, tabelaHash, raizTrie,
                                       filtroBloom, &sim, statsGlobais.contador); break;

            case 8: {
                /* Painel de Simulação: ativa/desativa as 5 restrições adversas do edital.
                 * As restrições impactam as operações em tempo de execução — não é necessário
                 * recarregar o dataset para sentir os efeitos nas buscas e benchmarks.
                 * Exceção: R2 (limite de memória) só tem efeito no carregamento (opção 6). */
                int opSim = 0;
                while (opSim != 7) {
                    printf("\n=== PAINEL DE SIMULACAO (CONDICOES ADVERSAS) ===\n");
                    printf("1. [R2] Limitar Memoria RAM a 5000 Registos:  %s\n",
                           sim.r2_memoria    ? "ON" : "OFF");
                    printf("2. [R9] Estrangular CPU (Hash Demorada):      %s\n",
                           sim.r9_cpu        ? "ON" : "OFF");
                    printf("3. [R12] Latencia de Rede (Servidor Lento):   %s\n",
                           sim.r12_latencia  ? "ON" : "OFF");
                    printf("4. [R16] Injetar Virus (Corromper Dados):     %s\n",
                           sim.r16_dados     ? "ON" : "OFF");
                    printf("5. [R21] Degradar Algoritmo (Desligar Hash):  %s\n",
                           sim.r21_algoritmo ? "ON" : "OFF");
                    printf("6. RECARREGAR BASE DE DADOS (Aplica a R2)\n");
                    printf("7. Voltar ao Menu Principal\n");
                    printf("Escolha o que ligar/desligar: ");

                    if (scanf("%d", &opSim) != 1) { while(getchar() != '\n'); continue; }

                    if      (opSim == 1) sim.r2_memoria    = !sim.r2_memoria;
                    else if (opSim == 2) sim.r9_cpu        = !sim.r9_cpu;
                    else if (opSim == 3) sim.r12_latencia  = !sim.r12_latencia;
                    else if (opSim == 4) { sim.r16_dados = 1; corromperDados(listaJogadores); }
                    else if (opSim == 5) sim.r21_algoritmo = !sim.r21_algoritmo;
                    else if (opSim == 6) {
                        printf("\nA recarregar do zero...\n");
                        limparEstruturas(&listaJogadores, tabelaHash, raizTrie,
                                         filtroBloom, &statsGlobais, ligas);
                        carregarDataset(&listaJogadores, tabelaHash, raizTrie,
                                        filtroBloom, &statsGlobais, ligas, &sim);
                    }
                }
                break;
            }
            case 9: printf("\nA encerrar o simulador...\n"); break;
            default: printf("[!] Opcao invalida.\n");
        }
    }

    /* Liberação de toda a memória antes de encerrar */
    limparEstruturas(&listaJogadores, tabelaHash, raizTrie,
                     filtroBloom, &statsGlobais, ligas);
    free(raizTrie);
    free(filtroBloom);
    return 0;
}