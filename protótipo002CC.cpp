/*
 * ============================================================
 *  SISTEMA DE GERENCIAMENTO DE JOGADORES FIFA 22
 * ============================================================
 *  Estruturas utilizadas:
 *    - Lista Encadeada Simples  (inserção, remoção, varredura)
 *    - Tabela Hash com Chaining (busca O(1) amortizado)
 *    - Árvore Trie              (autocompletar por prefixo)
 *    - Bloom Filter             (teste de pertinência sem falso negativo)
 *    - Pilha (Stack)            (exibição do TOP 11 em ordem inversa)
 *
 *  O código também simula 5 "condições adversas" (R2, R9, R12,
 *  R16, R21) para demonstrar resiliência das estruturas.
 * ============================================================
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   /* clock(), CLOCKS_PER_SEC — medir tempo de CPU */
 
/* ============================================================
 *  CONSTANTES GLOBAIS
 * ============================================================ */
 
/* Número de buckets da tabela hash. Valor primo reduz colisões,
 * pois primos distribuem os restos (%) de forma mais uniforme. */
#define HASH_SIZE 20011
 
/* Tamanho lógico do Bloom Filter em bits (200 000 bits ~ 24 KB). */
#define BLOOM_SIZE 200000
 
/* Tamanho físico em bytes do array de bits do Bloom Filter.
 * +1 garante que nenhum índice calculado por (bit / 8) ultrapasse
 * o array quando BLOOM_SIZE não é múltiplo exato de 8. */
#define BLOOM_ARRAY_SIZE (BLOOM_SIZE / 8) + 1
 
 
/* ============================================================
 *  STRUCTS DE DOMÍNIO
 * ============================================================ */
 
/* Representa um jogador com os campos extraídos do CSV. */
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
 *  uma "restrição de hardware/software" artificial para fins
 *  pedagógicos.
 * ============================================================ */
typedef struct {
    int r2_memoria;    /* R2  — limita o carregamento a 5 000 registos  */
    int r9_cpu;        /* R9  — insere cálculo inútil antes de cada busca na Hash */
    int r12_latencia;  /* R12 — adiciona atraso de 1 ms simulando rede lenta */
    int r16_dados;     /* R16 — corrompe 10 % dos registos (Overall = 999) */
    int r21_algoritmo; /* R21 — desliga a Hash e força pesquisa linear O(N) */
} Simulacao;
 
 
/* ============================================================
 *  STRUCTS DAS ESTRUTURAS DE DADOS
 * ============================================================ */
 
/* Nó da Pilha (Stack — LIFO: Last In, First Out).
 * Usada apenas para inverter a exibição do TOP 11. */
typedef struct NoPilha {
    Jogador        jogador;
    struct NoPilha* prox;   /* ponteiro para o nó anterior (topo anterior) */
} NoPilha;
 
/* Nó da Lista Encadeada Simples.
 * Estrutura principal de armazenamento; permite varredura O(N). */
typedef struct NoLista {
    Jogador        jogador;
    struct NoLista* prox;
} NoLista;
 
/* Nó da Tabela Hash com Encadeamento Externo (Separate Chaining).
 * Cada bucket da tabela é uma lista de nós; colisões são resolvidas
 * encadeando os elementos no mesmo bucket. */
typedef struct NoHash {
    Jogador       jogador;
    struct NoHash* prox;
} NoHash;
 
/* Nó da Árvore Trie (Prefix Tree).
 * Cada nó representa um caractere; o caminho da raiz até um nó
 * com fimDePalavra == 1 forma uma chave completa.
 * 128 filhos cobrem todos os caracteres ASCII imprimíveis. */
typedef struct NoTrie {
    struct NoTrie* filhos[128]; /* índice = valor ASCII do caractere */
    int            fimDePalavra;/* 1 se este nó termina uma chave inserida */
} NoTrie;
 
/* Bloom Filter baseado em array de bits.
 * Armazena BLOOM_SIZE bits compactados em bytes (8 bits por byte).
 * Permite testar pertinência em O(1) com zero falsos negativos,
 * mas pode gerar falsos positivos. */
typedef struct {
    unsigned char bits[BLOOM_ARRAY_SIZE];
} BloomFilter;
 
/* Estatísticas acumuladas por liga (uma das 5 grandes ligas europeias). */
typedef struct {
    char      nome_liga[50];
    long long soma_overall;
    long long soma_idade;
    long long soma_salario;
    int       contador;     /* quantidade de jogadores dessa liga no sistema */
} InfoLiga;
 
/* Estatísticas globais de todos os jogadores carregados. */
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
 
/* Funções auxiliares usadas exclusivamente pelo módulo de benchmark */
long contarNosTrie(NoTrie* no);
void analisarColisoesHash(NoHash** tabela);
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie);
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela,
                               NoTrie* trie, BloomFilter* bf, int n);
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel);
 
 
/* ============================================================
 *  PILHA (STACK) — LIFO
 *  Operações: push (empilhar) e pop (desempilhar).
 *  Complexidade: O(1) para ambas as operações.
 * ============================================================ */
 
/* Cria um novo nó e coloca no topo da pilha.
 * "topo" é passado como ponteiro para ponteiro (**) para que a
 * função possa atualizar o ponteiro de topo da pilha do chamador. */
void push(NoPilha** topo, Jogador jog) {
    NoPilha* novo = (NoPilha*)malloc(sizeof(NoPilha));
    if (novo == NULL) return; /* falha de alocação: aborta silenciosamente */
    novo->jogador = jog;
    novo->prox    = *topo;   /* o novo nó aponta para o topo anterior */
    *topo         = novo;    /* o topo passa a ser o novo nó */
}
 
/* Remove e retorna o elemento do topo da pilha.
 * Retorna um Jogador "vazio" caso a pilha esteja vazia. */
Jogador pop(NoPilha** topo) {
    Jogador jog = {"", "", "", 0, 0, 0}; /* valor sentinela de retorno vazio */
    if (*topo == NULL) return jog;
 
    NoPilha* temp = *topo;       /* guarda referência para liberar memória */
    jog           = temp->jogador;
    *topo         = temp->prox;  /* topo avança para o próximo nó */
    free(temp);                  /* libera o nó removido */
    return jog;
}
 
 
/* ============================================================
 *  LISTA ENCADEADA SIMPLES
 *  Inserção no início (O(1)); busca e remoção O(N).
 * ============================================================ */
 
/* Insere um novo jogador na cabeça da lista.
 * Inserir no início é O(1) e evita percorrer a lista inteira. */
void inserirLista(NoLista** cabeca, Jogador jog) {
    NoLista* novo = (NoLista*)malloc(sizeof(NoLista));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox    = *cabeca; /* novo nó aponta para a antiga cabeça */
    *cabeca       = novo;    /* cabeça atualizada para o novo nó */
}
 
/* Percorre toda a lista liberando cada nó.
 * Define *cabeca = NULL ao final para evitar ponteiro pendente (dangling pointer). */
void liberarLista(NoLista** cabeca) {
    NoLista* atual = *cabeca;
    while (atual != NULL) {
        NoLista* temp = atual; /* guarda referência antes de avançar */
        atual         = atual->prox;
        free(temp);
    }
    *cabeca = NULL;
}
 
/* Remove o primeiro nó cujo nome coincide com o parâmetro.
 * Retorna 1 em caso de sucesso, 0 se não encontrado.
 *
 * O ponteiro "anterior" rastreia o nó precedente para que
 * possamos redirecionar o prox sem perder o elo da lista. */
int removerDaLista(NoLista** cabeca, const char* nome) {
    NoLista* atual    = *cabeca;
    NoLista* anterior = NULL;
 
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0) {
            /* Caso especial: remoção da cabeça da lista */
            if (anterior == NULL) *cabeca       = atual->prox;
            /* Caso geral: remoção de nó intermediário ou final */
            else                  anterior->prox = atual->prox;
 
            free(atual);
            return 1; /* remoção bem-sucedida */
        }
        anterior = atual;
        atual    = atual->prox;
    }
    return 0; /* nome não encontrado */
}
 
 
/* ============================================================
 *  TABELA HASH COM ENCADEAMENTO EXTERNO
 *  Busca média O(1); pior caso O(N) se todos os elementos
 *  colidirem no mesmo bucket.
 * ============================================================ */
 
/* Função de hash djb2 (Daniel J. Bernstein).
 * hash = hash * 33 + c  (via operações de bit mais rápidas que
 * multiplicação direta em CPUs antigas).
 * A expressão "hash << 5 + hash" equivale a "hash * 32 + hash = hash * 33".
 * Retorna o índice do bucket (resultado % HASH_SIZE). */
unsigned int funcaoHash(const char* str) {
    unsigned long hash = 5381; /* valor inicial "mágico" escolhido empiricamente */
    int c;
    /* (unsigned char)*str++ — cast evita que bytes com valor > 127
     * sejam interpretados como negativos em plataformas onde char é signed */
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}
 
/* Insere um jogador na tabela hash.
 * O novo nó é inserido no início da cadeia do bucket (O(1)),
 * pelo mesmo motivo da lista: não é necessário percorrer a cadeia. */
void inserirHash(NoHash** tabela, Jogador jog) {
    unsigned int idx = funcaoHash(jog.nome); /* calcula o bucket */
    NoHash* novo = (NoHash*)malloc(sizeof(NoHash));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox    = tabela[idx]; /* encadeia antes do primeiro elemento do bucket */
    tabela[idx]   = novo;
}
 
/* Busca um jogador na hash pelo nome.
 * Recebe a struct Simulacao para aplicar as condições adversas R9 e R12.
 * Retorna ponteiro para o Jogador dentro da estrutura, ou NULL se não achado. */
Jogador* buscarHash(NoHash** tabela, const char* nome, Simulacao* sim) {
 
    /* [SIMULAÇÃO R9] — Estrangulamento de CPU.
     * Executa 50 000 somas de ponto flutuante inúteis antes de cada busca,
     * simulando uma CPU sobrecarregada. "volatile" impede que o compilador
     * otimize/elimine o loop (sem volatile, o compilador poderia descartar
     * o loop por perceber que dummy não é usado). */
    if (sim != NULL && sim->r9_cpu) {
        volatile double dummy = 0.0;
        for (int i = 0; i < 50000; i++) dummy += 1.1;
    }
 
    /* [SIMULAÇÃO R12] — Latência de rede/disco de 1 ms.
     * Usa busy-wait (espera ativa com clock()) em vez de sleep(), pois
     * sleep() teria resolução de segundos em C padrão. A expressão
     * "start + (1 * CLOCKS_PER_SEC / 1000)" converte 1 ms para ticks
     * de clock da CPU. */
    if (sim != NULL && sim->r12_latencia) {
        clock_t start = clock();
        while (clock() < start + (1 * CLOCKS_PER_SEC / 1000));
    }
 
    unsigned int idx  = funcaoHash(nome); /* calcula o bucket */
    NoHash*      atual = tabela[idx];
 
    /* Percorre a cadeia do bucket procurando pelo nome exato */
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0)
            return &(atual->jogador); /* retorna endereço do jogador no nó */
        atual = atual->prox;
    }
    return NULL; /* não encontrado */
}
 
/* Remove o nó com o nome dado da cadeia do bucket correspondente.
 * Lógica de remoção com "anterior" idêntica à da lista encadeada. */
int removerHash(NoHash** tabela, const char* nome) {
    unsigned int idx  = funcaoHash(nome);
    NoHash*      atual = tabela[idx];
    NoHash*      ant   = NULL;
 
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0) {
            if (ant == NULL) tabela[idx] = atual->prox;
            else             ant->prox   = atual->prox;
            free(atual);
            return 1;
        }
        ant   = atual;
        atual = atual->prox;
    }
    return 0;
}
 
/* Percorre todos os HASH_SIZE buckets e libera cada cadeia. */
void liberarHash(NoHash** tabela) {
    for (int i = 0; i < HASH_SIZE; i++) {
        NoHash* atual = tabela[i];
        while (atual != NULL) {
            NoHash* temp = atual;
            atual        = atual->prox;
            free(temp);
        }
        tabela[i] = NULL; /* evita ponteiro pendente no bucket */
    }
}
 
 
/* ============================================================
 *  ÁRVORE TRIE (PREFIX TREE)
 *  Inserção e busca O(M) onde M = comprimento da chave.
 *  Ideal para autocompletar: dado um prefixo, encontra a
 *  primeira palavra completa em O(M + profundidade_restante).
 * ============================================================ */
 
/* Aloca e inicializa um novo nó da Trie.
 * Todos os 128 ponteiros de filhos são NULL (nenhum filho ainda).
 * fimDePalavra = 0 significa que este nó ainda não termina uma chave. */
NoTrie* criarNoTrie() {
    NoTrie* n = (NoTrie*)malloc(sizeof(NoTrie));
    if (n) {
        n->fimDePalavra = 0;
        for (int i = 0; i < 128; i++) n->filhos[i] = NULL;
    }
    return n;
}
 
/* Insere uma chave na Trie caractere a caractere.
 * Para cada caractere c da chave:
 *   - usa seu valor ASCII como índice no array de filhos;
 *   - cria o nó filho se ainda não existir;
 *   - avança para esse filho.
 * Ao terminar a chave, marca fimDePalavra = 1 no nó atual. */
void inserirTrie(NoTrie* raiz, const char* chave) {
    NoTrie* atual = raiz;
    for (int i = 0; chave[i] != '\0'; i++) {
        unsigned char c = (unsigned char)chave[i];
        if (c >= 128) continue; /* ignora caracteres fora do ASCII de 7 bits */
        if (atual->filhos[c] == NULL)
            atual->filhos[c] = criarNoTrie();
        atual = atual->filhos[c];
    }
    atual->fimDePalavra = 1; /* marca o fim da chave inserida */
}
 
/* "Remove" uma chave da Trie desmarcando fimDePalavra.
 * ATENÇÃO: esta operação NÃO desaloca os nós do caminho —
 * eles ficam "mortos" até a Trie inteira ser liberada.
 * Esse é um trade-off de memória documentado (ver benchmark). */
void desmarcarTrie(NoTrie* raiz, const char* chave) {
    NoTrie* atual = raiz;
    for (int i = 0; chave[i] != '\0'; i++) {
        unsigned char c = (unsigned char)chave[i];
        if (c >= 128 || atual->filhos[c] == NULL) return; /* chave não existe */
        atual = atual->filhos[c];
    }
    atual->fimDePalavra = 0; /* desmarca sem desalocar */
}
 
/* Função auxiliar recursiva: a partir de um nó intermediário da Trie,
 * encontra a primeira palavra completa (DFS — Depth-First Search).
 * "profundidade" rastreia quantos caracteres do sufixo já foram
 * acumulados em "sufixo[]"; ao encontrar fimDePalavra, monta o
 * resultado concatenando prefixo + sufixo. */
int buscarPrimeiraPalavra(NoTrie* no, char* sufixo, int profundidade,
                           char* resultado, const char* prefixo) {
    if (no == NULL) return 0;
 
    if (no->fimDePalavra) {
        sufixo[profundidade] = '\0'; /* termina a string do sufixo */
        /* sprintf: formata prefixo + sufixo numa única string de resultado */
        sprintf(resultado, "%s%s", prefixo, sufixo);
        return 1; /* palavra encontrada */
    }
 
    /* Tenta cada filho em ordem ASCII (0 a 127) — retorna a primeira palavra */
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) {
            sufixo[profundidade] = (char)i; /* acrescenta o caractere ao sufixo */
            if (buscarPrimeiraPalavra(no->filhos[i], sufixo,
                                      profundidade + 1, resultado, prefixo))
                return 1;
        }
    }
    return 0; /* nenhuma palavra encontrada a partir deste nó */
}
 
/* Autocompleta: dado um prefixo, retorna a primeira chave completa
 * que começa com ele, gravando em "resultado".
 * Retorna 1 se encontrou, 0 caso contrário. */
int autocompletarTrie(NoTrie* raiz, const char* prefixo, char* resultado) {
    NoTrie* atual = raiz;
 
    /* Desce até o nó correspondente ao último caractere do prefixo */
    for (int i = 0; prefixo[i] != '\0'; i++) {
        unsigned char c = (unsigned char)prefixo[i];
        if (c >= 128 || atual->filhos[c] == NULL) return 0; /* prefixo inexistente */
        atual = atual->filhos[c];
    }
 
    char sufixo[256]; /* buffer para acumular o sufixo durante a DFS */
    return buscarPrimeiraPalavra(atual, sufixo, 0, resultado, prefixo);
}
 
/* Libera recursivamente todos os nós da Trie (pós-ordem).
 * Pós-ordem garante que os filhos são liberados antes do pai,
 * evitando acesso a memória já desalocada. */
void liberarTrie(NoTrie* no) {
    if (no == NULL) return;
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL)
            liberarTrie(no->filhos[i]);
    }
    free(no);
}
 
/* Conta recursivamente todos os nós alocados na Trie.
 * Inclui nós "mortos" (fimDePalavra == 0 sem filhos) deixados
 * por desmarcarTrie() — evidencia o custo de memória. */
long contarNosTrie(NoTrie* no) {
    if (no == NULL) return 0;
    long total = 1; /* conta o nó atual */
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL)
            total += contarNosTrie(no->filhos[i]);
    }
    return total;
}
 
 
/* ============================================================
 *  BLOOM FILTER
 *  Estrutura probabilística: testa pertinência em O(1) sem
 *  falsos negativos, mas com possibilidade de falsos positivos.
 *  NÃO suporta remoção segura (bits compartilhados entre chaves).
 * ============================================================ */
 
/* Aloca o Bloom Filter e zera todos os bits com memset.
 * memset(ptr, 0, n): preenche n bytes a partir de ptr com 0. */
BloomFilter* criarBloomFilter() {
    BloomFilter* bf = (BloomFilter*)malloc(sizeof(BloomFilter));
    if (bf != NULL)
        memset(bf->bits, 0, BLOOM_ARRAY_SIZE);
    return bf;
}
 
/* Função de hash SDBM — distribuição complementar à djb2.
 * Usar funções de hash independentes reduz a probabilidade de
 * colisão simultânea nas 3 posições de bit do Bloom Filter. */
unsigned int hash_sdbm(const char* str) {
    unsigned long hash = 0;
    int c;
    while ((c = (unsigned char)*str++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash % BLOOM_SIZE;
}
 
/* Terceira função de hash (polinomial de base 31).
 * Comumente usada em Java/C++ para strings. */
unsigned int hash_custom(const char* str) {
    unsigned long hash = 0;
    int c;
    while ((c = (unsigned char)*str++))
        hash = (hash * 31) + c;
    return hash % BLOOM_SIZE;
}
 
/* Liga os 3 bits correspondentes à chave no array de bits.
 *   bf->bits[h / 8]  — seleciona o byte que contém o bit h
 *   (1 << (h % 8))   — cria uma máscara com apenas o bit h ligado
 *   |=               — liga esse bit sem alterar os demais */
void inserirBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);
 
    bf->bits[h1 / 8] |= (1 << (h1 % 8));
    bf->bits[h2 / 8] |= (1 << (h2 % 8));
    bf->bits[h3 / 8] |= (1 << (h3 % 8));
}
 
/* Verifica se os 3 bits da chave estão todos ligados.
 * Se qualquer bit estiver 0 → a chave DEFINITIVAMENTE não foi inserida.
 * Se todos estiverem 1 → a chave PROVAVELMENTE foi inserida
 *   (pode ser falso positivo por colisão de bits de outra chave).
 *   &  — AND bit-a-bit; o resultado é != 0 somente se o bit estiver ligado. */
int verificarBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);
 
    if (!(bf->bits[h1 / 8] & (1 << (h1 % 8)))) return 0; /* bit 1 apagado → não existe */
    if (!(bf->bits[h2 / 8] & (1 << (h2 % 8)))) return 0; /* bit 2 apagado → não existe */
    if (!(bf->bits[h3 / 8] & (1 << (h3 % 8)))) return 0; /* bit 3 apagado → não existe */
 
    return 1; /* todos os bits ligados → provavelmente existe */
}
 
 
/* ============================================================
 *  FUNÇÕES DE SIMULAÇÃO DE CONDIÇÕES ADVERSAS
 * ============================================================ */
 
/* [SIMULAÇÃO R16] — Corrompe ~10 % dos registos na lista.
 * srand(time(NULL)): inicializa o gerador pseudoaleatório com o
 * timestamp Unix atual, garantindo sequências diferentes a cada execução.
 * rand() % 10 == 0: probabilidade 1/10 de corrupção por nó. */
void corromperDados(NoLista* cabeca) {
    srand(time(NULL));
    int     afetados = 0;
    NoLista* atual   = cabeca;
    while (atual != NULL) {
        if (rand() % 10 == 0) { /* ~10 % de chance */
            atual->jogador.overall = 999;   /* valor impossível (>99) como marcador */
            atual->jogador.salario = -50000;/* salário negativo como marcador */
            afetados++;
        }
        atual = atual->prox;
    }
    printf("[SIMULACAO] Alerta: %d registos sofreram corrupcao (Anomalias inseridas)!\n", afetados);
}
 
/* Exibe jogadores que atendem a um critério mínimo.
 * Protege contra dados corrompidos (R16) ao rejeitar overall > 99. */
void filtrarJogadores(NoLista* cabeca) {
    if (cabeca == NULL) return;
    int criterio, valorLimite;
 
    printf("\n=== FILTRAGEM (Tolerante a Falhas R16) ===\n");
    printf("1. Filtrar por Overall Minimo\n");
    printf("2. Filtrar por Salario Minimo\n");
    printf("Escolha: ");
 
    if (scanf("%d", &criterio) != 1) {
        while (getchar() != '\n'); /* descarta entrada inválida do buffer */
        return;
    }
 
    if (criterio == 1)      printf("Overall minimo pretendido: ");
    else if (criterio == 2) printf("Salario minimo (EUR): ");
    else                    return;
 
    if (scanf("%d", &valorLimite) != 1) {
        while (getchar() != '\n');
        return;
    }
    getchar(); /* descarta o '\n' residual deixado pelo scanf */
 
    printf("\n%-25s | %-7s | %-10s\n", "Nome", "Overall", "Salario");
    printf("--------------------------------------------------\n");
 
    NoLista* atual     = cabeca;
    int      encontrados = 0;
    while (atual != NULL) {
        int atende = 0;
        /* overall <= 99 filtra os registos corrompidos pela R16 (999) */
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
 
/* Exibe o TOP 11 de cada liga usando a Pilha para ordenação inversa.
 * Estratégia: varredura da lista → insertion sort no array top11[11]
 * (mantém os 11 maiores overalls) → empilha todos → desempilha
 * (o pop() inverte a ordem, exibindo do maior para o menor). */
void exibirTop11PorLiga(NoLista* cabeca, InfoLiga* ligas) {
    if (cabeca == NULL) return;
 
    for (int L = 0; L < 5; L++) {
        char* nomeDaLiga = ligas[L].nome_liga;
        if (ligas[L].contador == 0) continue; /* pula ligas sem jogadores */
 
        Jogador top11[11];
        for (int i = 0; i < 11; i++) top11[i].overall = -1; /* -1 = vaga vazia */
 
        NoLista* atual = cabeca;
        while (atual != NULL) {
            /* Rejeita corrompidos (overall > 99) e jogadores de outra liga */
            if (strcmp(atual->jogador.liga, nomeDaLiga) == 0
                && atual->jogador.overall <= 99) {
                /* Insertion sort simplificado: insere na posição correta */
                for (int i = 0; i < 11; i++) {
                    if (atual->jogador.overall > top11[i].overall) {
                        /* Desloca elementos menores para a esquerda */
                        for (int j = 0; j < i; j++) top11[j] = top11[j + 1];
                        top11[i] = atual->jogador;
                        break;
                    }
                }
            }
            atual = atual->prox;
        }
 
        /* Empilha os 11 jogadores (do pior para o melhor do array) */
        NoPilha* pilhaTop11 = NULL;
        for (int i = 0; i < 11; i++) {
            if (top11[i].overall != -1)
                push(&pilhaTop11, top11[i]);
        }
 
        /* Desempilha: exibe do melhor para o pior (LIFO inverte a ordem) */
        printf("\n=== TOP 11 DA PILHA - %s ===\n", nomeDaLiga);
        int posicao = 1;
        while (pilhaTop11 != NULL) {
            Jogador j = pop(&pilhaTop11);
            printf("%2d. %-20s | Overall: %d | Idade: %d\n",
                   posicao++, j.nome, j.overall, j.idade);
        }
    }
}
 
/* Exibe estatísticas globais e por liga calculadas durante o carregamento.
 * Os dados (somas e contadores) foram acumulados em carregarDataset();
 * esta função apenas formata e imprime as médias. */
void exibirEstatisticas(EstatisticasGlobais* stats, InfoLiga* ligas) {
    printf("\n======================================================\n");
    printf("           ESTATISTICAS GERAIS E POR LIGA              \n");
    printf("======================================================\n");
 
    if (stats->contador == 0) {
        printf("Nenhum jogador carregado.\n");
        return;
    }
 
    printf("\n>> GERAL (%d jogadores no sistema)\n", stats->contador);
    /* Cast para double antes da divisão evita divisão inteira (truncamento) */
    printf(" - Overall medio: %.2f\n", (double)stats->soma_overall / stats->contador);
    printf(" - Idade media:   %.2f\n", (double)stats->soma_idade   / stats->contador);
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
        double overallMedio = (double)ligas[i].soma_overall / ligas[i].contador;
        double idadeMedia   = (double)ligas[i].soma_idade   / ligas[i].contador;
        double salarioMedio = (double)ligas[i].soma_salario / ligas[i].contador;
        printf("%-28s | %-9d | %-7.2f | %-6.2f | EUR %-10.2f\n",
               ligas[i].nome_liga, ligas[i].contador,
               overallMedio, idadeMedia, salarioMedio);
    }
    printf("======================================================\n");
}
 
 
/* ============================================================
 *  MÓDULO DE BENCHMARK EXPANDIDO
 *  Mede o desempenho real das estruturas sob carga.
 * ============================================================ */
 
/* 3. ESCALABILIDADE — mede o tempo de varredura da Lista Encadeada
 * para tamanhos crescentes de N, evidenciando crescimento O(N):
 * dobrar N deve aproximadamente dobrar o tempo. */
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel) {
    int tamanhosBase[] = {1000, 2500, 5000, 10000};
    int tamanhos[5];
    int numTamanhos = 0;
 
    /* Adiciona apenas tamanhos menores que o total real disponível */
    for (int i = 0; i < 4; i++) {
        if (tamanhosBase[i] < totalDisponivel)
            tamanhos[numTamanhos++] = tamanhosBase[i];
    }
    tamanhos[numTamanhos++] = totalDisponivel; /* garante o tamanho real como ponto final */
 
    printf("\n>> 3. ESCALABILIDADE (varredura O(N), 200 repeticoes por tamanho)\n");
    printf("%-15s | %15s\n", "Tamanho (N)", "Tempo (ms)");
    printf("----------------------------------------\n");
 
    for (int t = 0; t < numTamanhos; t++) {
        int limite = tamanhos[t];
        if (limite <= 0) continue;
 
        clock_t inicio = clock(); /* snapshot do clock antes do loop */
        for (int rep = 0; rep < 200; rep++) {
            NoLista* atual   = lista;
            int      contagem = 0;
            /* Percorre até "limite" nós — simula uma busca que para no N-ésimo elemento */
            while (atual != NULL && contagem < limite) {
                contagem++;
                atual = atual->prox;
            }
        }
        clock_t fim   = clock();
        /* Conversão de ticks de CPU para milissegundos:
         * (ticks / CLOCKS_PER_SEC) = segundos; * 1000 = ms */
        double  tempo = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
        printf("%-15d | %15.3f\n", limite, tempo);
    }
    printf("(O crescimento deve ser aproximadamente linear, coerente com O(N) da Lista)\n");
}
 
/* 4. TEMPO DE INSERÇÃO E REMOÇÃO — cria N jogadores sintéticos,
 * insere em todas as estruturas, mede; depois remove, mede.
 * O dataset real não é alterado pois os registos sintéticos são
 * identificados por nomes únicos ("BenchPlayer_i"). */
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela,
                               NoTrie* trie, BloomFilter* bf, int n) {
    clock_t inicio, fim;
 
    /* Aloca array temporário de jogadores sintéticos */
    Jogador* sinteticos = (Jogador*)malloc(n * sizeof(Jogador));
    if (sinteticos == NULL) {
        printf("\n[ERRO] Sem memoria para o benchmark de insercao/remocao.\n");
        return;
    }
 
    /* Preenche os jogadores sintéticos com dados fixos */
    for (int i = 0; i < n; i++) {
        sprintf(sinteticos[i].nome, "BenchPlayer_%d", i); /* sprintf: formata string com número */
        strcpy(sinteticos[i].time,  "BenchTeam");
        strcpy(sinteticos[i].liga,  "BenchLeague");
        sinteticos[i].idade   = 20;
        sinteticos[i].overall = 70;
        sinteticos[i].salario = 10000;
    }
 
    /* Mede o tempo de inserção em todas as 4 estruturas */
    inicio = clock();
    for (int i = 0; i < n; i++) {
        inserirLista(lista, sinteticos[i]);
        inserirHash(tabela, sinteticos[i]);
        inserirTrie(trie, sinteticos[i].nome);
        inserirBloom(bf, sinteticos[i].nome);
    }
    fim = clock();
    double tempoInsercao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* Mede o tempo de remoção nas 3 estruturas que suportam remoção.
     * O Bloom Filter NÃO participa: seus bits podem ser compartilhados
     * com outras chaves (colisão de hash), então desligá-los causaria
     * falsos negativos para jogadores reais. */
    inicio = clock();
    for (int i = 0; i < n; i++) {
        removerDaLista(lista, sinteticos[i].nome);
        removerHash(tabela, sinteticos[i].nome);
        desmarcarTrie(trie, sinteticos[i].nome);
    }
    fim = clock();
    double tempoRemocao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    printf("\n>> 4. TEMPO DE INSERCAO E REMOCAO (%d registos sinteticos)\n", n);
    printf(" - Insercao (Lista+Hash+Trie+Bloom): %10.3f ms  (%.5f ms/registo)\n",
           tempoInsercao, tempoInsercao / n);
    printf(" - Remocao  (Lista+Hash+Trie):       %10.3f ms  (%.5f ms/registo)\n",
           tempoRemocao, tempoRemocao / n);
    printf("(Os registos sinteticos sao removidos ao final; o dataset real nao e alterado.\n");
    printf(" A Trie mantem nos 'mortos' dos prefixos testados, pois desmarcarTrie() nao\n");
    printf(" libera memoria - limitacao conhecida e documentada da estrutura.)\n");
 
    free(sinteticos); /* libera o array temporário */
}
 
/* 5. TAXA DE COLISÃO DA TABELA HASH — métricas que caracterizam a
 * qualidade da função de hash e a distribuição dos elementos. */
void analisarColisoesHash(NoHash** tabela) {
    int bucketsOcupados  = 0;
    int totalElementos   = 0;
    int maiorCadeia      = 0;
 
    /* Percorre todos os buckets contando elementos e cadeias */
    for (int i = 0; i < HASH_SIZE; i++) {
        int     tamanhoCadeia = 0;
        NoHash* atual         = tabela[i];
        while (atual != NULL) { tamanhoCadeia++; atual = atual->prox; }
 
        if (tamanhoCadeia > 0) {
            bucketsOcupados++;
            totalElementos += tamanhoCadeia;
            if (tamanhoCadeia > maiorCadeia) maiorCadeia = tamanhoCadeia;
        }
    }
 
    /* Colisões = elementos que não são o primeiro em seu bucket */
    int    colisoesTotais = totalElementos - bucketsOcupados;
    /* Fator de carga (load factor): média de elementos por bucket total.
     * Valores próximos de 1 indicam boa distribuição. */
    double fatorCarga  = (double)totalElementos / HASH_SIZE;
    /* Tamanho médio de cadeia: média apenas nos buckets ocupados. */
    double cadeiaMedia = bucketsOcupados > 0
                         ? (double)totalElementos / bucketsOcupados
                         : 0.0;
 
    printf("\n>> 5. TAXA DE COLISAO DA TABELA HASH (%d posicoes)\n", HASH_SIZE);
    printf(" - Elementos inseridos:        %d\n", totalElementos);
    printf(" - Posicoes (buckets) usadas:  %d\n", bucketsOcupados);
    printf(" - Fator de carga (load factor): %.4f\n", fatorCarga);
    printf(" - Colisoes totais (elementos extras em buckets ja ocupados): %d\n", colisoesTotais);
    printf(" - Maior cadeia (pior caso de busca): %d elementos\n", maiorCadeia);
    printf(" - Tamanho medio de cadeia (so buckets ocupados): %.2f\n", cadeiaMedia);
}
 
/* 6. USO DE MEMÓRIA — estimativa estática do consumo de RAM de cada estrutura.
 * Para a Trie, usa contarNosTrie() pois o tamanho real depende dos prefixos
 * compartilhados — não é simplesmente proporcional ao número de jogadores. */
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie) {
    /* sizeof(NoLista) inclui o Jogador embutido + o ponteiro "prox" */
    long memLista     = (long)totalJogadores * (long)sizeof(NoLista);
    long memHashNos   = (long)totalJogadores * (long)sizeof(NoHash);
    /* O vetor de ponteiros da tabela hash existe independentemente do número de elementos */
    long memHashTabela = (long)HASH_SIZE * (long)sizeof(NoHash*);
    long nosTrie      = contarNosTrie(raizTrie);
    /* Cada nó da Trie ocupa 128 * sizeof(ponteiro) + int = custo alto por nó */
    long memTrie      = nosTrie * (long)sizeof(NoTrie);
    long memBloom     = (long)BLOOM_ARRAY_SIZE * (long)sizeof(unsigned char);
    long total        = memLista + memHashNos + memHashTabela + memTrie + memBloom;
 
    printf("\n>> 6. USO DE MEMORIA ESTIMADO (%d jogadores carregados)\n", totalJogadores);
    printf("%-24s | %14s | %10s\n", "Estrutura", "Bytes", "KB");
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "Lista Encadeada",   memLista,      memLista      / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (nos)", memHashNos,    memHashNos    / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (vetor)",memHashTabela,memHashTabela / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Trie",              memTrie,       memTrie       / 1024.0);
    printf("  (Trie possui %ld nos alocados; cada no = %zu bytes devido aos 128 ponteiros)\n",
           nosTrie, sizeof(NoTrie));
    printf("%-24s | %14ld | %10.1f\n", "Bloom Filter (fixo)", memBloom,    memBloom      / 1024.0);
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "TOTAL ESTIMADO",    total,         total         / 1024.0);
}
 
 
/* ============================================================
 *  MÓDULO PRINCIPAL DE BENCHMARKS
 *  Orquestra todos os testes de desempenho e exibe um relatório
 *  completo, respeitando as simulações R9, R12 e R21 ativas.
 * ============================================================ */
void executarBenchmarks(NoLista* lista, NoHash** tabela, NoTrie* raizTrie,
                        BloomFilter* bf, Simulacao* sim, int totalJogadores) {
    clock_t inicio, fim;
    double  tempo_lista, tempo_hash, tempo_bloom_falso, tempo_hash_falso;
 
    /* Se R12 (latência) estiver ativa, reduz iterações para não bloquear
     * o terminal: 100 buscas × 1 ms = 100 ms em vez de 10 000 ms. */
    int iteracoes = sim->r12_latencia ? 100 : 10000;
 
    /* Nome de busca bem-sucedida (existe no dataset) e nome falso (não existe).
     * Estes valores de teste são fixos para garantir comparabilidade. */
    char* nomeBusca = "L. Messi";
    char* nomeFalso = "Jogador Inexistente 999";
 
    printf("\n======================================================\n");
    printf("        BENCHMARKS DE DESEMPENHO (%d Iteracoes)       \n", iteracoes);
    if (sim->r9_cpu)        printf("        [!] AVISO: Estrangulamento de CPU ATIVO\n");
    if (sim->r12_latencia)  printf("        [!] AVISO: Latencia de Servidor ATIVA\n");
    if (sim->r21_algoritmo) printf("        [!] AVISO: Hash Desativada (Algoritmo Lento Forcado)\n");
    printf("======================================================\n");
 
    /* TESTE 1 — Busca bem-sucedida na Lista Encadeada (O(N)):
     * percorre a lista até encontrar o nome ou chegar ao fim. */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        NoLista* atual = lista;
        while (atual != NULL) {
            if (strcmp(atual->jogador.nome, nomeBusca) == 0) break;
            atual = atual->prox;
        }
    }
    fim         = clock();
    tempo_lista = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* TESTE 2 — Busca bem-sucedida na Hash (O(1) normal, O(N) com R21):
     * quando R21 está ativa, força pesquisa linear como degradação de algoritmo. */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        if (sim->r21_algoritmo) {
            /* R21: ignora a Hash, força varredura na Lista */
            NoLista* atual = lista;
            while (atual != NULL) {
                if (strcmp(atual->jogador.nome, nomeBusca) == 0) break;
                atual = atual->prox;
            }
        } else {
            buscarHash(tabela, nomeBusca, sim); /* busca O(1) normal */
        }
    }
    fim        = clock();
    tempo_hash = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* TESTE 3 — Rejeição de nome inexistente na Hash:
     * percorre o bucket completo sem encontrar; mede o custo de uma busca negativa. */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) buscarHash(tabela, nomeFalso, sim);
    fim              = clock();
    tempo_hash_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* TESTE 4 — Rejeição de nome inexistente pelo Bloom Filter:
     * verifica apenas os 3 bits; muito mais rápido que qualquer busca real. */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) verificarBloom(bf, nomeFalso);
    fim               = clock();
    tempo_bloom_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* Exibe os resultados dos 4 testes básicos */
    printf(">> 1. TEMPO DE BUSCA BEM-SUCEDIDA (\"%s\")\n", nomeBusca);
    printf(" - Lista Encadeada: %10.3f ms\n", tempo_lista);
    printf(" - Tabela Hash:     %10.3f ms\n", tempo_hash);
 
    printf("\n>> 2. REJEICAO DE DADO INEXISTENTE (\"%s\")\n", nomeFalso);
    printf(" - Tentar na Hash:  %10.3f ms\n", tempo_hash_falso);
    printf(" - Filtro de Bloom: %10.3f ms\n", tempo_bloom_falso);
    printf(" (Hash e Bloom respondem aqui a mesma pergunta - 'este nome existe?' - por isso\n");
    printf("  a comparacao e valida; o Bloom NAO substitui a Hash para recuperar os dados\n");
    printf("  completos do jogador, apenas para descartar buscas inuteis antecipadamente.)\n");
 
    /* Testes adicionais: escalabilidade, inserção/remoção, colisões, memória */
    benchmarkEscalabilidade(lista, totalJogadores);
    /* Reduz N do benchmark de inserção/remoção quando a latência R12 está ativa */
    benchmarkInsercaoRemocao(&lista, tabela, raizTrie, bf,
                              sim->r12_latencia ? 200 : 2000);
    analisarColisoesHash(tabela);
    calcularUsoMemoria(totalJogadores, raizTrie);
 
    printf("======================================================\n");
}
 
 
/* ============================================================
 *  PARSER E GESTÃO DO DATASET
 * ============================================================ */
 
/* Retorna o índice (0–4) da liga nas 5 grandes europeias,
 * ou -1 se a liga não pertencer ao grupo monitorado. */
int verificarLiga(char* nome, InfoLiga* ligas) {
    for (int i = 0; i < 5; i++) {
        if (strcmp(nome, ligas[i].nome_liga) == 0) return i;
    }
    return -1;
}
 
/* Libera toda a memória alocada e zera os contadores.
 * Necessário antes de recarregar o dataset (opção 8→6 do menu). */
void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie,
                      BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas) {
    liberarLista(lista);
    liberarHash(tabela);
 
    /* Libera os filhos da raiz da Trie sem liberar a raiz em si
     * (a raiz é uma variável local em main, não alocada com malloc). */
    for (int i = 0; i < 128; i++) {
        if (trie->filhos[i]) {
            liberarTrie(trie->filhos[i]);
            trie->filhos[i] = NULL;
        }
    }
 
    /* Zera todos os bits do Bloom Filter (reset completo) */
    memset(bf->bits, 0, BLOOM_ARRAY_SIZE);
 
    /* Zera os contadores e somas globais */
    stats->contador    = 0;
    stats->soma_idade  = 0;
    stats->soma_overall = 0;
    stats->soma_salario = 0;
 
    /* Zera os contadores de cada liga */
    for (int i = 0; i < 5; i++) {
        ligas[i].contador    = 0;
        ligas[i].soma_overall = 0;
        ligas[i].soma_idade  = 0;
        ligas[i].soma_salario = 0;
    }
}
 
/* Lê o CSV "players_22.csv", faz parse linha a linha e popula
 * todas as estruturas de dados simultaneamente. */
void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie,
                     BloomFilter* bf, EstatisticasGlobais* stats,
                     InfoLiga* ligas, Simulacao* sim) {
    FILE* arquivo = fopen("players_22.csv", "r");
    if (!arquivo) {
        printf("\n[ERRO] O ficheiro 'players_22.csv' nao foi encontrado!\n");
        return;
    }
 
    char linha[10000]; /* buffer grande o suficiente para linhas longas do CSV */
 
    /* Descarta a primeira linha (cabeçalho com nomes das colunas) */
    fgets(linha, sizeof(linha), arquivo);
 
    while (fgets(linha, sizeof(linha), arquivo)) {
 
        /* [SIMULAÇÃO R2] — Para de carregar após 5 000 registos para simular
         * limitação de RAM. */
        if (sim->r2_memoria && stats->contador >= 5000) break;
 
        /* Parser manual de CSV com suporte a campos entre aspas.
         * Variáveis:
         *   col          — índice da coluna atual
         *   j            — índice de escrita no buffer do campo atual
         *   dentro_aspas — flag: 1 se estamos dentro de "...", 0 caso contrário
         *   buffer       — armazena o conteúdo do campo atual */
        int  col = 0, j = 0, dentro_aspas = 0, i;
        char buffer[256];
        Jogador jog = {"", "Agente Livre", "Sem Liga", 0, 0, 0};
 
        for (i = 0; linha[i] != '\0' && linha[i] != '\n'; i++) {
            /* Alterna o estado "dentro_aspas" ao encontrar aspas;
             * o caractere de aspa em si não é copiado para o buffer. */
            if (linha[i] == '"') { dentro_aspas = !dentro_aspas; continue; }
 
            /* Vírgula fora de aspas = separador de campo */
            if (linha[i] == ',' && !dentro_aspas) {
                buffer[j] = '\0'; /* termina a string do campo */
 
                /* Mapeamento de colunas do CSV (índices fixos do players_22.csv) */
                if      (col == 2)  strncpy(jog.nome,  buffer, 99); /* nome curto  */
                else if (col == 5)  jog.overall = atoi(buffer);      /* overall     */
                else if (col == 8)  jog.salario = atoi(buffer);      /* wage_eur    */
                else if (col == 9)  jog.idade   = atoi(buffer);      /* age         */
                else if (col == 14) strncpy(jog.time,  buffer, 99);  /* club_name   */
                else if (col == 15) strncpy(jog.liga,  buffer, 49);  /* league_name */
 
                j = 0; /* reinicia o buffer para o próximo campo */
                col++;
            } else if (j < 255) {
                /* Acumula caractere no buffer; limite 255 evita overflow */
                buffer[j++] = linha[i];
            }
        }
 
        /* Só insere jogadores com nome não-vazio */
        if (strlen(jog.nome) > 0) {
            inserirLista(cabecaLista, jog);
            inserirHash(tabela, jog);
            inserirTrie(raizTrie, jog.nome);
            inserirBloom(bf, jog.nome);
 
            /* Atualiza estatísticas globais acumuladas */
            stats->soma_overall += jog.overall;
            stats->soma_idade   += jog.idade;
            stats->soma_salario += jog.salario;
            stats->contador++;
 
            /* Atualiza estatísticas da liga, se for uma das 5 monitoradas */
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
 *  FUNÇÃO PRINCIPAL — LOOP DE MENU
 * ============================================================ */
int main() {
    /* Inicialização das estruturas de dados */
    NoLista*  listaJogadores = NULL;
    NoTrie*   raizTrie       = criarNoTrie();
    BloomFilter* filtroBloom = criarBloomFilter();
 
    /* Array estático de ponteiros para os buckets da hash;
     * cada posição começa NULL (bucket vazio). */
    NoHash* tabelaHash[HASH_SIZE];
    for (int i = 0; i < HASH_SIZE; i++) tabelaHash[i] = NULL;
 
    /* Inicializa estruturas de estatísticas */
    EstatisticasGlobais statsGlobais = {0, 0, 0, 0};
    InfoLiga ligas[5];
    char* nomes_ligas[] = {
        "English Premier League",
        "Spain Primera Division",
        "Italian Serie A",
        "German 1. Bundesliga",
        "French Ligue 1"
    };
    for (int i = 0; i < 5; i++) strcpy(ligas[i].nome_liga, nomes_ligas[i]);
 
    /* Todas as simulações começam desligadas */
    Simulacao sim = {0, 0, 0, 0, 0};
 
    printf("A ler a Base de Dados. Aguarde...\n");
    carregarDataset(&listaJogadores, tabelaHash, raizTrie,
                    filtroBloom, &statsGlobais, ligas, &sim);
 
    int  opcao = 0;
    char prefixo[100];
    char nomeCompleto[100]; /* resultado do autocomplete da Trie */
 
    /* Loop principal: continua até o usuário escolher opção 9 (Sair) */
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
 
        /* Verifica se scanf leu um inteiro; se não, limpa o buffer e recomeça */
        if (scanf("%d", &opcao) != 1) { while (getchar() != '\n'); continue; }
        getchar(); /* consome o '\n' residual do Enter para não contaminar fgets() posteriores */
 
        switch (opcao) {
 
            /* -------------------------------------------------- */
            case 1: { /* INSERÇÃO MANUAL */
                Jogador novo = {"", "Manual", "N/A", 22, 0, 0};
                printf("Nome: ");
                fgets(novo.nome, sizeof(novo.nome), stdin);
                /* strcspn retorna o índice do primeiro '\n' na string;
                 * substituímos por '\0' para remover o Enter do fgets. */
                novo.nome[strcspn(novo.nome, "\n")] = 0;
                printf("Overall: ");
                scanf("%d", &novo.overall);
                getchar();
 
                /* Insere em todas as estruturas simultaneamente */
                inserirLista(&listaJogadores, novo);
                inserirHash(tabelaHash, novo);
                inserirTrie(raizTrie, novo.nome);
                inserirBloom(filtroBloom, novo.nome);
 
                /* Mantém as estatísticas globais coerentes */
                statsGlobais.contador++;
                statsGlobais.soma_overall += novo.overall;
                statsGlobais.soma_idade   += novo.idade;
                statsGlobais.soma_salario += novo.salario;
 
                printf("[SUCESSO] Inserido!\n");
                break;
            }
 
            /* -------------------------------------------------- */
            case 2: { /* BUSCA COM AUTOCOMPLETE */
                printf("\n--- BUSCA RAPIDA ---\n");
                printf("Nome/Prefixo: ");
                fgets(prefixo, sizeof(prefixo), stdin);
                prefixo[strcspn(prefixo, "\n")] = 0;
 
                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);
 
                    /* O Bloom Filter é consultado primeiro como "porteiro":
                     * se disser NÃO (0 bits apagados), descarta a busca sem
                     * tocar na Hash — economia de tempo em buscas negativas. */
                    if (!sim.r21_algoritmo &&
                        verificarBloom(filtroBloom, nomeCompleto) == 0) {
                        printf("[BLOOM FILTER] DEFINITIVAMENTE NAO EXISTE.\n");
                    } else {
                        Jogador* achado = NULL;
                        if (sim.r21_algoritmo) {
                            /* R21 ativa: força varredura linear em vez de usar a Hash */
                            printf("[AVISO R21] A usar pesquisa linear...\n");
                            NoLista* atual = listaJogadores;
                            while (atual) {
                                if (strcmp(atual->jogador.nome, nomeCompleto) == 0) {
                                    achado = &(atual->jogador);
                                    break;
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
                } else {
                    printf("[TRIE] Nao encontrado.\n");
                }
                break;
            }
 
            /* -------------------------------------------------- */
            case 3: { /* REMOÇÃO COM AUTOCOMPLETE */
                printf("\n--- REMOCAO DE JOGADOR ---\n");
                printf("Nome/Prefixo: ");
                fgets(prefixo, sizeof(prefixo), stdin);
                prefixo[strcspn(prefixo, "\n")] = 0;
 
                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);
 
                    /* Busca na Hash primeiro para obter uma cópia dos dados
                     * antes de desalocar o nó (necessário para atualizar stats). */
                    Jogador* alvo = buscarHash(tabelaHash, nomeCompleto, &sim);
                    if (alvo != NULL) {
                        Jogador copia = *alvo; /* cópia por valor antes de liberar o nó */
 
                        int okLista = removerDaLista(&listaJogadores, nomeCompleto);
                        int okHash  = removerHash(tabelaHash, nomeCompleto);
                        desmarcarTrie(raizTrie, nomeCompleto);
                        /* Bloom Filter NÃO é alterado: bits compartilhados entre chaves
                         * tornam a remoção insegura (causaria falsos negativos). */
 
                        if (okLista && okHash) {
                            /* Atualiza estatísticas globais subtraindo os dados do removido */
                            statsGlobais.contador--;
                            statsGlobais.soma_overall -= copia.overall;
                            statsGlobais.soma_idade   -= copia.idade;
                            statsGlobais.soma_salario -= copia.salario;
 
                            /* Atualiza estatísticas da liga correspondente */
                            int idx_liga = verificarLiga(copia.liga, ligas);
                            if (idx_liga != -1) {
                                ligas[idx_liga].contador--;
                                ligas[idx_liga].soma_overall -= copia.overall;
                                ligas[idx_liga].soma_idade   -= copia.idade;
                                ligas[idx_liga].soma_salario -= copia.salario;
                            }
                            printf("[SUCESSO] \"%s\" removido da Lista, Hash e Trie.\n", nomeCompleto);
                            printf("[INFO] Bit do Bloom Filter mantido (limitacao estrutural).\n");
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
 
            /* Opções diretas que delegam para funções específicas */
            case 4: exibirEstatisticas(&statsGlobais, ligas); break;
            case 5: exibirTop11PorLiga(listaJogadores, ligas); break;
            case 6: filtrarJogadores(listaJogadores);          break;
            case 7: executarBenchmarks(listaJogadores, tabelaHash, raizTrie,
                                       filtroBloom, &sim, statsGlobais.contador);
                    break;
 
            /* -------------------------------------------------- */
            case 8: { /* PAINEL DE SIMULAÇÃO */
                int opSim = 0;
                while (opSim != 7) {
                    printf("\n=== PAINEL DE SIMULACAO (CONDICOES ADVERSAS) ===\n");
                    printf("1. [R2] Limitar Memoria RAM a 5000 Registos:  %s\n", sim.r2_memoria    ? "ON" : "OFF");
                    printf("2. [R9] Estrangular CPU (Hash Demorada):      %s\n", sim.r9_cpu        ? "ON" : "OFF");
                    printf("3. [R12] Latencia de Rede (Servidor Lento):   %s\n", sim.r12_latencia  ? "ON" : "OFF");
                    printf("4. [R16] Injetar Virus (Corromper Dados):     %s\n", sim.r16_dados     ? "ON" : "OFF");
                    printf("5. [R21] Degradar Algoritmo (Desligar Hash):  %s\n", sim.r21_algoritmo ? "ON" : "OFF");
                    printf("6. RECARREGAR BASE DE DADOS (Aplica a R2)\n");
                    printf("7. Voltar ao Menu Principal\n");
                    printf("Escolha o que ligar/desligar: ");
 
                    if (scanf("%d", &opSim) != 1) { while (getchar() != '\n'); continue; }
 
                    /* ! (NOT lógico) alterna a flag de 0 para 1 ou de 1 para 0 */
                    if      (opSim == 1) sim.r2_memoria    = !sim.r2_memoria;
                    else if (opSim == 2) sim.r9_cpu        = !sim.r9_cpu;
                    else if (opSim == 3) sim.r12_latencia  = !sim.r12_latencia;
                    else if (opSim == 4) { sim.r16_dados = 1; corromperDados(listaJogadores); }
                    else if (opSim == 5) sim.r21_algoritmo = !sim.r21_algoritmo;
                    else if (opSim == 6) {
                        /* Limpa todas as estruturas e recarrega do CSV,
                         * aplicando as restrições atualmente ativas (ex.: R2). */
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
 
    /* Libera toda a memória antes de encerrar, evitando memory leaks */
    limparEstruturas(&listaJogadores, tabelaHash, raizTrie,
                     filtroBloom, &statsGlobais, ligas);
    free(raizTrie);    /* a raiz da Trie foi alocada com malloc em main */
    free(filtroBloom); /* o Bloom Filter foi alocado com malloc em main */
    return 0;
}
