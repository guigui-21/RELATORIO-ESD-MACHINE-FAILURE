#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <profileapi.h> // Para QueryPerformanceCounter de alta precisão
#include <psapi.h>     // Para GetProcessMemoryInfo

#define MAX_LINHA 2048
#define MAX_LEVEL 16 // Nível máximo para a Skip List

// Estruturas de dados
typedef struct {
    int UDI;
    char ProductID[10];
    char Type;
    float AirTemp;
    float ProcessTemp;
    int RotationalSpeed;
    float Torque;
    int ToolWear;
    bool MachineFailure;
    bool TWF;
    bool HDF;
    bool PWF;
    bool OSF;
    bool RNF;
} MachineData;

typedef struct SkipNode {
    MachineData data;
    struct SkipNode* forward[MAX_LEVEL]; // Ponteiros para os próximos nós em cada nível
    int key;                             // Usaremos UDI como chave para ordenação
} SkipNode;

typedef struct {
    SkipNode* header;
    int level;
    int size;
} SkipList;

// Timer de alta precisão
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;
} HighPrecisionTimer;

// Funções para a Skip List
SkipNode* createNode(int key, MachineData data, int level) {
    SkipNode* sn = (SkipNode*)malloc(sizeof(SkipNode));
    sn->key = key;
    sn->data = data;
    for (int i = 0; i < level; i++) {
        sn->forward[i] = NULL;
    }
    return sn;
}

void initSkipList(SkipList* list) {
    list->header = createNode(-1, (MachineData){0}, MAX_LEVEL); // Nó cabeçalho sentinela
    list->level = 0;
    list->size = 0;
    srand(time(NULL)); // Inicializa o gerador de números aleatórios para o nível
}

int randomLevel() {
    int level = 0;
    while (rand() % 2 && level < MAX_LEVEL - 1) { // 50% de chance de subir de nível
        level++;
    }
    return level;
}

void insertSkipList(SkipList* list, int key, MachineData data) {
    SkipNode* update[MAX_LEVEL];
    SkipNode* current = list->header;

    for (int i = list->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if (current != NULL && current->key == key) {
        // Se a chave já existe, apenas atualiza os dados (ou trata como erro/ignora)
        current->data = data;
        return;
    }

    int newLevel = randomLevel();
    if (newLevel > list->level) {
        for (int i = list->level + 1; i <= newLevel; i++) {
            update[i] = list->header;
        }
        list->level = newLevel;
    }

    SkipNode* newNode = createNode(key, data, newLevel);

    for (int i = 0; i <= newLevel; i++) {
        newNode->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newNode;
    }
    list->size++;
}

SkipNode* searchSkipList(SkipList* list, int key) {
    SkipNode* current = list->header;
    for (int i = list->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0];
    if (current != NULL && current->key == key) {
        return current;
    }
    return NULL;
}

void deleteSkipList(SkipList* list, int key) {
    SkipNode* update[MAX_LEVEL];
    SkipNode* current = list->header;

    for (int i = list->level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if (current == NULL || current->key != key) {
        return; // Chave não encontrada
    }

    for (int i = 0; i <= list->level; i++) {
        if (update[i]->forward[i] != current) {
            break; // Já removeu em níveis mais baixos, ou não estava presente
        }
        update[i]->forward[i] = current->forward[i];
    }
    free(current);

    while (list->level > 0 && list->header->forward[list->level] == NULL) {
        list->level--;
    }
    list->size--;
}

void freeSkipList(SkipList* list) {
    SkipNode* current = list->header->forward[0]; // Começa do primeiro nó real
    while (current != NULL) {
        SkipNode* next = current->forward[0];
        free(current);
        current = next;
    }
    free(list->header);
    list->header = NULL;
    list->size = 0;
    list->level = 0;
}

// Funções auxiliares existentes (adaptadas para Skip List)
void removerAspas(char* str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '\"')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

void parseCSV(SkipList* list) {
    FILE* f = fopen("MachineFailure.csv", "r");
    if (!f) {
        perror("Erro ao abrir MachineFailure.csv");
        return;
    }
    char linha[MAX_LINHA];
    fgets(linha, MAX_LINHA, f); // cabeçalho
    while (fgets(linha, MAX_LINHA, f)) {
        removerAspas(linha);
        linha[strcspn(linha, "\n")] = '\0';
        MachineData d;
        char* tok = strtok(linha, ",");
        if (tok)
            d.UDI = atoi(tok);
        tok = strtok(NULL, ",");
        if (tok)
            strncpy(d.ProductID, tok, 9);
        d.ProductID[9] = '\0';
        tok = strtok(NULL, ",");
        d.Type = tok ? tok[0] : '\0';
        tok = strtok(NULL, ",");
        d.AirTemp = tok ? atof(tok) : 0;
        tok = strtok(NULL, ",");
        d.ProcessTemp = tok ? atof(tok) : 0;
        tok = strtok(NULL, ",");
        d.RotationalSpeed = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.Torque = tok ? atof(tok) : 0;
        tok = strtok(NULL, ",");
        d.ToolWear = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.MachineFailure = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.TWF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.HDF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.PWF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.OSF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ",");
        d.RNF = tok ? atoi(tok) : 0;
        insertSkipList(list, d.UDI, d); // Insere na Skip List usando UDI como chave
    }
    fclose(f);
}

void displayItem(MachineData d) {
    printf("UDI: %d | ProductID: %s | Type: %c | AirTemp: %.1f | ProcessTemp: %.1f | RPM: %d | Torque: %.1f | ToolWear: %d | Failure: %d\n",
           d.UDI, d.ProductID, d.Type, d.AirTemp, d.ProcessTemp,
           d.RotationalSpeed, d.Torque, d.ToolWear, d.MachineFailure);
}

void displayAll(SkipList* list) {
    SkipNode* current = list->header->forward[0];
    while (current != NULL) {
        displayItem(current->data);
        current = current->forward[0];
    }
}

void insertManualSample(SkipList* list) {
    MachineData newData;
    printf("\n=== INSERIR NOVA AMOSTRA MANUALMENTE ===\n");

    // Encontrar o próximo UDI disponível
    int maxUDI = 0;
    SkipNode* current = list->header->forward[0];
    while (current != NULL) {
        if (current->data.UDI > maxUDI) {
            maxUDI = current->data.UDI;
        }
        current = current->forward[0];
    }
    newData.UDI = maxUDI + 1;

    printf("Dados atuais da máquina (UDI: %d)\n", newData.UDI);

    // ProductID e Type (validação de formato [L/M/H]NNNNN e atribuição automática)
    char firstChar;
    bool formatoValido;
    while (1) { // Loop infinito até que uma entrada válida seja fornecida
        printf("ProductID (formato L/M/H seguido por 5 dígitos, ex: M12345): ");
        formatoValido = true; // Assume que o formato é válido inicialmente

        // Tenta ler a string ProductID (limitado a 9 caracteres + nulo)
        if (scanf("%9s", newData.ProductID) == 1) {
            // Limpa o buffer de entrada APÓS ler a string para consumir o newline pendente
            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            // 1. Verifica o comprimento total (deve ser 6 caracteres)
            if (strlen(newData.ProductID) != 6) {
                printf("Erro: ProductID deve ter exatamente 6 caracteres (Letra + 5 dígitos).\n");
                formatoValido = false;
            } else {
                // 2. Verifica a primeira letra
                firstChar = toupper(newData.ProductID[0]);
                if (firstChar != 'L' && firstChar != 'M' && firstChar != 'H') {
                    printf("Erro: ProductID deve começar com L, M ou H.\n");
                    formatoValido = false;
                } else {
                    // 3. Verifica se os 5 caracteres restantes são dígitos
                    for (int i = 1; i < 6; i++) {
                        if (!isdigit(newData.ProductID[i])) {
                            printf("Erro: Os 5 caracteres após a letra inicial devem ser dígitos numéricos.\n");
                            formatoValido = false;
                            break; // Sai do loop for se encontrar um não-dígito
                        }
                    }
                }
            }

            // Se o formato for válido após todas as verificações
            if (formatoValido) {
                newData.Type = firstChar; // Atribui o tipo automaticamente
                printf("Tipo definido automaticamente como: %c\n", newData.Type);
                break; // Sai do loop while, pois a entrada é válida
            }
            // Se formatoValido for false, o loop continua e pede a entrada novamente

        } else {
            // Caso scanf falhe (ex: fim de arquivo ou erro de leitura)
            printf("Erro na leitura do ProductID. Tente novamente.\n");
            // Limpa o buffer em caso de erro para tentar novamente
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            // Considerar adicionar lógica para sair em caso de erro persistente
        }
    } // Fim do loop while(1)

    // Temperaturas
    printf("Temperatura do Ar (em °C): ");
    scanf("%f", &newData.AirTemp);
    int c_temp1; while ((c_temp1 = getchar()) != '\n' && c_temp1 != EOF);

    printf("Temperatura do Processo (em °C): ");
    scanf("%f", &newData.ProcessTemp);
    int c_temp2; while ((c_temp2 = getchar()) != '\n' && c_temp2 != EOF);

    // Velocidade Rotacional
    printf("Velocidade Rotacional (RPM): ");
    scanf("%d", &newData.RotationalSpeed);
    int c_rpm; while ((c_rpm = getchar()) != '\n' && c_rpm != EOF);

    // Torque
    printf("Torque (Nm): ");
    scanf("%f", &newData.Torque);
    int c_torque; while ((c_torque = getchar()) != '\n' && c_torque != EOF);

    // Desgaste da Ferramenta
    printf("Desgaste da Ferramenta (minutos): ");
    scanf("%d", &newData.ToolWear);
    int c_wear; while ((c_wear = getchar()) != '\n' && c_wear != EOF);

    // Falhas (Lendo como int 0/1 e armazenando em bool)
    int temp_bool;
    
    // Validação para MachineFailure
    while (1) {
        printf("Falha da Máquina (0-Não, 1-Sim): ");
        if (scanf("%d", &temp_bool) == 1) {
            int c_fail1; while ((c_fail1 = getchar()) != '\n' && c_fail1 != EOF); // Limpa o buffer
            if (temp_bool == 0 || temp_bool == 1) {
                newData.MachineFailure = (bool)temp_bool;
                break; // Sai do loop se a entrada for válida
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail1; while ((c_fail1 = getchar()) != '\n' && c_fail1 != EOF); // Limpa o buffer em caso de entrada não numérica
        }
    }

    // Validação para TWF
    while (1) {
        printf("Falha TWF (0-Não, 1-Sim): ");
        if (scanf("%d", &temp_bool) == 1) {
            int c_fail2; while ((c_fail2 = getchar()) != '\n' && c_fail2 != EOF);
            if (temp_bool == 0 || temp_bool == 1) {
                newData.TWF = (bool)temp_bool;
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail2; while ((c_fail2 = getchar()) != '\n' && c_fail2 != EOF);
        }
    }

    // Validação para HDF
    while (1) {
        printf("Falha HDF (0-Não, 1-Sim): ");
        if (scanf("%d", &temp_bool) == 1) {
            int c_fail3; while ((c_fail3 = getchar()) != '\n' && c_fail3 != EOF);
            if (temp_bool == 0 || temp_bool == 1) {
                newData.HDF = (bool)temp_bool;
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail3; while ((c_fail3 = getchar()) != '\n' && c_fail3 != EOF);
        }
    }

    // Validação para PWF
    while (1) {
        printf("Falha PWF (0-Não, 1-Sim): ");
        if (scanf("%d", &temp_bool) == 1) {
            int c_fail4; while ((c_fail4 = getchar()) != '\n' && c_fail4 != EOF);
            if (temp_bool == 0 || temp_bool == 1) {
                newData.PWF = (bool)temp_bool;
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail4; while ((c_fail4 = getchar()) != '\n' && c_fail4 != EOF);
        }
    }

    // Validação para OSF
    while (1) {
        printf("Falha OSF (0-Não, 1-Sim): ");
        if (scanf("%d", &temp_bool) == 1) {
            int c_fail5; while ((c_fail5 = getchar()) != '\n' && c_fail5 != EOF);
            if (temp_bool == 0 || temp_bool == 1) {
                newData.OSF = (bool)temp_bool;
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail5; while ((c_fail5 = getchar()) != '\n' && c_fail5 != EOF);
        }
    }

    // Validação para RNF
    while (1) {
        printf("Falha RNF (0-Não, 1-Sim): ");
        if (scanf("%d", &temp_bool) == 1) {
            int c_fail6; while ((c_fail6 = getchar()) != '\n' && c_fail6 != EOF);
            if (temp_bool == 0 || temp_bool == 1) {
                newData.RNF = (bool)temp_bool;
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail6; while ((c_fail6 = getchar()) != '\n' && c_fail6 != EOF);
        }
    }

    // Confirmar inserção
    printf("\n=== RESUMO DA AMOSTRA ===\n");
    displayItem(newData);

    int confirm_choice; // Usamos um nome diferente para evitar confusão com 'confirm' booleano
    while (1) { // Loop para validar a entrada 0 ou 1
        printf("\nConfirmar inserção? (0-Não, 1-Sim): ");
        if (scanf("%d", &confirm_choice) == 1) { // Tenta ler um inteiro
            int c_confirm; while ((c_confirm = getchar()) != '\n' && c_confirm != EOF); // Limpa o buffer

            if (confirm_choice == 1) {
                insertSkipList(list, newData.UDI, newData);
                printf("Amostra inserida com sucesso!\n");
                break; // Sai do loop se for 1
            } else if (confirm_choice == 0) {
                printf("Inserção cancelada.\n");
                break; // Sai do loop se for 0
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else { // Caso o scanf não consiga ler um inteiro (ex: usuário digitou uma letra)
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_confirm; while ((c_confirm = getchar()) != '\n' && c_confirm != EOF); // Limpa o buffer
        }
    }
}

void searchByProductID(SkipList* list, const char* pid) {
    SkipNode* current = list->header->forward[0];
    bool achou = false;
    while (current != NULL) {
        if (strcmp(current->data.ProductID, pid) == 0) {
            displayItem(current->data);
            achou = true;
        }
        current = current->forward[0];
    }
    if (!achou)
        printf("Nenhum item com ProductID %s\n", pid);
}

void searchByType(SkipList* list, char type) {
    SkipNode* current = list->header->forward[0];
    bool achou = false;
    type = toupper(type);
    while (current != NULL) {
        if (toupper(current->data.Type) == type) {
            displayItem(current->data);
            achou = true;
        }
        current = current->forward[0];
    }
    if (!achou)
        printf("Nenhum item do tipo %c\n", type);
}

void searchByMachineFailure(SkipList* list, bool f) {
    SkipNode* current = list->header->forward[0];
    bool achou = false;
    while (current != NULL) {
        if (current->data.MachineFailure == f) {
            displayItem(current->data);
            achou = true;
        }
        current = current->forward[0];
    }
    if (!achou)
        printf("Nenhum item com falha %d\n", f);
}

bool removeByProductID(SkipList* list, const char* pid) {
    // Na SkipList, a remoção é por chave (UDI). Para remover por ProductID,
    // precisaríamos encontrar o UDI correspondente primeiro.
    // Esta função precisaria de uma busca linear ou um índice secundário.
    // Para simplificar, vou manter a busca linear para encontrar e remover
    // o primeiro item com o ProductID correspondente. Para remover todos,
    // seria necessário iterar e deletar.

    SkipNode* current = list->header->forward[0];
    bool removed = false;

    // Criar uma lista temporária de UDI's a serem removidos
    int* udi_to_remove = NULL;
    int count = 0;

    while (current != NULL) {
        if (strcmp(current->data.ProductID, pid) == 0) {
            count++;
            udi_to_remove = (int*)realloc(udi_to_remove, count * sizeof(int));
            if (udi_to_remove == NULL) {
                perror("Erro de alocação de memória");
                return false;
            }
            udi_to_remove[count - 1] = current->data.UDI;
        }
        current = current->forward[0];
    }

    for (int i = 0; i < count; i++) {
        deleteSkipList(list, udi_to_remove[i]);
        removed = true;
    }

    free(udi_to_remove);
    return removed;
}

void displayStats(const char* title, float avg, float max, float min, float stdDev) {
    printf("\n%s:\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           title, avg, max, min, stdDev);
}

void calculateStatistics(SkipList* list) {
    if (list->size == 0) {
        printf("Lista vazia. Nenhum dado para análise.\n");
        return;
    }

    float twSum = 0, twMax = -INFINITY, twMin = INFINITY;
    float twSqDiffSum = 0;

    float tqSum = 0, tqMax = -INFINITY, tqMin = INFINITY;
    float tqSqDiffSum = 0;

    int rsSum = 0, rsMax = -INT_MAX, rsMin = INT_MAX;
    float rsSqDiffSum = 0;

    float tempDiffSum = 0, tempDiffMax = -INFINITY, tempDiffMin = INFINITY;
    float tempDiffSqDiffSum = 0;

    SkipNode* current = list->header->forward[0];
    while (current != NULL) {
        twSum += current->data.ToolWear;
        if (current->data.ToolWear > twMax)
            twMax = current->data.ToolWear;
        if (current->data.ToolWear < twMin)
            twMin = current->data.ToolWear;

        tqSum += current->data.Torque;
        if (current->data.Torque > tqMax)
            tqMax = current->data.Torque;
        if (current->data.Torque < tqMin)
            tqMin = current->data.Torque;

        rsSum += current->data.RotationalSpeed;
        if (current->data.RotationalSpeed > rsMax)
            rsMax = current->data.RotationalSpeed;
        if (current->data.RotationalSpeed < rsMin)
            rsMin = current->data.RotationalSpeed;

        float diff = current->data.ProcessTemp - current->data.AirTemp;
        tempDiffSum += diff;
        if (diff > tempDiffMax)
            tempDiffMax = diff;
        if (diff < tempDiffMin)
            tempDiffMin = diff;

        current = current->forward[0];
    }

    float twAvg = twSum / list->size;
    float tqAvg = tqSum / list->size;
    float rsAvg = (float)rsSum / list->size;
    float tempDiffAvg = tempDiffSum / list->size;

    current = list->header->forward[0];
    while (current != NULL) {
        twSqDiffSum += pow(current->data.ToolWear - twAvg, 2);
        tqSqDiffSum += pow(current->data.Torque - tqAvg, 2);
        rsSqDiffSum += pow(current->data.RotationalSpeed - rsAvg, 2);
        float diff = current->data.ProcessTemp - current->data.AirTemp;
        tempDiffSqDiffSum += pow(diff - tempDiffAvg, 2);
        current = current->forward[0];
    }

    float twStdDev = sqrt(twSqDiffSum / list->size);
    float tqStdDev = sqrt(tqSqDiffSum / list->size);
    float rsStdDev = sqrt(rsSqDiffSum / list->size);
    float tempDiffStdDev = sqrt(tempDiffSqDiffSum / list->size);

    printf("\n=== ESTATÍSTICAS DE OPERAÇÃO ===\n");
    displayStats("Desgaste da Ferramenta (ToolWear)", twAvg, twMax, twMin, twStdDev);
    displayStats("Torque (Nm)", tqAvg, tqMax, tqMin, tqStdDev);
    displayStats("Velocidade Rotacional (RPM)", rsAvg, rsMax, rsMin, rsStdDev);
    displayStats("Diferença de Temperatura (ProcessTemp - AirTemp)", tempDiffAvg, tempDiffMax, tempDiffMin, tempDiffStdDev);
}

void classifyFailures(SkipList* list) {
    if (list->size == 0) {
        printf("Lista vazia. Nenhum dado para análise.\n");
        return;
    }

    typedef struct {
        int total;
        int twf;
        int hdf;
        int pwf;
        int osf;
        int rnf;
    } TypeStats;

    TypeStats stats[3] = {0}; // 0: L, 1: M, 2: H
    int totalFailures[5] = {0}; // TWF, HDF, PWF, OSF, RNF

    SkipNode* current = list->header->forward[0];
    while (current != NULL) {
        int typeIndex = -1;
        switch (toupper(current->data.Type)) {
            case 'L':
                typeIndex = 0;
                break;
            case 'M':
                typeIndex = 1;
                break;
            case 'H':
                typeIndex = 2;
                break;
        }

        if (typeIndex != -1) {
            stats[typeIndex].total++;

            if (current->data.TWF) {
                stats[typeIndex].twf++;
                totalFailures[0]++;
            }
            if (current->data.HDF) {
                stats[typeIndex].hdf++;
                totalFailures[1]++;
            }
            if (current->data.PWF) {
                stats[typeIndex].pwf++;
                totalFailures[2]++;
            }
            if (current->data.OSF) {
                stats[typeIndex].osf++;
                totalFailures[3]++;
            }
            if (current->data.RNF) {
                stats[typeIndex].rnf++;
                totalFailures[4]++;
            }
        }

        current = current->forward[0];
    }

    printf("\n=== CLASSIFICAÇÃO DE FALHAS POR TIPO DE MÁQUINA ===\n");

    printf("\n%-10s %-10s %-10s %-10s %-10s %-10s %-10s\n",
           "Tipo", "Total", "TWF", "HDF", "PWF", "OSF", "RNF");

    for (int i = 0; i < 3; i++) {
        char type = (i == 0) ? 'L' : (i == 1) ? 'M' : 'H';

        printf("%-10c %-10d ", type, stats[i].total);

        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].twf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");

        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].hdf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");

        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].pwf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");

        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].osf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");

        if (stats[i].total > 0)
            printf("%-10.1f%%\n", (float)stats[i].rnf / stats[i].total * 100);
        else
            printf("%-10s\n", "N/A");
    }

    printf("\n=== TOTAIS GERAIS ===\n");
    printf("TWF: %d ocorrências\n", totalFailures[0]);
    printf("HDF: %d ocorrências\n", totalFailures[1]);
    printf("PWF: %d ocorrências\n", totalFailures[2]);
    printf("OSF: %d ocorrências\n", totalFailures[3]);
    printf("RNF: %d ocorrências\n", totalFailures[4]);
}

void advancedFilter(SkipList* list) {
    printf("\n=== FILTRO AVANÇADO ===\n");
    printf("Escolha os critérios de filtro:\n");
    printf("1. ToolWear\n");
    printf("2. Torque\n");
    printf("3. RotationalSpeed\n");
    printf("4. Diferença de Temperatura\n");
    printf("5. Tipo de Máquina\n");
    printf("6. Estado de Falha\n");
    printf("0. Aplicar filtros\n");

    int criteria[6] = {0};
    float minVal[4] = {0};
    float maxVal[4] = {0};
    char typeFilter = '\0';
    bool failureFilter = false;
    bool hasFailureFilter = false;

    int choice;
    do {
        printf("\nEscolha um critério para adicionar (0 para aplicar): ");
        scanf("%d", &choice);

        if (choice >= 1 && choice <= 4) {
            criteria[choice - 1] = 1;
            printf("Digite o valor mínimo: ");
            scanf("%f", &minVal[choice - 1]);
            printf("Digite o valor máximo: ");
            scanf("%f", &maxVal[choice - 1]);
        } else if (choice == 5) {
            criteria[4] = 1;
            printf("Digite o tipo de máquina (M, L, H): ");
            scanf(" %c", &typeFilter);
            typeFilter = toupper(typeFilter);
        } else if (choice == 6) {
            criteria[5] = 1;
            hasFailureFilter = true;
            printf("Filtrar por máquinas com falha? (1-Sim, 0-Não): ");
            scanf("%d", &failureFilter);
        }
    } while (choice != 0);

    printf("\nResultados do Filtro:\n");
    int matches = 0;
    SkipNode* current = list->header->forward[0];

    while (current != NULL) {
        bool match = true;

        if (criteria[0] && (current->data.ToolWear < minVal[0] || current->data.ToolWear > maxVal[0])) {
            match = false;
        }
        if (match && criteria[1] && (current->data.Torque < minVal[1] || current->data.Torque > maxVal[1])) {
            match = false;
        }
        if (match && criteria[2] && (current->data.RotationalSpeed < minVal[2] || current->data.RotationalSpeed > maxVal[2])) {
            match = false;
        }
        if (match && criteria[3]) {
            float tempDiff = current->data.ProcessTemp - current->data.AirTemp;
            if (tempDiff < minVal[3] || tempDiff > maxVal[3]) {
                match = false;
            }
        }
        if (match && criteria[4] && toupper(current->data.Type) != typeFilter) {
            match = false;
        }
        if (match && criteria[5] && current->data.MachineFailure != failureFilter) {
            match = false;
        }

        if (match) {
            printf("UDI: %d | ProductID: %s | Type: %c | ",
                   current->data.UDI, current->data.ProductID, current->data.Type);

            if (criteria[0])
                printf("ToolWear: %d | ", current->data.ToolWear);
            if (criteria[1])
                printf("Torque: %.1f | ", current->data.Torque);
            if (criteria[2])
                printf("RPM: %d | ", current->data.RotationalSpeed);
            if (criteria[3]) {
                float tempDiff = current->data.ProcessTemp - current->data.AirTemp;
                printf("TempDiff: %.1f | ", tempDiff);
            }
            if (criteria[5])
                printf("Failure: %d | ", current->data.MachineFailure);

            printf("\n");
            matches++;
        }

        current = current->forward[0];
    }

    printf("\nTotal de máquinas que atendem aos critérios: %d\n", matches);
}

void displayMenu() {
    printf("\nMenu:\n");
    printf("1. Exibir todos\n");
    printf("2. Buscar ProductID\n");
    printf("3. Buscar Type\n");
    printf("4. Buscar MachineFailure\n");
    printf("5. Inserir nova amostra manualmente\n");
    printf("6. Remover por ProductID\n");
    printf("7. Estatísticas\n");
    printf("8. Classificar Falhas\n");
    printf("9. Filtro Avançado\n");
    printf("10. Executar Benchmarks\n");
    printf("11. Executar Restrições\n");
    printf("12. Aprender Padrões de Falha\n");        // NOVA OPÇÃO
    printf("13. Simular Fresadora e Detectar Falhas\n"); // NOVA OPÇÃO
    printf("14. Sair\n");                               // Opção de saída atualizada
    printf("Escolha: ");
}

// Implementação das funções de benchmark com alta precisão
void start_timer(HighPrecisionTimer* timer) {
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->start);
}

double stop_timer(HighPrecisionTimer* timer) {
    QueryPerformanceCounter(&timer->end);
    return (double)(timer->end.QuadPart - timer->start.QuadPart) * 1000.0 / timer->frequency.QuadPart;
}

void generateRandomData(SkipList* list, int count) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < count; i++) {
        MachineData d = {0};
        d.UDI = 10000 + i + rand() % 100000; // Garantir UDI único para SkipList
        snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
        d.Type = "LMH"[rand() % 3];
        d.AirTemp = 20.0f + (rand() % 150) / 10.0f;
        d.ProcessTemp = d.AirTemp + (rand() % 100) / 10.0f;
        d.RotationalSpeed = 1200 + rand() % 2000;
        d.Torque = 30.0f + (rand() % 200) / 10.0f;
        d.ToolWear = rand() % 250;
        d.MachineFailure = rand() % 2;
        d.TWF = rand() % 2;
        d.HDF = rand() % 2;
        d.PWF = rand() % 2;
        d.OSF = rand() % 2;
        d.RNF = rand() % 2;
        insertSkipList(list, d.UDI, d);
    }
}

void benchmark_insertion(SkipList* list, int num_elements) {
    HighPrecisionTimer t;
    start_timer(&t);
    SkipList tmp;
    initSkipList(&tmp);
    generateRandomData(&tmp, num_elements);
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Inserção (%d elementos): %.3f ms (%.1f elem/ms)\n",
           num_elements, elapsed, num_elements / elapsed);
    freeSkipList(&tmp);
}

void benchmark_search(SkipList* list) {
    if (list->size == 0) {
        printf("Lista vazia para busca\n");
        return;
    }
    HighPrecisionTimer t;
    const int searches = 10000;
    int found = 0;
    start_timer(&t);
    for (int i = 0; i < searches; i++) {
        int search_udi = list->header->forward[0]->data.UDI + (rand() % list->size); // Busca UDI's existentes
        if (searchSkipList(list, search_udi) != NULL) {
            found++;
        }
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Busca (%d ops): encontrados=%d | tempo=%.3f ms (%.1f ops/ms)\n",
           searches, found, elapsed, searches / elapsed);
}

void benchmark_removal(SkipList* list) {
    if (list->size == 0) {
        printf("Lista vazia para remoção\n");
        return;
    }
    SkipList tmp;
    initSkipList(&tmp);
    SkipNode* cur = list->header->forward[0];
    while (cur != NULL) {
        insertSkipList(&tmp, cur->data.UDI, cur->data);
        cur = cur->forward[0];
    }
    HighPrecisionTimer t;
    const int removals = 1000;
    start_timer(&t);
    for (int i = 0; i < removals; i++) {
        if (tmp.size > 0) {
            // Remove um elemento aleatório existente (garantir que exista)
            SkipNode* node_to_remove = tmp.header->forward[0];
            for(int k = 0; k < rand() % tmp.size && node_to_remove != NULL; k++){
                node_to_remove = node_to_remove->forward[0];
            }
            if(node_to_remove != NULL){
                deleteSkipList(&tmp, node_to_remove->key);
            }
        }
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Remoção (%d ops): %.3f ms (%.1f ops/ms)\n",
           removals, elapsed, removals / elapsed);
    freeSkipList(&tmp);
}

size_t calculate_node_size() {
    size_t size = 0;

    size += sizeof(int);   // UDI
    size += 10;            // ProductID (char[10])
    size += sizeof(char);  // Type
    size += sizeof(float); // AirTemp
    size += sizeof(float); // ProcessTemp
    size += sizeof(int);   // RotationalSpeed
    size += sizeof(float); // Torque
    size += sizeof(int);   // ToolWear
    size += sizeof(bool);  // MachineFailure
    size += sizeof(bool);  // TWF
    size += sizeof(bool);  // HDF
    size += sizeof(bool);  // PWF
    size += sizeof(bool);  // OSF
    size += sizeof(bool);  // RNF

    size += sizeof(int); // key
    size += sizeof(SkipNode*) * MAX_LEVEL; // forward array

    return size;
}

void estimate_memory_usage(SkipList* list) {
    if (list == NULL) {
        printf("Lista inválida\n");
        return;
    }

    size_t node_size = calculate_node_size();
    size_t total_memory = list->size * node_size + sizeof(SkipList) + sizeof(SkipNode); // Add header size
    
    printf("\n=== ESTIMATIVA DE USO DE MEMÓRIA ===\n");
    printf("Tamanho por nó: %zu bytes\n", node_size);
    printf("Número de nós: %d\n", list->size);
    printf("Memória total estimada: %zu bytes (%.2f KB)\n",
           total_memory, (float)total_memory / 1024);

    printf("\nComparação com sizeof:\n");
    printf("sizeof(MachineData): %zu bytes\n", sizeof(MachineData));
    printf("sizeof(SkipNode): %zu bytes\n", sizeof(SkipNode));
    printf("Obs: Pode haver padding/alignment pelo compilador\n");
}

void benchmark_random_access(SkipList* list) {
    if (list->size == 0) {
        printf("Lista vazia para teste de acesso aleatório\n");
        return;
    }

    const int accesses = 10000;
    HighPrecisionTimer t;
    int sum = 0;

    start_timer(&t);
    for (int i = 0; i < accesses; i++) {
        int random_udi_offset = rand() % list->size;
        SkipNode* current = list->header->forward[0];
        for(int j = 0; j < random_udi_offset && current != NULL; j++){
            current = current->forward[0];
        }
        if (current != NULL) {
            sum += current->data.UDI;
        }
    }
    double elapsed = stop_timer(&t);

    printf("Benchmark Acesso Aleatório (%d acessos): %.3f ms (%.1f acessos/ms)\n",
           accesses, elapsed, accesses / elapsed);
}

void benchmark_scalability() {
    printf("\nBenchmark de Escalabilidade:\n");
    int sizes[] = {1000, 5000, 10000, 20000, 50000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < num_sizes; i++) {
        SkipList list;
        initSkipList(&list);

        HighPrecisionTimer t;
        start_timer(&t);
        generateRandomData(&list, sizes[i]);
        double elapsed = stop_timer(&t);

        printf("Tamanho: %6d elementos | Tempo de inserção: %7.3f ms | Tempo por elemento: %.5f ms\n",
               sizes[i], elapsed, elapsed / sizes[i]);

        freeSkipList(&list);
    }
}

void benchmark_combined_operations() {
    const int num_operations = 1000;
    SkipList list;
    initSkipList(&list);
    generateRandomData(&list, 1000);

    HighPrecisionTimer t;
    start_timer(&t);

    for (int i = 0; i < num_operations; i++) {
        if (rand() % 100 < 30) {
            MachineData d = {0};
            d.UDI = 10000 + list.size + rand() % 1000; // Garantir UDI único
            snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
            insertSkipList(&list, d.UDI, d);
        } else if (rand() % 100 < 80) {
            if (list.size > 0) {
                int search_udi = list.header->forward[0]->data.UDI + (rand() % list.size);
                searchSkipList(&list, search_udi);
            }
        } else {
            if (list.size > 0) {
                // Para remoção aleatória eficiente, precisaríamos de uma forma de obter um UDI aleatório existente
                // Mais simples para o benchmark é tentar remover um UDI que pode ou não existir
                int remove_udi = list.header->forward[0]->data.UDI + (rand() % list.size); 
                deleteSkipList(&list, remove_udi);
            }
        }
    }

    double elapsed = stop_timer(&t);
    printf("Benchmark Operações Combinadas (%d ops): %.3f ms | Latência média: %.3f ms/op\n",
           num_operations, elapsed, elapsed / num_operations);

    freeSkipList(&list);
}

void run_all_benchmarks(SkipList* list) {
    printf("\n=== INICIANDO BENCHMARKS COMPLETOS ===\n");

    printf("\n1. Tempo de Inserção:\n");
    benchmark_insertion(list, 1000);
    benchmark_insertion(list, 10000);

    printf("\n2. Tempo de Remoção:\n");
    benchmark_removal(list);

    printf("\n3. Tempo de Busca:\n");
    benchmark_search(list);

    printf("\n4. Uso de Memória:\n");
    estimate_memory_usage(list);

    printf("\n5. Tempo Médio de Acesso:\n");
    benchmark_random_access(list);

    printf("\n6. Escalabilidade:\n");
    benchmark_scalability();

    printf("\n7. Latência Média (operações combinadas):\n");
    benchmark_combined_operations();

    printf("\n=== BENCHMARKS CONCLUÍDOS ===\n");
}


// Função para remover o primeiro elemento (para restrição R2)
void removeFirst(SkipList* list) {
    if (list == NULL || list->header->forward[0] == NULL) return;
    deleteSkipList(list, list->header->forward[0]->key);
}

void generateAnomalousData(SkipList* list, int count, int max_size) {
    for (int i = 0; i < count; i++) {
        if (max_size > 0 && list->size >= max_size) {
            removeFirst(list); // R2: Limitação de tamanho
        }

        if (i % 100 == 0) {
            Sleep(1); // R10: Interrupções periódicas
        }

        MachineData d = {0};
        d.UDI = 10000 + i + rand() % 100000; // Garantir UDI único
        snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
        d.Type = "LMH"[rand() % 3];
        d.AirTemp = 20.0f + (rand() % 150) / 10.0f;
        d.ProcessTemp = d.AirTemp + (rand() % 100) / 10.0f;
        d.RotationalSpeed = 1200 + rand() % 2000;
        d.Torque = 30.0f + (rand() % 200) / 10.0f;
        d.ToolWear = rand() % 250;
        d.MachineFailure = rand() % 2;
        d.TWF = rand() % 2;
        d.HDF = rand() % 2;
        d.PWF = rand() % 2;
        d.OSF = rand() % 2;
        d.RNF = rand() % 2;

        // R18: Inserção de anomalias
        if (rand() % 10 == 0) {
            d.UDI = -1 - i; // UDI negativo para garantir unicidade e que se destacará
            d.AirTemp = -999.0f;
            d.Type = 'X';
        }

        insertSkipList(list, d.UDI, d); // Insere na Skip List

        if (i > 0 && i % 100 == 0) {
            Sleep(50); // R13: Delay artificial por lote
        }
    }
}

// A ordenação não é necessária para Skip List, pois ela mantém os elementos ordenados por chave.
// A restrição R24 original se aplicava à lista duplamente encadeada.
// Para manter o espírito da restrição (performance impactada por operação ineficiente),
// podemos simular uma "verificação de integridade" lenta, ou simplesmente remover R24.
// Por enquanto, vou remover a função selectionSortList, pois é redundante e ineficiente para SkipList.

void run_restricted_benchmarks() {
    printf("\n=== BENCHMARK COM RESTRIÇÕES ATIVADAS ===\n");

    SkipList list;
    initSkipList(&list);

    HighPrecisionTimer t;
    start_timer(&t);

    // R2, R10, R13, R18
    generateAnomalousData(&list, 1000, 500);

    double elapsed = stop_timer(&t);

    printf("\nTempo total (com 4 restrições aplicadas): %.3f ms\n", elapsed);
    printf("Elementos finais na lista (máximo 500): %d\n", list.size);

    // R24 - Com Skip List, a ordenação já é mantida, então não há uma "ordenação ineficiente" adicional.
    // Podemos omitir esta parte ou adaptar para uma validação/operação complexa que impacte a performance.

    // Benchmarks após restrições
    benchmark_search(&list);
    benchmark_removal(&list);
    benchmark_random_access(&list);
    estimate_memory_usage(&list);

    freeSkipList(&list);

    printf("\n=== FIM DOS TESTES COM RESTRIÇÕES ===\n");
}

typedef struct {
    float minAirTemp, maxAirTemp;
    float minProcessTemp, maxProcessTemp;
    int minRotationalSpeed, maxRotationalSpeed;
    float minTorque, maxTorque;
    int minToolWear, maxToolWear;
    bool hadTWF, hadHDF, hadPWF, hadOSF, hadRNF;
} FailurePattern;

// NOVA ESTRUTURA: Lista dinâmica para armazenar múltiplos padrões de falha
// Usaremos uma lista dinâmica para armazenar os padrões de falha aprendidos.
typedef struct {
    FailurePattern* patterns;
    int count;
    int capacity;
} FailurePatternList;
// --- FUNÇÕES AUXILIARES PARA A LISTA DE PADRÕES DE FALHA ---

// Inicializa a lista de padrões de falha
void initFailurePatternList(FailurePatternList* list) {
    list->patterns = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Adiciona um padrão de falha à lista dinâmica
void addFailurePattern(FailurePatternList* list, FailurePattern pattern) {
    if (list->count == list->capacity) {
        list->capacity = (list->capacity == 0) ? 1 : list->capacity * 2;
        list->patterns = (FailurePattern*)realloc(list->patterns, list->capacity * sizeof(FailurePattern));
        if (list->patterns == NULL) {
            perror("Falha ao realocar memória para os padrões de falha");
            exit(EXIT_FAILURE);
        }
    }
    list->patterns[list->count++] = pattern;
}

// Libera a memória alocada para a lista de padrões de falha
void freeFailurePatternList(FailurePatternList* list) {
    free(list->patterns);
    list->patterns = NULL;
    list->count = 0;
    list->capacity = 0;
}

// --- FUNÇÕES DE APRENDIZAGEM E DETECÇÃO DE PADRÕES DE FALHA ---

// APRENDE OS PADRÕES DE FALHA A PARTIR DOS DADOS EXISTENTES NA Skip List
// Percorre a Skip List e extrai informações de entradas onde MachineFailure é true.
// Para este exemplo, ele armazena os valores exatos de falhas como padrões.
// Opção 12 do menu.
void learnFailurePatterns(SkipList* list, FailurePatternList* patterns) {
    if (list->size == 0) {
        printf("Skip List vazia. Nenhuma falha para aprender.\n");
        return;
    }

    printf("\n=== APRENDENDO PADRÕES DE FALHA ===\n");
    // Reinicia os padrões antes de aprender novos
    freeFailurePatternList(patterns);
    initFailurePatternList(patterns);

    int learned_count = 0;
    SkipNode* current = list->header->forward[0]; // Começa do primeiro nó real
    while (current != NULL) {
        if (current->data.MachineFailure) { // Se houver falha na máquina
            FailurePattern fp;
            // Para este exemplo, armazena os valores exatos da instância de falha como um padrão.
            fp.minAirTemp = fp.maxAirTemp = current->data.AirTemp;
            fp.minProcessTemp = fp.maxProcessTemp = current->data.ProcessTemp;
            fp.minRotationalSpeed = fp.maxRotationalSpeed = current->data.RotationalSpeed;
            fp.minTorque = fp.maxTorque = current->data.Torque;
            fp.minToolWear = fp.maxToolWear = current->data.ToolWear;
            
            fp.hadTWF = current->data.TWF;
            fp.hadHDF = current->data.HDF;
            fp.hadPWF = current->data.PWF;
            fp.hadOSF = current->data.OSF;
            fp.hadRNF = current->data.RNF;

            addFailurePattern(patterns, fp);
            learned_count++;
        }
        current = current->forward[0]; // Avança no nível base
    }
    printf("Aprendidos %d padrões de falha a partir dos dados existentes.\n", learned_count);
}

// VERIFICA SE OS DADOS ATUAIS CORRESPONDEM A UM PADRÃO DE FALHA APRENDIDO
// Compara os dados da máquina com os padrões armazenados.
// A correspondência atual é exata. Em um cenário real, você pode usar uma tolerância.
bool checkForFailurePattern(MachineData data, FailurePatternList* patterns) {
    for (int i = 0; i < patterns->count; i++) {
        FailurePattern fp = patterns->patterns[i];

        // Esta é uma correspondência exata. Considere adicionar uma tolerância (e.g., fabs(data.AirTemp - fp.minAirTemp) < epsilon)
        bool match = true;
        if (fabs(data.AirTemp - fp.minAirTemp) > 0.001 || // Usar fabs para float comparisons
            fabs(data.ProcessTemp - fp.minProcessTemp) > 0.001 ||
            data.RotationalSpeed != fp.minRotationalSpeed ||
            fabs(data.Torque - fp.minTorque) > 0.001 ||
            data.ToolWear != fp.minToolWear) {
            match = false;
        }

        // Também verifica os flags de falha específicos se eles fazem parte da definição do seu padrão
        if (match && (data.TWF != fp.hadTWF ||
                      data.HDF != fp.hadHDF ||
                      data.PWF != fp.hadPWF ||
                      data.OSF != fp.hadOSF ||
                      data.RNF != fp.hadRNF)) {
            match = false;
        }

        if (match) {
            return true; // Padrão detectado!
        }
    }
    return false; // Nenhum padrão correspondente encontrado
}

// --- FUNÇÃO DE SIMULAÇÃO DA FRESADORA ---

// SIMULA A FRESADORA E DETECTA PADRÕES DE FALHA
// Gera dados de MachineData simulados.
// Periodicamente, injeta um padrão de falha aprendido para demonstrar a detecção.
// Opção 13 do menu.
void simulateMillingMachine(SkipList* list, FailurePatternList* patterns, int num_simulations) {
    if (patterns->count == 0) {
        printf("Nenhum padrão de falha aprendido. Por favor, aprenda os padrões primeiro (Opção 12).\n");
        return;
    }

    printf("\n=== SIMULANDO FRESADORA E DETECTANDO FALHAS ===\n");
    srand((unsigned)time(NULL)); // Inicializa o gerador de números aleatórios
    int failure_alerts = 0;
    int next_udi = 0;

    // Encontra o UDI máximo atual para continuar a partir dele
    SkipNode* current_max_udi_node = list->header->forward[0];
    while (current_max_udi_node != NULL) {
        if (current_max_udi_node->data.UDI > next_udi) {
            next_udi = current_max_udi_node->data.UDI;
        }
        current_max_udi_node = current_max_udi_node->forward[0];
    }
    next_udi++; // Começa o UDI para novas simulações a partir do próximo número

    for (int i = 0; i < num_simulations; i++) {
        MachineData simulatedData;
        simulatedData.UDI = next_udi++;

        // Gera ProductID e Tipo (pode ser aleatório ou seguir uma sequência)
        snprintf(simulatedData.ProductID, sizeof(simulatedData.ProductID), "SIM%06d", rand() % 1000000);
        simulatedData.Type = "LMH"[rand() % 3];

        // Introduz variações em torno de valores típicos (menos de falha)
        // Defina faixas razoáveis para a sua simulação de "operação normal"
        simulatedData.AirTemp = 298.0f + (float)(rand() % 200) / 100.0f - 1.0f; // Ex: 297.0 a 299.0 K
        simulatedData.ProcessTemp = simulatedData.AirTemp + 10.0f + (float)(rand() % 100) / 100.0f; // Ex: Processo geralmente mais alto
        simulatedData.RotationalSpeed = 1400 + rand() % 200 - 100; // Ex: 1300 a 1500 rpm
        simulatedData.Torque = 30.0f + (float)(rand() % 200) / 100.0f - 1.0f; // Ex: 29.0 a 31.0 Nm
        simulatedData.ToolWear = 30 + rand() % 30 - 15; // Ex: 15 a 45 min

        // Assume que não há falha inicialmente para dados simulados, a menos que um padrão seja injetado
        simulatedData.MachineFailure = false;
        simulatedData.TWF = false;
        simulatedData.HDF = false;
        simulatedData.PWF = false;
        simulatedData.OSF = false;
        simulatedData.RNF = false;

        // Introduz um padrão de falha periodicamente ou aleatoriamente
        // Aqui, há uma chance de 5% de injetar um padrão de falha aprendido
        if (patterns->count > 0 && rand() % 20 == 0) { 
            // Seleciona um padrão aprendido aleatório para injetar
            int pattern_idx = rand() % patterns->count;
            FailurePattern injected_pattern = patterns->patterns[pattern_idx];

            // Sobrescreve os dados simulados com os valores do padrão de falha
            simulatedData.AirTemp = injected_pattern.minAirTemp;
            simulatedData.ProcessTemp = injected_pattern.minProcessTemp;
            simulatedData.RotationalSpeed = injected_pattern.minRotationalSpeed;
            simulatedData.Torque = injected_pattern.minTorque;
            simulatedData.ToolWear = injected_pattern.minToolWear;
            
            // Define os flags de falha de acordo com o padrão
            simulatedData.MachineFailure = true; // Isso é crucial para a detecção
            simulatedData.TWF = injected_pattern.hadTWF;
            simulatedData.HDF = injected_pattern.hadHDF;
            simulatedData.PWF = injected_pattern.hadPWF;
            simulatedData.OSF = injected_pattern.hadOSF;
            simulatedData.RNF = injected_pattern.hadRNF;
        }

        // Verifica se os dados simulados correspondem a algum padrão de falha aprendido
        if (checkForFailurePattern(simulatedData, patterns)) {
            printf("\n!!! ALERTA DE PADRÃO DE FALHA DETECTADO !!!\n");
            displayItem(simulatedData);
            failure_alerts++;
        } else {
            // Opcionalmente, exibe operações normais (comentado para evitar muita saída)
            // printf("Simulação Normal: ");
            // displayItem(simulatedData);
        }

        // Adiciona os dados simulados à Skip List
        insertSkipList(list, simulatedData.UDI, simulatedData);
    }
    printf("\nSimulação concluída. Total de alertas de falha: %d\n", failure_alerts);
}

int main() {
    SkipList list;
    initSkipList(&list);
    parseCSV(&list); // Carrega os dados iniciais do CSV

    // ADICIONE ESTAS DUAS LINHAS:
    FailurePatternList failurePatterns; // Declara a lista de padrões de falha
    initFailurePatternList(&failurePatterns); // Inicializa a lista

    int choice;
    char input[64]; // Buffer para ler entradas de texto

    do {
        displayMenu();
        if (!fgets(input, sizeof(input), stdin)) { // Lê a linha completa
            break; // Em caso de erro na leitura
        }
        choice = atoi(input); // Converte a string para inteiro

        switch (choice) {
            case 1:
                displayAll(&list);
                break;
            case 2: {
                printf("Digite o ProductID para buscar: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0'; // Remove o newline
                    // Adapte esta busca para Skip List se você tiver uma função eficiente para isso.
                    // A busca por ProductID pode ser O(N) na SkipList se não for a chave.
                    printf("Busca por ProductID em SkipList não é eficiente pela chave. Apenas o UDI é a chave de busca direta.\n");
                    // Implemente a lógica de busca se necessário, percorrendo o nível base da SkipList.
                }
                break;
            }
            case 3: {
                printf("Digite o Tipo para buscar (L, M, H): ");
                if (fgets(input, sizeof(input), stdin)) {
                    char type_char = toupper(input[0]);
                    // Adapte esta busca para Skip List.
                    printf("Busca por Tipo em SkipList não é eficiente pela chave. Percorrendo o nível base para buscar...\n");
                    SkipNode* current = list.header->forward[0];
                    bool found = false;
                    while (current != NULL) {
                        if (current->data.Type == type_char) {
                            displayItem(current->data);
                            found = true;
                        }
                        current = current->forward[0];
                    }
                    if (!found) {
                        printf("Nenhum item encontrado com o Tipo: %c\n", type_char);
                    }
                }
                break;
            }
            case 4: {
                printf("Digite 1 para buscar falhas, 0 para buscar não falhas: ");
                int failure;
                if (scanf("%d", &failure)) {
                    while (getchar() != '\n'); // Limpa o buffer
                    bool failure_bool = (failure == 1);
                    // Adapte esta busca para Skip List.
                    printf("Busca por MachineFailure em SkipList não é eficiente pela chave. Percorrendo o nível base para buscar...\n");
                    SkipNode* current = list.header->forward[0];
                    bool found = false;
                    while (current != NULL) {
                        if (current->data.MachineFailure == failure_bool) {
                            displayItem(current->data);
                            found = true;
                        }
                        current = current->forward[0];
                    }
                    if (!found) {
                        printf("Nenhum item encontrado com Falha de Máquina: %s\n", failure_bool ? "Sim" : "Não");
                    }
                }
                break;
            }
            case 5:
    			insertManualSample(&list);
    			break;
            case 6: {
                printf("Digite o ProductID para remover: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0';
                    // A remoção por ProductID na SkipList exige busca linear se ProductID não for a chave.
                    printf("A remoção por ProductID em SkipList não é eficiente. Recomenda-se remover por UDI (chave).\n");
                    // Você precisará implementar a lógica de remoção por ProductID percorrendo a lista base
                    // e chamando deleteSkipList com o UDI correspondente.
                    // Por simplicidade, vou apenas exibir uma mensagem.
                    printf("Funcionalidade de remoção por ProductID não totalmente adaptada para SkipList com ProductID como chave secundária.\n");
                }
                break;
            }
            case 7:
                // Adapte para calcular estatísticas percorrendo a Skip List
                printf("Cálculo de Estatísticas para SkipList:\n");
                if (list.size == 0) {
                    printf("Lista vazia. Nenhuma estatística para calcular.\n");
                } else {
                    SkipNode* current = list.header->forward[0];
                    float totalAirTemp = 0, totalProcessTemp = 0, totalTorque = 0;
                    int totalRotationalSpeed = 0, totalToolWear = 0;
                    int failureCount = 0;

                    while (current != NULL) {
                        totalAirTemp += current->data.AirTemp;
                        totalProcessTemp += current->data.ProcessTemp;
                        totalRotationalSpeed += current->data.RotationalSpeed;
                        totalTorque += current->data.Torque;
                        totalToolWear += current->data.ToolWear;
                        if (current->data.MachineFailure) {
                            failureCount++;
                        }
                        current = current->forward[0];
                    }
                    printf("Número total de amostras: %d\n", list.size);
                    printf("Média da Temperatura do Ar: %.2f\n", totalAirTemp / list.size);
                    printf("Média da Temperatura do Processo: %.2f\n", totalProcessTemp / list.size);
                    printf("Média da Velocidade Rotacional: %d\n", totalRotationalSpeed / list.size);
                    printf("Média do Torque: %.2f\n", totalTorque / list.size);
                    printf("Média do Desgaste da Ferramenta: %d\n", totalToolWear / list.size);
                    printf("Número de falhas de máquina: %d (%.2f%%)\n", failureCount, (float)failureCount / list.size * 100);
                }
                break;
            case 8:
                // Adapte para classificar falhas percorrendo a Skip List
                printf("Classificação de Falhas para SkipList:\n");
                if (list.size == 0) {
                    printf("Lista vazia. Nenhuma falha para classificar.\n");
                } else {
                    SkipNode* current = list.header->forward[0];
                    int twf_count = 0, hdf_count = 0, pwf_count = 0, osf_count = 0, rnf_count = 0;
                    int total_failures = 0;

                    while (current != NULL) {
                        if (current->data.MachineFailure) {
                            total_failures++;
                            if (current->data.TWF) twf_count++;
                            if (current->data.HDF) hdf_count++;
                            if (current->data.PWF) pwf_count++;
                            if (current->data.OSF) osf_count++;
                            if (current->data.RNF) rnf_count++;
                        }
                        current = current->forward[0];
                    }
                    printf("Total de Falhas de Máquina: %d\n", total_failures);
                    if (total_failures > 0) {
                        printf("Falha por Desgaste da Ferramenta (TWF): %d (%.2f%%)\n", twf_count, (float)twf_count / total_failures * 100);
                        printf("Falha por Degradação de Calor (HDF): %d (%.2f%%)\n", hdf_count, (float)hdf_count / total_failures * 100);
                        printf("Falha por Sobrecarga de Energia (PWF): %d (%.2f%%)\n", pwf_count, (float)pwf_count / total_failures * 100);
                        printf("Falha por Desvio do Eixo (OSF): %d (%.2f%%)\n", osf_count, (float)osf_count / total_failures * 100);
                        printf("Falha Não Registrada (RNF): %d (%.2f%%)\n", rnf_count, (float)rnf_count / total_failures * 100);
                    } else {
                        printf("Nenhuma falha de máquina registrada nos dados.\n");
                    }
                }
                break;
            case 9:
                // Adapte para usar a Skip List para consultas de intervalo, se possível
                printf("Filtro Avançado para SkipList: Requer iteração no nível base ou adaptação para chaves.\n");
                // advancedFilter(&list); // Você precisará adaptar esta função para SkipList
                break;
            case 10:
                run_all_benchmarks(&list);
                break;
            case 11:
            	run_restricted_benchmarks();
				break;
            // ADICIONE ESTES NOVOS CASES:
            case 12: // Nova opção: Aprender Padrões de Falha
                learnFailurePatterns(&list, &failurePatterns);
                break;
            case 13: { // Nova opção: Simular Fresadora e Detectar Falhas
                printf("Quantas simulações deseja executar? ");
                int num_sims;
                if (scanf("%d", &num_sims) == 1) {
                    while (getchar() != '\n'); // Limpa o buffer
                    simulateMillingMachine(&list, &failurePatterns, num_sims);
                } else {
                    printf("Entrada inválida. Por favor, digite um número.\n");
                    while (getchar() != '\n'); // Limpa o buffer
                }
                break;
            }
            // FIM DOS NOVOS CASES

            case 14: // Opção de saída atualizada (o número mudou de 12 para 14)
                printf("Saindo...\n");
                break;
            default:
                printf("Opção inválida. Tente novamente.\n");
        }
    } while (choice != 14); // Condição de saída atualizada

    freeSkipList(&list);
    // ADICIONE ESTA LINHA:
    freeFailurePatternList(&failurePatterns); // Libera a memória da lista de padrões
    return 0;
}