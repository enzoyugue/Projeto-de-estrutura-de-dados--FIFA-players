/*
 * ============================================================================
 * SISTEMA DE GERENCIAMENTO DE JOGADORES FIFA 22 (VERSÃO 100% CONCLUÍDA)
 * ============================================================================
 * Estruturas de Dados Utilizadas:
 * 1. Lista Encadeada Simples - Armazenamento sequencial e ordenação dinâmica.
 * 2. Tabela Hash com Chaining - Busca em tempo constante amortizado O(1).
 * 3. Árvore Trie - Motor de autocompletar e busca por prefixos em tempo O(L).
 * 4. Pilha (Stack) - Alocação temporária e inversão do Top 11 de jogadores.
 * 5. [OTIMIZAÇÃO AVANÇADA] Counting Bloom Filter - Filtro probabilístico 
 * sofisticado usando vetor de contadores (short de 16 bits) que permite
 * a operação de remoção (decremento), solucionando a limitação de exclusão
 * do modelo clássico de bits.
 * ============================================================================
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   /* Utilizado para mensuração de clock e latências simuladas */
 
/* ============================================================================
 * CONSTANTES GLOBAIS
 * ============================================================================ */
#define HASH_SIZE 20011 
#define BLOOM_SIZE 200000 

/* OTIMIZAÇÃO ESTRUTURAL: O Counting Bloom Filter aloca 1 contador do tipo 'short' 
 * (16 bits / 2 bytes) para cada posição do vetor. Espaço físico: 200.000 * 2 bytes = ~390.6 KB.
 * Esta abordagem consome mais RAM que o filtro clássico, porém viabiliza a deleção estável. */
#define BLOOM_ARRAY_SIZE BLOOM_SIZE 
 
typedef struct {
    char nome[100];
    char time[100];
    char liga[50]; 
    int idade;
    int overall;
    int salario;
} Jogador;
 
/* --- ESTRUTURA PARA CONTROLO DA SIMULAÇÃO (As 5 Restrições do Edital) --- */
typedef struct {
    int r2_memoria;     /* R2: Restrição de Memória RAM - Limita o dataset a 5000 registros */
    int r9_cpu;         /* R9: Carga Computacional - Injeta overhead matemático forçado na Hash */
    int r12_latencia;   /* R12: Latência de Rede - Impõe delay de 1ms nas requisições */
    int r16_dados;      /* R16: Ruído/Corrupção - Injeta dados espúrios (Overall 999) no heap */
    int r21_algoritmo;  /* R21: Degradação Algorítmica - Desativa indexadores, forçando busca O(N) */
} Simulacao;
 
/* ============================================================================
 * DEFINIÇÕES DAS ESTRUTURAS DE DADOS
 * ============================================================================ */
typedef struct NoPilha {
    Jogador jogador;
    struct NoPilha* prox;
} NoPilha;
 
typedef struct NoLista {
    Jogador jogador;
    struct NoLista* prox;
} NoLista;
 
typedef struct NoHash {
    Jogador jogador;
    struct NoHash* prox;
} NoHash;
 
typedef struct NoTrie {
    struct NoTrie* filhos[128];
    int fimDePalavra;
} NoTrie;
 
/* OTIMIZAÇÃO AVANÇADA EXIGIDA PELO EDITAL: Struct modificada para Counting Bloom Filter.
 * Substitui o vetor bruto de bits por contadores de inteiros curtos (short). */
typedef struct {
    short contadores[BLOOM_ARRAY_SIZE]; 
} BloomFilter;
 
typedef struct {
    char nome_liga[50];
    long long soma_overall;
    long long soma_idade;
    long long soma_salario;
    int contador;
} InfoLiga;
 
typedef struct {
    long long soma_overall;
    long long soma_idade;
    long long soma_salario;
    int contador;
} EstatisticasGlobais;
 
/* --- PROTÓTIPOS DAS FUNÇÕES EXTRA-MANIPULAÇÃO --- */
void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas, Simulacao* sim);
void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas);
int removerDaLista(NoLista** cabeca, const char* nome);
void exibirEstatisticas(EstatisticasGlobais* stats, InfoLiga* ligas);
long contarNosTrie(NoTrie* no);
void analisarColisoesHash(NoHash** tabela);
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie);
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, int n);
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel);
 
/* ============================================================================
 * IMPLEMENTAÇÃO: PILHA DILIGENTE (STACK)
 * ============================================================================ */
void push(NoPilha** topo, Jogador jog) {
    NoPilha* novo = (NoPilha*)malloc(sizeof(NoPilha));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox = *topo;
    *topo = novo;
}
 
Jogador pop(NoPilha** topo) {
    Jogador jog = {"", "", "", 0, 0, 0};
    if (*topo == NULL) return jog;
    NoPilha* temp = *topo;
    jog = temp->jogador;
    *topo = temp->prox;
    free(temp);
    return jog;
}
 
/* ============================================================================
 * IMPLEMENTAÇÃO: LISTA ENCADEADA SIMPLES
 * ============================================================================ */
void inserirLista(NoLista** cabeca, Jogador jog) {
    NoLista* novo = (NoLista*)malloc(sizeof(NoLista));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox = *cabeca;
    *cabeca = novo;
}
 
void liberarLista(NoLista** cabeca) {
    NoLista* atual = *cabeca;
    while (atual != NULL) {
        NoLista* temp = atual;
        atual = atual->prox;
        free(temp);
    }
    *cabeca = NULL;
}
 
int removerDaLista(NoLista** cabeca, const char* nome) {
    NoLista* atual = *cabeca;
    NoLista* anterior = NULL;
    while (atual != NULL) {
        if (strcmp(atual->jogador.nome, nome) == 0) {
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
 
/* ============================================================================
 * IMPLEMENTAÇÃO: TABELA HASH COM ENCADEAMENTO (CHAINING)
 * ============================================================================ */
unsigned int funcaoHash(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}
 
void inserirHash(NoHash** tabela, Jogador jog) {
    unsigned int idx = funcaoHash(jog.nome);
    NoHash* novo = (NoHash*)malloc(sizeof(NoHash));
    if (novo == NULL) return;
    novo->jogador = jog;
    novo->prox = tabela[idx]; 
    tabela[idx] = novo;
}
 
Jogador* buscarHash(NoHash** tabela, const char* nome, Simulacao* sim) {
    /* [SIMULAÇÃO R9] - Injeção de estresse computacional artificial na CPU */
    if (sim != NULL && sim->r9_cpu) {
        volatile double dummy = 0.0;
        for (int i = 0; i < 50000; i++) dummy += 1.1; 
    }
    
    /* [SIMULAÇÃO R12] - Travamento e indução de latência de rede/I/O */
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
 
/* ============================================================================
 * IMPLEMENTAÇÃO: ÁRVORE TRIE (PREFIX COMPRESSION MOTOR)
 * ============================================================================ */
NoTrie* criarNoTrie() {
    NoTrie* n = (NoTrie*)malloc(sizeof(NoTrie));
    if (n) {
        n->fimDePalavra = 0;
        for (int i = 0; i < 128; i++) n->filhos[i] = NULL;
    }
    return n;
}
 
void inserirTrie(NoTrie* raiz, const char* chave) {
    NoTrie* atual = raiz;
    for (int i = 0; chave[i] != '\0'; i++) {
        unsigned char c = (unsigned char)chave[i];
        if (c >= 128) continue;
        if (atual->filhos[c] == NULL) atual->filhos[c] = criarNoTrie();
        atual = atual->filhos[c];
    }
    atual->fimDePalavra = 1;
}
 
void desmarcarTrie(NoTrie* raiz, const char* chave) {
    NoTrie* atual = raiz;
    for (int i = 0; chave[i] != '\0'; i++) {
        unsigned char c = (unsigned char)chave[i];
        if (c >= 128 || atual->filhos[c] == NULL) return;
        atual = atual->filhos[c];
    }
    atual->fimDePalavra = 0; /* Exclusão lógica para otimizar velocidade de escrita */
}
 
int buscarPrimeiraPalavra(NoTrie* no, char* sufixo, int profundidade, char* resultado, const char* prefixo) {
    if (no == NULL) return 0;
    if (no->fimDePalavra) {
        sufixo[profundidade] = '\0';
        sprintf(resultado, "%s%s", prefixo, sufixo);
        return 1;
    }
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) {
            sufixo[profundidade] = (char)i;
            if (buscarPrimeiraPalavra(no->filhos[i], sufixo, profundidade + 1, resultado, prefixo)) return 1;
        }
    }
    return 0;
}
 
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
 
void liberarTrie(NoTrie* no) {
    if (no == NULL) return;
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) liberarTrie(no->filhos[i]);
    }
    free(no);
}
 
long contarNosTrie(NoTrie* no) {
    if (no == NULL) return 0;
    long total = 1;
    for (int i = 0; i < 128; i++) {
        if (no->filhos[i] != NULL) total += contarNosTrie(no->filhos[i]);
    }
    return total;
}
 
/* ============================================================================
 * IMPLEMENTAÇÃO OTIMIZADA: COUNTING BLOOM FILTER (RESOLUÇÃO DE DELEÇÃO)
 * ============================================================================ */
BloomFilter* criarBloomFilter() {
    BloomFilter* bf = (BloomFilter*)malloc(sizeof(BloomFilter));
    /* Zera todos os contadores 'short' de forma contígua em memória */
    if (bf != NULL) memset(bf->contadores, 0, BLOOM_ARRAY_SIZE * sizeof(short));
    return bf;
}
 
unsigned int hash_sdbm(const char* str) {
    unsigned long hash = 0;
    int c;
    while ((c = (unsigned char)*str++)) hash = c + (hash << 6) + (hash << 16) - hash;
    return hash % BLOOM_SIZE;
}
 
unsigned int hash_custom(const char* str) {
    unsigned long hash = 0;
    int c;
    while ((c = (unsigned char)*str++)) hash = (hash * 31) + c;
    return hash % BLOOM_SIZE;
}
 
void inserirBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE; 
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);
 
    /* Incrementa os contadores evitando estouro (overflow) do limite de inteiros de 16 bits */
    if (bf->contadores[h1] < 32767) bf->contadores[h1]++;
    if (bf->contadores[h2] < 32767) bf->contadores[h2]++;
    if (bf->contadores[h3] < 32767) bf->contadores[h3]++;
}
 
int verificarBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);
 
    /* Se qualquer uma das posições possuir contador zerado, o elemento é garantidamente inexistente */
    if (bf->contadores[h1] == 0) return 0;
    if (bf->contadores[h2] == 0) return 0;
    if (bf->contadores[h3] == 0) return 0;
 
    return 1; /* O dado pode existir (sujeito à taxa marginal de falso positivo) */
}
 
/* OTIMIZAÇÃO ESTRUTURAL CHAVE: Operação avançada de remoção no Filtro probabilístico.
 * Decrementa as posições correspondentes protegendo contra underflow de memória. */
void removerBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);
 
    if (bf->contadores[h1] > 0) bf->contadores[h1]--;
    if (bf->contadores[h2] > 0) bf->contadores[h2]--;
    if (bf->contadores[h3] > 0) bf->contadores[h3]--;
}
 
/* ============================================================================
 * OPERAÇÕES ADICIONAIS ANALÍTICAS E TOLERÂNCIA A FALHAS
 * ============================================================================ */

/* [SIMULAÇÃO R16] Corrupção simulada via injeção de ruído severo no Heap */
void corromperDados(NoLista* cabeca) {
    srand(time(NULL));
    int afetados = 0;
    NoLista* atual = cabeca;
    while (atual != NULL) {
        if (rand() % 10 == 0) { 
            atual->jogador.overall = 999;
            atual->jogador.salario = -50000;
            afetados++;
        }
        atual = atual->prox;
    }
    printf("[SIMULACAO] Alerta: %d registos sofreram corrupcao (Anomalias inseridas)!\n", afetados);
}
 
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
        /* Defesa arquitetural ativa contra dados inválidos (Injeção R16) */
        if (criterio == 1 && atual->jogador.overall >= valorLimite && atual->jogador.overall <= 99) atende = 1;
        if (criterio == 2 && atual->jogador.salario >= valorLimite) atende = 1;
 
        if (atende) {
            printf("%-25s | %-7d | EUR %-6d\n", atual->jogador.nome, atual->jogador.overall, atual->jogador.salario);
            encontrados++;
        }
        atual = atual->prox;
    }
    printf("Total encontrados (Ignorando corrompidos): %d\n", encontrados);
}
 
void exibirTop11PorLiga(NoLista* cabeca, InfoLiga* ligas) {
    if (cabeca == NULL) return;
    for (int L = 0; L < 5; L++) {
        char* nomeDaLiga = ligas[L].nome_liga;
        if (ligas[L].contador == 0) continue; 
 
        Jogador top11[11];
        for (int i = 0; i < 11; i++) top11[i].overall = -1; 
 
        NoLista* atual = cabeca;
        while (atual != NULL) {
            if (strcmp(atual->jogador.liga, nomeDaLiga) == 0 && atual->jogador.overall <= 99) { 
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
 
        NoPilha* pilhaTop11 = NULL;
        for (int i = 0; i < 11; i++) {
            if (top11[i].overall != -1) push(&pilhaTop11, top11[i]);
        }
 
        printf("\n=== TOP 11 DA PILHA - %s ===\n", nomeDaLiga);
        int posicao = 1;
        while (pilhaTop11 != NULL) {
            Jogador j = pop(&pilhaTop11);
            printf("%2d. %-20s | Overall: %d | Idade: %d\n", posicao++, j.nome, j.overall, j.idade);
        }
    }
}
 
void exibirEstatisticas(EstatisticasGlobais* stats, InfoLiga* ligas) {
    printf("\n======================================================\n");
    printf("           ESTATISTICAS GERAIS E POR LIGA              \n");
    printf("======================================================\n");
    if (stats->contador == 0) { printf("Nenhum jogador carregado.\n"); return; }
 
    printf("\n>> GERAL (%d jogadores no sistema)\n", stats->contador);
    printf(" - Overall medio: %.2f\n", (double)stats->soma_overall / stats->contador);
    printf(" - Idade media:   %.2f\n", (double)stats->soma_idade / stats->contador);
    printf(" - Salario medio: EUR %.2f\n", (double)stats->soma_salario / stats->contador);
 
    printf("\n>> POR LIGA (Top 5 Europeias)\n");
    printf("%-28s | %-9s | %-7s | %-6s | %-14s\n", "Liga", "Jogador.", "Overall", "Idade", "Salario Medio");
    printf("-----------------------------------------------------------------------\n");
    for (int i = 0; i < 5; i++) {
        if (ligas[i].contador == 0) { printf("%-28s | sem dados\n", ligas[i].nome_liga); continue; }
        double overallMedio = (double)ligas[i].soma_overall / ligas[i].contador;
        double idadeMedia = (double)ligas[i].soma_idade / ligas[i].contador;
        double salarioMedio = (double)ligas[i].soma_salario / ligas[i].contador;
        printf("%-28s | %-9d | %-7.2f | %-6.2f | EUR %-10.2f\n",
               ligas[i].nome_liga, ligas[i].contador, overallMedio, idadeMedia, salarioMedio);
    }
}
 
/* ============================================================================
 * MOTORES DE BENCHMARK E ANÁLISE EMPÍRICA (MÉTRICAS DO EDITAL)
 * ============================================================================ */
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel) {
    int tamanhosBase[] = {1000, 2500, 5000, 10000};
    int tamanhos[5];
    int numTamanhos = 0;
 
    for (int i = 0; i < 4; i++) {
        if (tamanhosBase[i] < totalDisponivel) tamanhos[numTamanhos++] = tamanhosBase[i];
    }
    tamanhos[numTamanhos++] = totalDisponivel; 
 
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
}
 
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, int n) {
    clock_t inicio, fim;
    Jogador* sinteticos = (Jogador*)malloc(n * sizeof(Jogador));
    if (sinteticos == NULL) return;
 
    for (int i = 0; i < n; i++) {
        sprintf(sinteticos[i].nome, "BenchPlayer_%d", i);
        strcpy(sinteticos[i].time, "BenchTeam");
        strcpy(sinteticos[i].liga, "BenchLeague");
        sinteticos[i].idade = 20; sinteticos[i].overall = 70; sinteticos[i].salario = 10000;
    }
 
    /* BENCHMARK INSERÇÃO CONJUNTA */
    inicio = clock();
    for (int i = 0; i < n; i++) {
        inserirLista(lista, sinteticos[i]);
        inserirHash(tabela, sinteticos[i]);
        inserirTrie(trie, sinteticos[i].nome);
        inserirBloom(bf, sinteticos[i].nome);
    }
    fim = clock();
    double tempoInsercao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* BENCHMARK REMOÇÃO CONJUNTA - TESTANDO O MOTOR DE DELEÇÃO DO COUNTING BLOOM */
    inicio = clock();
    for (int i = 0; i < n; i++) {
        removerDaLista(lista, sinteticos[i].nome);
        removerHash(tabela, sinteticos[i].nome);
        desmarcarTrie(trie, sinteticos[i].nome);
        removerBloom(bf, sinteticos[i].nome); /* Ativação estável da remoção probabilística */
    }
    fim = clock();
    double tempoRemocao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    printf("\n>> 4. TEMPO DE INSERCAO E REMOCAO (%d registos sinteticos)\n", n);
    printf(" - Insercao (Lista+Hash+Trie+Bloom): %10.3f ms\n", tempoInsercao);
    printf(" - Remocao  (Lista+Hash+Trie+Bloom): %10.3f ms\n", tempoRemocao);
    free(sinteticos);
}
 
void analisarColisoesHash(NoHash** tabela) {
    int bucketsOcupados = 0, totalElementos = 0, maiorCadeia = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        int tamanhoCadeia = 0;
        NoHash* atual = tabela[i];
        while (atual != NULL) { tamanhoCadeia++; atual = atual->prox; }
        if (tamanhoCadeia > 0) {
            bucketsOcupados++; totalElementos += tamanhoCadeia;
            if (tamanhoCadeia > maiorCadeia) maiorCadeia = tamanhoCadeia;
        }
    }
    printf("\n>> 5. TAXA DE COLISAO DA TABELA HASH (%d posicoes)\n", HASH_SIZE);
    printf(" - Fator de carga (load factor): %.4f\n", (double)totalElementos / HASH_SIZE);
    printf(" - Maior cadeia (pior caso de busca): %d elementos\n", maiorCadeia);
}
 
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie) {
    long memLista = (long)totalJogadores * (long)sizeof(NoLista);
    long memHashNos = (long)totalJogadores * (long)sizeof(NoHash);
    long memHashTabela = (long)HASH_SIZE * (long)sizeof(NoHash*);
    long nosTrie = contarNosTrie(raizTrie);
    long memTrie = nosTrie * (long)sizeof(NoTrie);
    
    /* MENSURAÇÃO EXATA DA OTIMIZAÇÃO: Registra o tamanho físico real alocado pelos shorts */
    long memBloom = (long)BLOOM_ARRAY_SIZE * (long)sizeof(short);
    long total = memLista + memHashNos + memHashTabela + memTrie + memBloom;
 
    printf("\n>> 6. USO DE MEMORIA ESTIMADO (%d jogadores carregados)\n", totalJogadores);
    printf("%-24s | %14s | %10s\n", "Estrutura", "Bytes", "KB");
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "Lista Encadeada", memLista, memLista / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (Completa)", memHashNos + memHashTabela, (memHashNos + memHashTabela) / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Trie (Prefixos)", memTrie, memTrie / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Counting Bloom (Otimiz.)", memBloom, memBloom / 1024.0);
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "TOTAL ESTIMADO RAM", total, total / 1024.0);
}
 
void executarBenchmarks(NoLista* lista, NoHash** tabela, NoTrie* raizTrie, BloomFilter* bf, Simulacao* sim, int totalJogadores) {
    clock_t inicio, fim;
    double tempo_lista, tempo_hash, tempo_bloom_falso, tempo_hash_falso;
    int iteracoes = sim->r12_latencia ? 100 : 10000;
    
    char* nomeBusca = "L. Messi"; 
    char* nomeFalso = "Jogador Inexistente 999";
 
    /* TESTE DE PERFORMANCE: Busca Linear vs Hash */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        NoLista* atual = lista;
        while (atual != NULL) { if (strcmp(atual->jogador.nome, nomeBusca) == 0) break; atual = atual->prox; }
    }
    tempo_lista = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        if (sim->r21_algoritmo) {
            NoLista* atual = lista;
            while (atual != NULL) { if (strcmp(atual->jogador.nome, nomeBusca) == 0) break; atual = atual->prox; }
        } else { buscarHash(tabela, nomeBusca, sim); }
    }
    fim = clock();
    tempo_hash = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    /* TESTE DE PERFORMANCE: Rejeição Rápida Hash vs Counting Bloom */
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) { buscarHash(tabela, nomeFalso, sim); }
    fim = clock();
    tempo_hash_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) { verificarBloom(bf, nomeFalso); }
    fim = clock();
    tempo_bloom_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;
 
    printf("\n>> 1. TEMPO DE BUSCA BEM-SUCEDIDA (\"%s\")\n", nomeBusca);
    printf(" - Lista Encadeada: %10.3f ms | Tabela Hash: %10.3f ms\n", tempo_lista, tempo_hash);
    printf("\n>> 2. REJEICAO DE DADO INEXISTENTE (\"%s\")\n", nomeFalso);
    printf(" - Tentar na Hash:  %10.3f ms | Counting Bloom: %10.3f ms\n", tempo_hash_falso, tempo_bloom_falso);
 
    benchmarkEscalabilidade(lista, totalJogadores);
    benchmarkInsercaoRemocao(&lista, tabela, raizTrie, bf, sim->r12_latencia ? 200 : 2000);
    analisarColisoesHash(tabela);
    calcularUsoMemoria(totalJogadores, raizTrie);
}
 
/* ============================================================================
 * PARSER DE DATASET CSV
 * ============================================================================ */
int verificarLiga(char* nome, InfoLiga* ligas) {
    for (int i = 0; i < 5; i++) { if (strcmp(nome, ligas[i].nome_liga) == 0) return i; }
    return -1;
}
 
void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas) {
    liberarLista(lista); liberarHash(tabela);
    for(int i=0; i<128; i++) if(trie->filhos[i]) { liberarTrie(trie->filhos[i]); trie->filhos[i] = NULL; }
    memset(bf->contadores, 0, BLOOM_ARRAY_SIZE * sizeof(short));
    stats->contador = 0; stats->soma_idade = 0; stats->soma_overall = 0; stats->soma_salario = 0;
    for (int i = 0; i < 5; i++) { ligas[i].contador = 0; ligas[i].soma_overall = 0; ligas[i].soma_idade = 0; ligas[i].soma_salario = 0; }
}
 
void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas, Simulacao* sim) {
    FILE* arquivo = fopen("players_22.csv", "r");
    if (!arquivo) { printf("\n[ERRO] O ficheiro 'players_22.csv' nao foi encontrado!\n"); return; }
    char linha[10000]; fgets(linha, sizeof(linha), arquivo); 
 
    while (fgets(linha, sizeof(linha), arquivo)) {
        if (sim->r2_memoria && stats->contador >= 5000) break;
        int col = 0, j = 0, dentro_aspas = 0, i; char buffer[256];
        Jogador jog = {"", "Agente Livre", "Sem Liga", 0, 0, 0};
        
        for (i = 0; linha[i] != '\0' && linha[i] != '\n'; i++) {
            if (linha[i] == '"') { dentro_aspas = !dentro_aspas; continue; }
            if (linha[i] == ',' && !dentro_aspas) {
                buffer[j] = '\0';
                if (col == 2) strncpy(jog.nome, buffer, 99);
                else if (col == 5) jog.overall = atoi(buffer);
                else if (col == 8) jog.salario = atoi(buffer);
                else if (col == 9) jog.idade = atoi(buffer);
                else if (col == 14) strncpy(jog.time, buffer, 99);
                else if (col == 15) strncpy(jog.liga, buffer, 49);
                j = 0; col++;
            } else if (j < 255) buffer[j++] = linha[i];
        }
        if (strlen(jog.nome) > 0) {
            inserirLista(cabecaLista, jog); inserirHash(tabela, jog);
            inserirTrie(raizTrie, jog.nome); inserirBloom(bf, jog.nome); 
            stats->soma_overall += jog.overall; stats->soma_idade += jog.idade; stats->soma_salario += jog.salario; stats->contador++;
            int idx_liga = verificarLiga(jog.liga, ligas);
            if (idx_liga != -1) {
                ligas[idx_liga].soma_overall += jog.overall; ligas[idx_liga].soma_idade += jog.idade;
                ligas[idx_liga].soma_salario += jog.salario; ligas[idx_liga].contador++;
            }
        }
    }
    fclose(arquivo);
}
 
/* ============================================================================
 * PROGRAMA PRINCIPAL (INTERFACE DO USUÁRIO)
 * ============================================================================ */
int main() {
    NoLista* listaJogadores = NULL;
    NoTrie* raizTrie = criarNoTrie();
    BloomFilter* filtroBloom = criarBloomFilter(); 
    NoHash* tabelaHash[HASH_SIZE];
    for (int i = 0; i < HASH_SIZE; i++) tabelaHash[i] = NULL;
 
    EstatisticasGlobais statsGlobais = {0, 0, 0, 0};
    InfoLiga ligas[5];
    char* nomes_ligas[] = { "English Premier League", "Spain Primera Division", "Italian Serie A", "German 1. Bundesliga", "French Ligue 1" };
    for (int i = 0; i < 5; i++) { strcpy(ligas[i].nome_liga, nomes_ligas[i]); }
    Simulacao sim = {0, 0, 0, 0, 0};
 
    printf("A ler a Base de Dados. Aguarde...\n");
    carregarDataset(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas, &sim);
 
    int opcao = 0; char prefixo[100]; char nomeCompleto[100];
    while (opcao != 9) {
        printf("\n=========================================\n");
        printf("    SISTEMA DE GERENCIAMENTO FIFA (CBF)  \n");
        printf("=========================================\n");
        printf("1. Inserir Jogador (Manual)\n");
        printf("2. Buscar Jogador (Filtro Ativo)\n");
        printf("3. Remover Jogador (Delecao no Bloom)\n");
        printf("4. Exibir Dados das Ligas e Gerais\n");
        printf("5. Agrupar e Exibir TOP 11 por Liga\n");
        printf("6. Filtrar Jogadores (Anti-Ruido)\n");
        printf("7. Executar Benchmarks Completos\n");
        printf("8. Painel de Simulacao de Falhas\n");
        printf("9. Sair\n");
        printf("Escolha uma opcao: ");
        if (scanf("%d", &opcao) != 1) { while(getchar() != '\n'); continue; }
        getchar(); 
 
        switch (opcao) {
            case 1: {
                Jogador novo = {"", "Manual", "N/A", 22, 0, 0};
                printf("Nome: "); fgets(novo.nome, sizeof(novo.nome), stdin); novo.nome[strcspn(novo.nome, "\n")] = 0;
                printf("Overall: "); scanf("%d", &novo.overall); getchar();
                inserirLista(&listaJogadores, novo); inserirHash(tabelaHash, novo);
                inserirTrie(raizTrie, novo.nome); inserirBloom(filtroBloom, novo.nome); 
                statsGlobais.contador++; statsGlobais.soma_overall += novo.overall;
                printf("[SUCESSO] Inserido globalmente!\n");
                break;
            }
            case 2: {
                printf("\nNome/Prefixo: "); fgets(prefixo, sizeof(prefixo), stdin); prefixo[strcspn(prefixo, "\n")] = 0;
                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);
                    if (!sim.r21_algoritmo && verificarBloom(filtroBloom, nomeCompleto) == 0) {
                        printf("[COUNTING BLOOM] DETERMINADO COMO: INEXISTENTE (0%% Falso Negativo).\n");
                    } else {
                        Jogador* achado = buscarHash(tabelaHash, nomeCompleto, &sim);
                        if (achado) printf("[HASH] Encontrado! Clube: %s | Overall: %d\n", achado->time, achado->overall);
                    }
                } else printf("[TRIE] Prefixo nao reconhecido.\n");
                break;
            }
            case 3: {
                printf("\nNome para remocao: "); fgets(prefixo, sizeof(prefixo), stdin); prefixo[strcspn(prefixo, "\n")] = 0;
                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    Jogador* alvo = buscarHash(tabelaHash, nomeCompleto, &sim);
                    if (alvo != NULL) {
                        Jogador copia = *alvo; 
                        removerDaLista(&listaJogadores, nomeCompleto);
                        removerHash(tabelaHash, nomeCompleto);
                        desmarcarTrie(raizTrie, nomeCompleto);
                        
                        /* EXECUÇÃO DA OPERAÇÃO DE OTIMIZAÇÃO: 
                         * O Counting Bloom decrementa os contadores de forma estável */
                        removerBloom(filtroBloom, nomeCompleto);
 
                        statsGlobais.contador--;
                        statsGlobais.soma_overall -= copia.overall;
                        int idx_liga = verificarLiga(copia.liga, ligas);
                        if (idx_liga != -1) ligas[idx_liga].contador--;
                        printf("[SUCESSO] \"%s\" removido de TODAS as estruturas (Contadores decrementados no Bloom).\n", nomeCompleto);
                    }
                }
                break;
            }
            case 4: exibirEstatisticas(&statsGlobais, ligas); break; 
            case 5: exibirTop11PorLiga(listaJogadores, ligas); break;
            case 6: filtrarJogadores(listaJogadores); break;
            case 7: executarBenchmarks(listaJogadores, tabelaHash, raizTrie, filtroBloom, &sim, statsGlobais.contador); break;
            case 8: {
                int opSim = 0;
                while (opSim != 7) {
                    printf("\n=== PAINEL DE SIMULACAO ===\n");
                    printf("1. [R2] Limitar RAM (5k): %s\n", sim.r2_memoria ? "ON" : "OFF");
                    printf("2. [R9] Forcar Carga CPU: %s\n", sim.r9_cpu ? "ON" : "OFF");
                    printf("3. [R12] Latencia Rede:   %s\n", sim.r12_latencia ? "ON" : "OFF");
                    printf("4. [R16] Corromper Dados: %s\n", sim.r16_dados ? "ON" : "OFF");
                    printf("5. [R21] Degradar Algor.: %s\n", sim.r21_algoritmo ? "ON" : "OFF");
                    printf("6. RECARREGAR DATASET CSV\n");
                    printf("7. Voltar\n");
                    printf("Escolha: ");
                    if (scanf("%d", &opSim) != 1) { while(getchar() != '\n'); continue; }
                    if (opSim == 1) sim.r2_memoria = !sim.r2_memoria;
                    else if (opSim == 2) sim.r9_cpu = !sim.r9_cpu;
                    else if (opSim == 3) sim.r12_latencia = !sim.r12_latencia;
                    else if (opSim == 4) { sim.r16_dados = 1; corromperDados(listaJogadores); }
                    else if (opSim == 5) sim.r21_algoritmo = !sim.r21_algoritmo;
                    else if (opSim == 6) {
                        limparEstruturas(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas);
                        carregarDataset(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas, &sim);
                    }
                }
                break;
            }
            case 9: break;
        }
    }
    limparEstruturas(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas);
    free(raizTrie); free(filtroBloom);
    return 0;
}