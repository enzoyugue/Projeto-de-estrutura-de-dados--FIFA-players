#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 

#define HASH_SIZE 20011 
#define BLOOM_SIZE 200000 
// ATUALIZADO: O tamanho agora é exato, 1 contador (short) por posição
#define BLOOM_ARRAY_SIZE BLOOM_SIZE 

typedef struct {
    char nome[100];
    char time[100];
    char liga[50]; 
    int idade;
    int overall;
    int salario;
} Jogador;

// --- ESTRUTURA PARA CONTROLO DA SIMULAÇÃO (As 5 Restrições) ---
typedef struct {
    int r2_memoria;     // Limita RAM a 5000 registos
    int r9_cpu;         // Estrangula CPU na Hash
    int r12_latencia;   // Atrasa resposta em 1ms
    int r16_dados;      // Corrompe dados (Overall = 999)
    int r21_algoritmo;  // Desliga Hash e Bloom (Força O(N))
} Simulacao;

// --- ESTRUTURAS DE DADOS DO PROJETO ---
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

// ATUALIZADO: Estrutura otimizada para Counting Bloom Filter
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

// --- DECLARAÇÕES ANTECIPADAS PARA ORGANIZAÇÃO ---
void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas, Simulacao* sim);
void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas);

int removerDaLista(NoLista** cabeca, const char* nome);
void exibirEstatisticas(EstatisticasGlobais* stats, InfoLiga* ligas);
long contarNosTrie(NoTrie* no);
void analisarColisoesHash(NoHash** tabela);
void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie);
void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, int n);
void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel);

// --- FUNÇÕES DA PILHA ---
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

// --- FUNÇÕES DA LISTA ENCADEADA ---
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

// --- FUNÇÕES DA TABELA HASH ---
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
    // [SIMULAÇÃO R9] - Estrangulamento de CPU (Operações inúteis forçadas)
    if (sim != NULL && sim->r9_cpu) {
        volatile double dummy = 0.0;
        for (int i = 0; i < 50000; i++) dummy += 1.1; 
    }
    
    // [SIMULAÇÃO R12] - Latência de Rede/Disco (Atraso de 1 milissegundo)
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

// --- FUNÇÕES DA ÁRVORE TRIE ---
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
    atual->fimDePalavra = 0;
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

// --- FUNÇÕES DO BLOOM FILTER OTIMIZADO (COUNTING BLOOM FILTER) ---

BloomFilter* criarBloomFilter() {
    BloomFilter* bf = (BloomFilter*)malloc(sizeof(BloomFilter));
    // ATUALIZADO: Inicializa todos os contadores a zero
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

    // ATUALIZADO: Em vez de ligar o bit, soma-se +1 ao contador
    bf->contadores[h1]++;
    bf->contadores[h2]++;
    bf->contadores[h3]++;
}

int verificarBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);

    // ATUALIZADO: Se qualquer contador for zero, a chave NÃO existe
    if (bf->contadores[h1] == 0) return 0;
    if (bf->contadores[h2] == 0) return 0;
    if (bf->contadores[h3] == 0) return 0;

    return 1; 
}

// NOVO: Função para remover itens do Counting Bloom Filter
void removerBloom(BloomFilter* bf, const char* chave) {
    unsigned int h1 = funcaoHash(chave) % BLOOM_SIZE;
    unsigned int h2 = hash_sdbm(chave);
    unsigned int h3 = hash_custom(chave);

    // Subtrai -1 de forma segura
    if (bf->contadores[h1] > 0) bf->contadores[h1]--;
    if (bf->contadores[h2] > 0) bf->contadores[h2]--;
    if (bf->contadores[h3] > 0) bf->contadores[h3]--;
}

// --- FUNÇÕES DE MANIPULAÇÃO E SIMULAÇÃO ---

// [SIMULAÇÃO R16] Corrompe 10% dos dados gerando Overalls bizarros
void corromperDados(NoLista* cabeca) {
    srand(time(NULL));
    int afetados = 0;
    NoLista* atual = cabeca;
    while (atual != NULL) {
        if (rand() % 10 == 0) { // 10% de chance de corromper
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
        // O Filtro bloqueia Overalls 999 (Provando resiliência à Simulação R16)
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
            if (strcmp(atual->jogador.liga, nomeDaLiga) == 0 && atual->jogador.overall <= 99) { // Proteção R16
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

    if (stats->contador == 0) {
        printf("Nenhum jogador carregado.\n");
        return;
    }

    printf("\n>> GERAL (%d jogadores no sistema)\n", stats->contador);
    printf(" - Overall medio: %.2f\n", (double)stats->soma_overall / stats->contador);
    printf(" - Idade media:   %.2f\n", (double)stats->soma_idade / stats->contador);
    printf(" - Salario medio: EUR %.2f\n", (double)stats->soma_salario / stats->contador);

    printf("\n>> POR LIGA (Top 5 Europeias)\n");
    printf("%-28s | %-9s | %-7s | %-6s | %-14s\n", "Liga", "Jogador.", "Overall", "Idade", "Salario Medio");
    printf("-----------------------------------------------------------------------\n");
    for (int i = 0; i < 5; i++) {
        if (ligas[i].contador == 0) {
            printf("%-28s | sem dados\n", ligas[i].nome_liga);
            continue;
        }
        double overallMedio = (double)ligas[i].soma_overall / ligas[i].contador;
        double idadeMedia = (double)ligas[i].soma_idade / ligas[i].contador;
        double salarioMedio = (double)ligas[i].soma_salario / ligas[i].contador;
        printf("%-28s | %-9d | %-7.2f | %-6.2f | EUR %-10.2f\n",
               ligas[i].nome_liga, ligas[i].contador, overallMedio, idadeMedia, salarioMedio);
    }
    printf("======================================================\n");
}

// --- FUNÇÕES AUXILIARES DE BENCHMARK EXPANDIDO ---

void benchmarkEscalabilidade(NoLista* lista, int totalDisponivel) {
    int tamanhosBase[] = {1000, 2500, 5000, 10000};
    int tamanhos[5];
    int numTamanhos = 0;

    for (int i = 0; i < 4; i++) {
        if (tamanhosBase[i] < totalDisponivel) tamanhos[numTamanhos++] = tamanhosBase[i];
    }
    tamanhos[numTamanhos++] = totalDisponivel; // garante que o tamanho real entra na tabela

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

void benchmarkInsercaoRemocao(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, int n) {
    clock_t inicio, fim;

    Jogador* sinteticos = (Jogador*)malloc(n * sizeof(Jogador));
    if (sinteticos == NULL) {
        printf("\n[ERRO] Sem memoria para o benchmark de insercao/remocao.\n");
        return;
    }

    for (int i = 0; i < n; i++) {
        sprintf(sinteticos[i].nome, "BenchPlayer_%d", i);
        strcpy(sinteticos[i].time, "BenchTeam");
        strcpy(sinteticos[i].liga, "BenchLeague");
        sinteticos[i].idade = 20;
        sinteticos[i].overall = 70;
        sinteticos[i].salario = 10000;
    }

    // INSERCAO (Lista + Hash + Trie + Bloom)
    inicio = clock();
    for (int i = 0; i < n; i++) {
        inserirLista(lista, sinteticos[i]);
        inserirHash(tabela, sinteticos[i]);
        inserirTrie(trie, sinteticos[i].nome);
        inserirBloom(bf, sinteticos[i].nome);
    }
    fim = clock();
    double tempoInsercao = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    // REMOCAO (Lista + Hash + Trie + Bloom)
    inicio = clock();
    for (int i = 0; i < n; i++) {
        removerDaLista(lista, sinteticos[i].nome);
        removerHash(tabela, sinteticos[i].nome);
        desmarcarTrie(trie, sinteticos[i].nome);
        // ATUALIZADO: Agora podemos remover também do Bloom Filter
        removerBloom(bf, sinteticos[i].nome);
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

void analisarColisoesHash(NoHash** tabela) {
    int bucketsOcupados = 0;
    int totalElementos = 0;
    int maiorCadeia = 0;

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

    int colisoesTotais = totalElementos - bucketsOcupados;
    double fatorCarga = (double)totalElementos / HASH_SIZE;
    double cadeiaMedia = bucketsOcupados > 0 ? (double)totalElementos / bucketsOcupados : 0.0;

    printf("\n>> 5. TAXA DE COLISAO DA TABELA HASH (%d posicoes)\n", HASH_SIZE);
    printf(" - Elementos inseridos:        %d\n", totalElementos);
    printf(" - Posicoes (buckets) usadas:  %d\n", bucketsOcupados);
    printf(" - Fator de carga (load factor): %.4f\n", fatorCarga);
    printf(" - Colisoes totais (elementos extras em buckets ja ocupados): %d\n", colisoesTotais);
    printf(" - Maior cadeia (pior caso de busca): %d elementos\n", maiorCadeia);
    printf(" - Tamanho medio de cadeia (so buckets ocupados): %.2f\n", cadeiaMedia);
}

void calcularUsoMemoria(int totalJogadores, NoTrie* raizTrie) {
    long memLista = (long)totalJogadores * (long)sizeof(NoLista);
    long memHashNos = (long)totalJogadores * (long)sizeof(NoHash);
    long memHashTabela = (long)HASH_SIZE * (long)sizeof(NoHash*);
    long nosTrie = contarNosTrie(raizTrie);
    long memTrie = nosTrie * (long)sizeof(NoTrie);
    // ATUALIZADO: Conta memória utilizando sizeof(short) agora
    long memBloom = (long)BLOOM_ARRAY_SIZE * (long)sizeof(short);
    long total = memLista + memHashNos + memHashTabela + memTrie + memBloom;

    printf("\n>> 6. USO DE MEMORIA ESTIMADO (%d jogadores carregados)\n", totalJogadores);
    printf("%-24s | %14s | %10s\n", "Estrutura", "Bytes", "KB");
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "Lista Encadeada", memLista, memLista / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (nos)", memHashNos, memHashNos / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Tabela Hash (vetor)", memHashTabela, memHashTabela / 1024.0);
    printf("%-24s | %14ld | %10.1f\n", "Trie", memTrie, memTrie / 1024.0);
    printf("  (Trie possui %ld nos alocados; cada no = %zu bytes devido aos 128 ponteiros)\n",
           nosTrie, sizeof(NoTrie));
    printf("%-24s | %14ld | %10.1f\n", "Counting Bloom Filter", memBloom, memBloom / 1024.0);
    printf("----------------------------------------------------------\n");
    printf("%-24s | %14ld | %10.1f\n", "TOTAL ESTIMADO", total, total / 1024.0);
}

// --- MÓDULO DE BENCHMARKS COM SIMULAÇÃO ---
void executarBenchmarks(NoLista* lista, NoHash** tabela, NoTrie* raizTrie, BloomFilter* bf, Simulacao* sim, int totalJogadores) {
    clock_t inicio, fim;
    double tempo_lista, tempo_hash, tempo_bloom_falso, tempo_hash_falso;
    
    // Se a latência R12 estiver ativa, fazemos menos iterações para o programa não ficar congelado horas
    int iteracoes = sim->r12_latencia ? 100 : 10000;
    
    char* nomeBusca = "L. Messi"; 
    char* nomeFalso = "Jogador Inexistente 999";

    printf("\n======================================================\n");
    printf("        BENCHMARKS DE DESEMPENHO (%d Iteracoes)       \n", iteracoes);
    if (sim->r9_cpu) printf("        [!] AVISO: Estrangulamento de CPU ATIVO\n");
    if (sim->r12_latencia) printf("        [!] AVISO: Latencia de Servidor ATIVA\n");
    if (sim->r21_algoritmo) printf("        [!] AVISO: Hash Desativada (Algoritmo Lento Forcado)\n");
    printf("======================================================\n");

    // TESTE 1: Busca na Lista Encadeada (O(N))
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

    // TESTE 2: Busca na Tabela Hash (O(1) ou O(N) se R21 estiver ativa)
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        if (sim->r21_algoritmo) {
            // Se R21 estiver ativa, ignora a Hash e força a Lista
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

    // TESTE: Rejeição de Falsos (Hash vs Bloom)
    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        buscarHash(tabela, nomeFalso, sim);
    }
    fim = clock();
    tempo_hash_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    inicio = clock();
    for (int i = 0; i < iteracoes; i++) {
        verificarBloom(bf, nomeFalso);
    }
    fim = clock();
    tempo_bloom_falso = ((double)(fim - inicio)) / CLOCKS_PER_SEC * 1000.0;

    // RESULTADOS
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
    benchmarkInsercaoRemocao(&lista, tabela, raizTrie, bf, sim->r12_latencia ? 200 : 2000);
    analisarColisoesHash(tabela);
    calcularUsoMemoria(totalJogadores, raizTrie);

    printf("======================================================\n");
}

// --- PARSER E GESTÃO DO DATASET ---
int verificarLiga(char* nome, InfoLiga* ligas) {
    for (int i = 0; i < 5; i++) {
        if (strcmp(nome, ligas[i].nome_liga) == 0) return i;
    }
    return -1;
}

void limparEstruturas(NoLista** lista, NoHash** tabela, NoTrie* trie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas) {
    liberarLista(lista);
    liberarHash(tabela);
    for(int i=0; i<128; i++) if(trie->filhos[i]) { liberarTrie(trie->filhos[i]); trie->filhos[i] = NULL; }
    // ATUALIZADO: Reset aos contadores (memória multiplicada pelo tamanho de short)
    memset(bf->contadores, 0, BLOOM_ARRAY_SIZE * sizeof(short));
    
    stats->contador = 0; stats->soma_idade = 0; stats->soma_overall = 0; stats->soma_salario = 0;
    for (int i = 0; i < 5; i++) { ligas[i].contador = 0; ligas[i].soma_overall = 0; ligas[i].soma_idade = 0; ligas[i].soma_salario = 0; }
}

void carregarDataset(NoLista** cabecaLista, NoHash** tabela, NoTrie* raizTrie, BloomFilter* bf, EstatisticasGlobais* stats, InfoLiga* ligas, Simulacao* sim) {
    FILE* arquivo = fopen("players_22.csv", "r");
    if (!arquivo) {
        printf("\n[ERRO] O ficheiro 'players_22.csv' nao foi encontrado!\n");
        return;
    }

    char linha[10000];
    fgets(linha, sizeof(linha), arquivo); 

    while (fgets(linha, sizeof(linha), arquivo)) {
        // [SIMULAÇÃO R2] Limitação de Memória a 5000 Registos
        if (sim->r2_memoria && stats->contador >= 5000) break;

        int col = 0, j = 0, dentro_aspas = 0, i;
        char buffer[256];
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
            inserirLista(cabecaLista, jog);
            inserirHash(tabela, jog);
            inserirTrie(raizTrie, jog.nome);
            inserirBloom(bf, jog.nome); 

            stats->soma_overall += jog.overall;
            stats->soma_idade += jog.idade;
            stats->soma_salario += jog.salario;
            stats->contador++;

            int idx_liga = verificarLiga(jog.liga, ligas);
            if (idx_liga != -1) {
                ligas[idx_liga].soma_overall += jog.overall;
                ligas[idx_liga].soma_idade += jog.idade;
                ligas[idx_liga].soma_salario += jog.salario;
                ligas[idx_liga].contador++;
            }
        }
    }
    fclose(arquivo);
    printf("\n[SUCESSO] %d jogadores foram carregados!\n", stats->contador);
}

// --- LOOP PRINCIPAL DO MENU ---
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
    
    Simulacao sim = {0, 0, 0, 0, 0}; // Começa com restrições desligadas

    printf("A ler a Base de Dados. Aguarde...\n");
    carregarDataset(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas, &sim);

    int opcao = 0;
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
                Jogador novo = {"", "Manual", "N/A", 22, 0, 0};
                printf("Nome: "); fgets(novo.nome, sizeof(novo.nome), stdin); novo.nome[strcspn(novo.nome, "\n")] = 0;
                printf("Overall: "); scanf("%d", &novo.overall); getchar();
                inserirLista(&listaJogadores, novo); inserirHash(tabelaHash, novo);
                inserirTrie(raizTrie, novo.nome); inserirBloom(filtroBloom, novo.nome); 

                statsGlobais.contador++;
                statsGlobais.soma_overall += novo.overall;
                statsGlobais.soma_idade += novo.idade;
                statsGlobais.soma_salario += novo.salario;

                printf("[SUCESSO] Inserido!\n");
                break;
            }
            case 2: {
                printf("\n--- BUSCA RAPIDA ---\n");
                printf("Nome/Prefixo: ");
                fgets(prefixo, sizeof(prefixo), stdin); prefixo[strcspn(prefixo, "\n")] = 0;

                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);
                    
                    if (!sim.r21_algoritmo && verificarBloom(filtroBloom, nomeCompleto) == 0) {
                        printf("[BLOOM FILTER] DEFINITIVAMENTE NAO EXISTE.\n");
                    } else {
                        Jogador* achado = NULL;
                        if (sim.r21_algoritmo) {
                            printf("[AVISO R21] A usar pesquisa linear...\n");
                            NoLista* atual = listaJogadores;
                            while(atual) { if(strcmp(atual->jogador.nome, nomeCompleto)==0) { achado = &(atual->jogador); break; } atual = atual->prox; }
                        } else {
                            achado = buscarHash(tabelaHash, nomeCompleto, &sim);
                        }
                        
                        if (achado) printf("Nome: %s | Clube: %s | Overall: %d\n", achado->nome, achado->time, achado->overall);
                    }
                } else printf("[TRIE] Nao encontrado.\n");
                break;
            }
            case 3: {
                printf("\n--- REMOCAO DE JOGADOR ---\n");
                printf("Nome/Prefixo: ");
                fgets(prefixo, sizeof(prefixo), stdin); prefixo[strcspn(prefixo, "\n")] = 0;

                if (autocompletarTrie(raizTrie, prefixo, nomeCompleto)) {
                    printf("[TRIE] Autocompletado para: \"%s\"\n", nomeCompleto);

                    Jogador* alvo = buscarHash(tabelaHash, nomeCompleto, &sim);
                    if (alvo != NULL) {
                        Jogador copia = *alvo; 

                        int okLista = removerDaLista(&listaJogadores, nomeCompleto);
                        int okHash = removerHash(tabelaHash, nomeCompleto);
                        desmarcarTrie(raizTrie, nomeCompleto);
                        
                        // ATUALIZADO: Agora suporta remoção (Counting Bloom Filter)
                        removerBloom(filtroBloom, nomeCompleto); 

                        if (okLista && okHash) {
                            statsGlobais.contador--;
                            statsGlobais.soma_overall -= copia.overall;
                            statsGlobais.soma_idade -= copia.idade;
                            statsGlobais.soma_salario -= copia.salario;

                            int idx_liga = verificarLiga(copia.liga, ligas);
                            if (idx_liga != -1) {
                                ligas[idx_liga].contador--;
                                ligas[idx_liga].soma_overall -= copia.overall;
                                ligas[idx_liga].soma_idade -= copia.idade;
                                ligas[idx_liga].soma_salario -= copia.salario;
                            }
                            printf("[SUCESSO] \"%s\" removido da Lista, Hash, Trie e Bloom Filter.\n", nomeCompleto);
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
            case 5: exibirTop11PorLiga(listaJogadores, ligas); break;
            case 6: filtrarJogadores(listaJogadores); break;
            case 7: executarBenchmarks(listaJogadores, tabelaHash, raizTrie, filtroBloom, &sim, statsGlobais.contador); break;
            
            // --- PAINEL DE SIMULAÇÃO ---
            case 8: {
                int opSim = 0;
                while (opSim != 7) {
                    printf("\n=== PAINEL DE SIMULACAO (CONDICOES ADVERSAS) ===\n");
                    printf("1. [R2] Limitar Memoria RAM a 5000 Registos:  %s\n", sim.r2_memoria ? "ON" : "OFF");
                    printf("2. [R9] Estrangular CPU (Hash Demorada):      %s\n", sim.r9_cpu ? "ON" : "OFF");
                    printf("3. [R12] Latencia de Rede (Servidor Lento):   %s\n", sim.r12_latencia ? "ON" : "OFF");
                    printf("4. [R16] Injetar Virus (Corromper Dados):     %s\n", sim.r16_dados ? "ON" : "OFF");
                    printf("5. [R21] Degradar Algoritmo (Desligar Hash):  %s\n", sim.r21_algoritmo ? "ON" : "OFF");
                    printf("6. RECARREGAR BASE DE DADOS (Aplica a R2)\n");
                    printf("7. Voltar ao Menu Principal\n");
                    printf("Escolha o que ligar/desligar: ");
                    
                    if (scanf("%d", &opSim) != 1) { while(getchar() != '\n'); continue; }
                    
                    if (opSim == 1) sim.r2_memoria = !sim.r2_memoria;
                    else if (opSim == 2) sim.r9_cpu = !sim.r9_cpu;
                    else if (opSim == 3) sim.r12_latencia = !sim.r12_latencia;
                    else if (opSim == 4) { sim.r16_dados = 1; corromperDados(listaJogadores); }
                    else if (opSim == 5) sim.r21_algoritmo = !sim.r21_algoritmo;
                    else if (opSim == 6) {
                        printf("\nA recarregar do zero...\n");
                        limparEstruturas(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas);
                        carregarDataset(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas, &sim);
                    }
                }
                break;
            }
            case 9: printf("\nA encerrar o simulador...\n"); break;
            default: printf("[!] Opcao invalida.\n");
        }
    }

    limparEstruturas(&listaJogadores, tabelaHash, raizTrie, filtroBloom, &statsGlobais, ligas);
    free(raizTrie); free(filtroBloom);
    return 0;
}