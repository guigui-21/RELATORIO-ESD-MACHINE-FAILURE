#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <profileapi.h>  // Para QueryPerformanceCounter de alta precisão
#include <psapi.h>  // Para GetProcessMemoryInfo

#define MAX_LINHA 2048

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

typedef struct Node {
    MachineData data;
    struct Node* prev;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int size;
} DoublyLinkedList;

// Timer de alta precisão
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;
} HighPrecisionTimer;

// Implementações das funções básicas
void initList(DoublyLinkedList* list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void append(DoublyLinkedList* list, MachineData data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;
    if (!list->head) {
        newNode->prev = NULL;
        list->head = list->tail = newNode;
    } else {
        newNode->prev = list->tail;
        list->tail->next = newNode;
        list->tail = newNode;
    }
    list->size++;
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
typedef struct {
    FailurePattern* patterns;
    int count;
    int capacity;
} FailurePatternList;

void freeList(DoublyLinkedList* list) {
    Node* curr = list->head;
    while (curr) {
        Node* nxt = curr->next;
        free(curr);
        curr = nxt;
    }
    list->head = list->tail = NULL;
    list->size = 0;
}

void removerAspas(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '\"') *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

void parseCSV(DoublyLinkedList* list) {
    FILE *f = fopen("MachineFailure.csv", "r");
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
        char *tok = strtok(linha, ",");
        if (tok) d.UDI = atoi(tok);
        tok = strtok(NULL, ",");
        if (tok) strncpy(d.ProductID, tok, 9);
        d.ProductID[9] = '\0';
        tok = strtok(NULL, ",");
        d.Type = tok ? tok[0] : '\0';
        tok = strtok(NULL, ","); d.AirTemp = tok ? atof(tok) : 0;
        tok = strtok(NULL, ","); d.ProcessTemp = tok ? atof(tok) : 0;
        tok = strtok(NULL, ","); d.RotationalSpeed = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.Torque = tok ? atof(tok) : 0;
        tok = strtok(NULL, ","); d.ToolWear = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.MachineFailure = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.TWF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.HDF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.PWF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.OSF = tok ? atoi(tok) : 0;
        tok = strtok(NULL, ","); d.RNF = tok ? atoi(tok) : 0;
        append(list, d);
    }
    fclose(f);
}

void displayItem(MachineData d) {
    printf("UDI: %d | ProductID: %s | Type: %c | AirTemp: %.1f | ProcessTemp: %.1f | RPM: %d | Torque: %.1f | ToolWear: %d | Failure: %d\n",
           d.UDI, d.ProductID, d.Type, d.AirTemp, d.ProcessTemp,
           d.RotationalSpeed, d.Torque, d.ToolWear, d.MachineFailure);
}

void displayAll(DoublyLinkedList* list) {
    Node* curr = list->head;
    while (curr) {
        displayItem(curr->data);
        curr = curr->next;
    }
}

// Função para inserir novas amostras manualmente (Versão Corrigida V2 - com validação de formato)
void insertManualSample(DoublyLinkedList* list) {
    MachineData newData;
    printf("\n=== INSERIR NOVA AMOSTRA MANUALMENTE ===\n");

    // Encontrar o próximo UDI disponível (mantido da versão anterior)
    int maxUDI = 0;
    Node* current = list->head;
    while (current != NULL) {
        if (current->data.UDI > maxUDI) {
            maxUDI = current->data.UDI;
        }
        current = current->next;
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

    // --- O restante da função segue a lógica original (com limpeza de buffer após cada scanf) --- 

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
                append(list, newData);
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

void searchByProductID(DoublyLinkedList* list, const char* pid) {
    Node* curr = list->head;
    bool achou = false;
    while (curr) {
        if (strcmp(curr->data.ProductID, pid) == 0) {
            displayItem(curr->data);
            achou = true;
        }
        curr = curr->next;
    }
    if (!achou) printf("Nenhum item com ProductID %s\n", pid);
}

void searchByType(DoublyLinkedList* list, char type) {
    Node* curr = list->head;
    bool achou = false;
    type = toupper(type);
    while (curr) {
        if (toupper(curr->data.Type) == type) {
            displayItem(curr->data);
            achou = true;
        }
        curr = curr->next;
    }
    if (!achou) printf("Nenhum item do tipo %c\n", type);
}

void searchByMachineFailure(DoublyLinkedList* list, bool f) {
    Node* curr = list->head;
    bool achou = false;
    while (curr) {
        if (curr->data.MachineFailure == f) {
            displayItem(curr->data);
            achou = true;
        }
        curr = curr->next;
    }
    if (!achou) printf("Nenhum item com falha %d\n", f);
}

bool removeByProductID(DoublyLinkedList* list, const char* pid) {
    Node* curr = list->head;
    bool removed = false;
    while (curr) {
        if (strcmp(curr->data.ProductID, pid) == 0) {
            Node* toDel = curr;
            if (curr->prev) curr->prev->next = curr->next;
            else list->head = curr->next;
            if (curr->next) curr->next->prev = curr->prev;
            else list->tail = curr->prev;
            curr = curr->next;
            free(toDel);
            list->size--;
            removed = true;
        } else curr = curr->next;
    }
    return removed;
}

void displayStats(const char* title, float avg, float max, float min, float stdDev) {
    printf("\n%s:\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           title, avg, max, min, stdDev);
}

void calculateStatistics(DoublyLinkedList* list) {
    if (list->size == 0) {
        printf("Lista vazia. Nenhum dado para análise.\n");
        return;
    }

    // Variáveis para ToolWear
    float twSum = 0, twMax = -INFINITY, twMin = INFINITY;
    float twSqDiffSum = 0;
    
    // Variáveis para Torque
    float tqSum = 0, tqMax = -INFINITY, tqMin = INFINITY;
    float tqSqDiffSum = 0;
    
    // Variáveis para RotationalSpeed
    int rsSum = 0, rsMax = -INT_MAX, rsMin = INT_MAX;
    float rsSqDiffSum = 0;
    
    // Variáveis para diferença de temperatura
    float tempDiffSum = 0, tempDiffMax = -INFINITY, tempDiffMin = INFINITY;
    float tempDiffSqDiffSum = 0;

    Node* current = list->head;
    while (current != NULL) {
        // Cálculos para ToolWear
        twSum += current->data.ToolWear;
        if (current->data.ToolWear > twMax) twMax = current->data.ToolWear;
        if (current->data.ToolWear < twMin) twMin = current->data.ToolWear;
        
        // Cálculos para Torque
        tqSum += current->data.Torque;
        if (current->data.Torque > tqMax) tqMax = current->data.Torque;
        if (current->data.Torque < tqMin) tqMin = current->data.Torque;
        
        // Cálculos para RotationalSpeed
        rsSum += current->data.RotationalSpeed;
        if (current->data.RotationalSpeed > rsMax) rsMax = current->data.RotationalSpeed;
        if (current->data.RotationalSpeed < rsMin) rsMin = current->data.RotationalSpeed;
        
        // Cálculos para diferença de temperatura
        float diff = current->data.ProcessTemp - current->data.AirTemp;
        tempDiffSum += diff;
        if (diff > tempDiffMax) tempDiffMax = diff;
        if (diff < tempDiffMin) tempDiffMin = diff;

        current = current->next;
    }

    // Cálculo das médias
    float twAvg = twSum / list->size;
    float tqAvg = tqSum / list->size;
    float rsAvg = (float)rsSum / list->size;
    float tempDiffAvg = tempDiffSum / list->size;

    // Segunda passada para calcular desvios padrão
    current = list->head;
    while (current != NULL) {
        twSqDiffSum += pow(current->data.ToolWear - twAvg, 2);
        tqSqDiffSum += pow(current->data.Torque - tqAvg, 2);
        rsSqDiffSum += pow(current->data.RotationalSpeed - rsAvg, 2);
        float diff = current->data.ProcessTemp - current->data.AirTemp;
        tempDiffSqDiffSum += pow(diff - tempDiffAvg, 2);
        current = current->next;
    }

    // Cálculo dos desvios padrão
    float twStdDev = sqrt(twSqDiffSum / list->size);
    float tqStdDev = sqrt(tqSqDiffSum / list->size);
    float rsStdDev = sqrt(rsSqDiffSum / list->size);
    float tempDiffStdDev = sqrt(tempDiffSqDiffSum / list->size);

    // Exibição dos resultados
    printf("\n=== ESTATÍSTICAS DE OPERAÇÃO ===\n");
    displayStats("Desgaste da Ferramenta (ToolWear)", twAvg, twMax, twMin, twStdDev);
    displayStats("Torque (Nm)", tqAvg, tqMax, tqMin, tqStdDev);
    displayStats("Velocidade Rotacional (RPM)", rsAvg, rsMax, rsMin, rsStdDev);
    displayStats("Diferença de Temperatura (ProcessTemp - AirTemp)", tempDiffAvg, tempDiffMax, tempDiffMin, tempDiffStdDev);
}

void classifyFailures(DoublyLinkedList* list) {
    if (list->size == 0) {
        printf("Lista vazia. Nenhum dado para análise.\n");
        return;
    }

    // Estruturas para contagem de falhas por tipo de máquina
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

    Node* current = list->head;
    while (current != NULL) {
        int typeIndex = -1;
        switch (toupper(current->data.Type)) {
            case 'L': typeIndex = 0; break;
            case 'M': typeIndex = 1; break;
            case 'H': typeIndex = 2; break;
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

        current = current->next;
    }

    // Exibe resultados
    printf("\n=== CLASSIFICAÇÃO DE FALHAS POR TIPO DE MÁQUINA ===\n");
    
    // Cabeçalho
    printf("\n%-10s %-10s %-10s %-10s %-10s %-10s %-10s\n", 
           "Tipo", "Total", "TWF", "HDF", "PWF", "OSF", "RNF");
    
    // Dados para cada tipo
    for (int i = 0; i < 3; i++) {
        char type = (i == 0) ? 'L' : (i == 1) ? 'M' : 'H';
        
        printf("%-10c %-10d ", type, stats[i].total);
        
        // TWF
        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].twf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");
        
        // HDF
        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].hdf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");
        
        // PWF
        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].pwf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");
        
        // OSF
        if (stats[i].total > 0)
            printf("%-10.1f%% ", (float)stats[i].osf / stats[i].total * 100);
        else
            printf("%-10s ", "N/A");
        
        // RNF
        if (stats[i].total > 0)
            printf("%-10.1f%%\n", (float)stats[i].rnf / stats[i].total * 100);
        else
            printf("%-10s\n", "N/A");
    }

    // Totais gerais
    printf("\n=== TOTAIS GERAIS ===\n");
    printf("TWF: %d ocorrências\n", totalFailures[0]);
    printf("HDF: %d ocorrências\n", totalFailures[1]);
    printf("PWF: %d ocorrências\n", totalFailures[2]);
    printf("OSF: %d ocorrências\n", totalFailures[3]);
    printf("RNF: %d ocorrências\n", totalFailures[4]);
}

void advancedFilter(DoublyLinkedList* list) {
    printf("\n=== FILTRO AVANÇADO ===\n");
    printf("Escolha os critérios de filtro:\n");
    printf("1. ToolWear\n");
    printf("2. Torque\n");
    printf("3. RotationalSpeed\n");
    printf("4. Diferença de Temperatura\n");
    printf("5. Tipo de Máquina\n");
    printf("6. Estado de Falha\n");
    printf("0. Aplicar filtros\n");
    
    int criteria[6] = {0}; // 0-5 correspondem às opções acima
    float minVal[4] = {0}; // Para critérios 1-4 (valores mínimos)
    float maxVal[4] = {0}; // Para critérios 1-4 (valores máximos)
    char typeFilter = '\0';
    bool failureFilter = false;
    bool hasFailureFilter = false;
    
    int choice;
    do {
        printf("\nEscolha um critério para adicionar (0 para aplicar): ");
        scanf("%d", &choice);
        
        if (choice >= 1 && choice <= 4) {
            criteria[choice-1] = 1;
            printf("Digite o valor mínimo: ");
            scanf("%f", &minVal[choice-1]);
            printf("Digite o valor máximo: ");
            scanf("%f", &maxVal[choice-1]);
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
    
    // Aplicar filtros
    printf("\nResultados do Filtro:\n");
    int matches = 0;
    Node* current = list->head;
    
    while (current != NULL) {
        bool match = true;
        
        // Verificar cada critério
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
            
            if (criteria[0]) printf("ToolWear: %d | ", current->data.ToolWear);
            if (criteria[1]) printf("Torque: %.1f | ", current->data.Torque);
            if (criteria[2]) printf("RPM: %d | ", current->data.RotationalSpeed);
            if (criteria[3]) {
                float tempDiff = current->data.ProcessTemp - current->data.AirTemp;
                printf("TempDiff: %.1f | ", tempDiff);
            }
            if (criteria[5]) printf("Failure: %d | ", current->data.MachineFailure);
            
            printf("\n");
            matches++;
        }
        
        current = current->next;
    }
    
    printf("\nTotal de máquinas que atendem aos critérios: %d\n", matches);
}

void displayMenu() {
    printf("\nMenu:\n");
    printf("1. Exibir todos os itens\n");
    printf("2. Buscar por ProductID\n");
    printf("3. Buscar por Tipo\n");
    printf("4. Buscar por Falha da Máquina\n");
    printf("5. Inserir nova amostra manualmente\n");
    printf("6. Remover por ProductID\n");
    printf("7. Calcular Estatísticas\n");
    printf("8. Classificar Falhas\n");
    printf("9. Filtro Avançado\n");
    printf("10. Executar Benchmarks Completos\n");
    printf("11. Executar Benchmarks Restritos\n");
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

void generateRandomData(DoublyLinkedList* list, int count) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < count; i++) {
        MachineData d = {0};
        d.UDI = 10000 + i;
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
        append(list, d);
    }
}

void benchmark_insertion(DoublyLinkedList* list, int num_elements) {
    HighPrecisionTimer t;
    start_timer(&t);
    DoublyLinkedList tmp;
    initList(&tmp);
    generateRandomData(&tmp, num_elements);
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Inserção (%d elementos): %.3f ms (%.1f elem/ms)\n",
           num_elements, elapsed, num_elements / elapsed);
    freeList(&tmp);
}

void benchmark_search(DoublyLinkedList* list) {
    if (list->size == 0) { printf("Lista vazia para busca\n"); return; }
    HighPrecisionTimer t;
    const int searches = 10000;
    int found = 0;
    start_timer(&t);
    for (int i = 0; i < searches; i++) {
        char id[10];
        snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
        Node* cur = list->head;
        while (cur) {
            if (strcmp(cur->data.ProductID, id) == 0) { found++; break; }
            cur = cur->next;
        }
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Busca (%d ops): encontrados=%d | tempo=%.3f ms (%.1f ops/ms)\n",
           searches, found, elapsed, searches / elapsed);
}

void benchmark_removal(DoublyLinkedList* list) {
    if (list->size == 0) { printf("Lista vazia para remoção\n"); return; }
    DoublyLinkedList tmp;
    initList(&tmp);
    Node* cur = list->head;
    while (cur) { append(&tmp, cur->data); cur = cur->next; }
    HighPrecisionTimer t;
    const int removals = 1000;
    start_timer(&t);
    for (int i = 0; i < removals; i++) {
        char id[10];
        snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
        removeByProductID(&tmp, id);
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Remoção (%d ops): %.3f ms (%.1f ops/ms)\n",
           removals, elapsed, removals / elapsed);
    freeList(&tmp);
}
// Função para medir o uso de memória
// Função para calcular o tamanho de um nó da lista
size_t calculate_node_size() {
    size_t size = 0;
    
    // Tamanho da estrutura MachineData
    size += sizeof(int);    // UDI
    size += 10;             // ProductID (char[10])
    size += sizeof(char);   // Type
    size += sizeof(float);  // AirTemp
    size += sizeof(float);  // ProcessTemp
    size += sizeof(int);    // RotationalSpeed
    size += sizeof(float);  // Torque
    size += sizeof(int);    // ToolWear
    size += sizeof(bool);   // MachineFailure
    size += sizeof(bool);   // TWF
    size += sizeof(bool);   // HDF
    size += sizeof(bool);   // PWF
    size += sizeof(bool);   // OSF
    size += sizeof(bool);   // RNF
    
    // Tamanho da estrutura Node (MachineData + ponteiros)
    size += sizeof(Node*) * 2;  // prev e next
    
    return size;
}

// Função para estimar o uso total de memória
void estimate_memory_usage(DoublyLinkedList* list) {
    if (list == NULL) {
        printf("Lista inválida\n");
        return;
    }
    
    size_t node_size = calculate_node_size();
    size_t total_memory = list->size * node_size;
    
    printf("\n=== ESTIMATIVA DE USO DE MEMÓRIA ===\n");
    printf("Tamanho por nó: %zu bytes\n", node_size);
    printf("Número de nós: %d\n", list->size);
    printf("Memória total estimada: %zu bytes (%.2f KB)\n", 
           total_memory, (float)total_memory / 1024);
    
    // Adicional: comparar com o tamanho real da estrutura
    printf("\nComparação com sizeof:\n");
    printf("sizeof(MachineData): %zu bytes\n", sizeof(MachineData));
    printf("sizeof(Node): %zu bytes\n", sizeof(Node));
    printf("Obs: Pode haver padding/alignment pelo compilador\n");
}

// Benchmark de tempo médio de acesso
void benchmark_random_access(DoublyLinkedList* list) {
    if (list->size == 0) {
        printf("Lista vazia para teste de acesso aleatório\n");
        return;
    }

    const int accesses = 10000;
    HighPrecisionTimer t;
    int sum = 0; // Para evitar otimização

    start_timer(&t);
    for (int i = 0; i < accesses; i++) {
        int random_pos = rand() % list->size;
        Node* current = list->head;
        for (int j = 0; j < random_pos && current != NULL; j++) {
            current = current->next;
        }
        if (current != NULL) {
            sum += current->data.UDI; // Operação qualquer para evitar otimização
        }
    }
    double elapsed = stop_timer(&t);

    printf("Benchmark Acesso Aleatório (%d acessos): %.3f ms (%.1f acessos/ms)\n",
           accesses, elapsed, accesses / elapsed);
}

// Benchmark de escalabilidade
void benchmark_scalability() {
    printf("\nBenchmark de Escalabilidade:\n");
    int sizes[] = {1000, 5000, 10000, 20000, 50000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < num_sizes; i++) {
        DoublyLinkedList list;
        initList(&list);
        
        HighPrecisionTimer t;
        start_timer(&t);
        generateRandomData(&list, sizes[i]);
        double elapsed = stop_timer(&t);
        
        printf("Tamanho: %6d elementos | Tempo de inserção: %7.3f ms | Tempo por elemento: %.5f ms\n",
               sizes[i], elapsed, elapsed / sizes[i]);
        
        freeList(&list);
    }
}

// Benchmark de latência média (operações combinadas)
void benchmark_combined_operations() {
    const int num_operations = 1000;
    DoublyLinkedList list;
    initList(&list);
    generateRandomData(&list, 1000); // Lista inicial com 1000 elementos

    HighPrecisionTimer t;
    start_timer(&t);
    
    for (int i = 0; i < num_operations; i++) {
        // Operação de inserção (30% das vezes)
        if (rand() % 100 < 30) {
            MachineData d = {0};
            d.UDI = 10000 + i;
            snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
            append(&list, d);
        }
        // Operação de busca (50% das vezes)
        else if (rand() % 100 < 80) { // 30% inserção + 50% busca = 80%
            char id[10];
            snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
            Node* cur = list.head;
            while (cur) {
                if (strcmp(cur->data.ProductID, id) == 0) break;
                cur = cur->next;
            }
        }
        // Operação de remoção (20% das vezes)
        else {
            char id[10];
            snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
            removeByProductID(&list, id);
        }
    }
    
    double elapsed = stop_timer(&t);
    printf("Benchmark Operações Combinadas (%d ops): %.3f ms | Latência média: %.3f ms/op\n",
           num_operations, elapsed, elapsed / num_operations);
    
    freeList(&list);
}

// Substitua a função run_all_benchmarks existente por esta versão atualizada
void run_all_benchmarks(DoublyLinkedList* list) {
    printf("\n=== INICIANDO BENCHMARKS COMPLETOS ===\n");
    
    // 1. Benchmark de Inserção
    printf("\n1. Tempo de Inserção:\n");
    benchmark_insertion(list, 1000);
    benchmark_insertion(list, 10000);
    
    // 2. Benchmark de Remoção
    printf("\n2. Tempo de Remoção:\n");
    benchmark_removal(list);
    
    // 3. Benchmark de Busca
    printf("\n3. Tempo de Busca:\n");
    benchmark_search(list);
    
    // 4. Benchmark de Uso de Memória
    printf("\n4. Uso de Memória:\n");
    estimate_memory_usage(list);
    
    // 5. Benchmark de Tempo Médio de Acesso
    printf("\n5. Tempo Médio de Acesso:\n");
    benchmark_random_access(list);
    
    // 6. Benchmark de Escalabilidade
    printf("\n6. Escalabilidade:\n");
    benchmark_scalability();
    
    // 7. Benchmark de Latência Média
    printf("\n7. Latência Média (operações combinadas):\n");
    benchmark_combined_operations();
    
    printf("\n=== BENCHMARKS CONCLUÍDOS ===\n");
}

void removeFirst(DoublyLinkedList* list) {
    if (list == NULL || list->head == NULL) return;

    Node* temp = list->head;
    list->head = list->head->next;

    if (list->head != NULL) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }

    free(temp);
    list->size--;
}

void generateAnomalousData(DoublyLinkedList* list, int count, int max_size) {
    for (int i = 0; i < count; i++) {
        if (max_size > 0 && list->size >= max_size) {
            removeFirst(list); // R2: Limitação de tamanho
        }

        if (i % 100 == 0) {
            Sleep(1); // R10: Interrupções periódicas
        }

        MachineData d = {0};
        d.UDI = 10000 + i;
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
            d.UDI = -1;
            d.AirTemp = -999.0f;
            d.Type = 'X';
        }

        append(list, d);

        if (i > 0 && i % 100 == 0) {
            Sleep(50); // R13: Delay artificial por lote
        }
    }
}

void selectionSortList(DoublyLinkedList* list) {
    if (list == NULL || list->size < 2) return;

    for (Node* i = list->head; i != NULL; i = i->next) {
        Node* min = i;
        for (Node* j = i->next; j != NULL; j = j->next) {
            if (j->data.UDI < min->data.UDI) {
                min = j;
            }
        }
        if (min != i) {
            MachineData temp = i->data;
            i->data = min->data;
            min->data = temp;
        }
    }
}

void run_restricted_benchmarks() {
    printf("\n=== BENCHMARK COM RESTRIÇÕES ATIVADAS ===\n");

    DoublyLinkedList list;
    initList(&list);

    HighPrecisionTimer t;
    start_timer(&t);

    // R2, R10, R13, R18
    generateAnomalousData(&list, 1000, 500); 

    double elapsed = stop_timer(&t);

    printf("\nTempo total (com 4 restrições aplicadas): %.3f ms\n", elapsed);
    printf("Elementos finais na lista (máximo 500): %d\n", list.size);

    // R24 – Ordenação por algoritmo ineficiente
    printf("\nAplicando R24: ordenação ineficiente (selection sort)...\n");
    selectionSortList(&list); 

    // Benchmarks após restrições
    benchmark_search(&list);
    benchmark_removal(&list);
    benchmark_random_access(&list);
    estimate_memory_usage(&list);

    freeList(&list);

    printf("\n=== FIM DOS TESTES COM RESTRIÇÕES ===\n");
}

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

// APRENDE OS PADRÕES DE FALHA A PARTIR DOS DADOS EXISTENTES
// Percorre a lista de dados e extrai informações de entradas onde MachineFailure é true.
// Para este exemplo, ele armazena os valores exatos de falhas como padrões.
void learnFailurePatterns(DoublyLinkedList* list, FailurePatternList* patterns) {
    if (list->size == 0) {
        printf("Lista vazia. Nenhuma falha para aprender.\n");
        return;
    }

    printf("\n=== APRENDENDO PADRÕES DE FALHA ===\n");
    Node* current = list->head;
    int learned_count = 0;

    // Reinicia os padrões antes de aprender novos
    freeFailurePatternList(patterns);
    initFailurePatternList(patterns);

    while (current != NULL) {
        if (current->data.MachineFailure) {
            FailurePattern fp;
            // Armazena os valores exatos da instância de falha como um padrão.
            // Para um sistema mais complexo, você poderia calcular faixas, médias, etc.
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
        current = current->next;
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
        if (data.AirTemp != fp.minAirTemp ||
            data.ProcessTemp != fp.minProcessTemp ||
            data.RotationalSpeed != fp.minRotationalSpeed ||
            data.Torque != fp.minTorque ||
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

// SIMULA A FRESADORA E DETECTA PADRÕES DE FALHA
// Gera dados de MachineData simulados.
// Periodicamente, injeta um padrão de falha aprendido para demonstrar a detecção.
void simulateMillingMachine(DoublyLinkedList* list, FailurePatternList* patterns, int num_simulations) {
    if (patterns->count == 0) {
        printf("Nenhum padrão de falha aprendido. Por favor, aprenda os padrões primeiro (Opção 12).\n");
        return;
    }

    printf("\n=== SIMULANDO FRESADORA E DETECTANDO FALHAS ===\n");
    srand((unsigned)time(NULL)); // Inicializa o gerador de números aleatórios
    int failure_alerts = 0;
    int next_udi = 0;

    // Encontra o UDI máximo atual para continuar a partir dele
    Node* current_node_udi = list->head;
    while(current_node_udi != NULL) {
        if (current_node_udi->data.UDI > next_udi) {
            next_udi = current_node_udi->data.UDI;
        }
        current_node_udi = current_node_udi->next;
    }
    next_udi++; // Começa o UDI para novas simulações

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

        // Adiciona os dados simulados à lista principal (opcional, se você quiser manter os dados simulados)
        append(list, simulatedData);
    }
    printf("\nSimulação concluída. Total de alertas de falha: %d\n", failure_alerts);
}

int main() {
    DoublyLinkedList list;
    initList(&list);
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
                    searchByProductID(&list, input);
                }
                break;
            }
            case 3: {
                printf("Digite o Tipo para buscar (L, M, H): ");
                if (fgets(input, sizeof(input), stdin)) {
                    searchByType(&list, toupper(input[0]));
                }
                break;
            }
            case 4: {
                printf("Digite 1 para buscar falhas, 0 para buscar não falhas: ");
                int failure;
                if (scanf("%d", &failure)) {
                    while (getchar() != '\n'); // Limpa o buffer
                    searchByMachineFailure(&list, failure);
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
                    if (removeByProductID(&list, input))
                        printf("Item(s) removido(s) com sucesso.\n");
                    else
                        printf("Nenhum item encontrado com o ProductID: %s\n", input);
                }
                break;
            }
            case 7:
                calculateStatistics(&list);
                break;
            case 8:
                classifyFailures(&list); // Certifique-se que o nome da função está correto, anteriormente 'classifyFailhas'
                break;
            case 9:
                advancedFilter(&list);
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

    freeList(&list);
    // ADICIONE ESTA LINHA:
    freeFailurePatternList(&failurePatterns); // Libera a memória da lista de padrões
    return 0;
}