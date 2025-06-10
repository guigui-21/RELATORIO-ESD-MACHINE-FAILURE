#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <windows.h> // For QueryPerformanceCounter, GetProcessMemoryInfo, Sleep
#include <profileapi.h> // For QueryPerformanceCounter of high precision
#include <psapi.h> // For GetProcessMemoryInfo

#define MAX_LINHA 2048
#define DEFAULT_QUEUE_CAPACITY 10000 // A suitable default capacity for the circular queue

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

// Estrutura da Fila Circular Otimizada
typedef struct {
    MachineData* data; // Array to store elements
    int front;         // Index of the front element
    int rear;          // Index of the rear element
    int size;          // Current number of elements
    int capacity;      // Maximum capacity of the queue
} CircularQueue;

// Timer de alta precisão
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;
} HighPrecisionTimer;

// Implementações das funções básicas da Fila Circular
void initQueue(CircularQueue* queue, int capacity) {
    queue->capacity = capacity;
    queue->data = (MachineData*)malloc(sizeof(MachineData) * queue->capacity);
    if (queue->data == NULL) {
        perror("Erro ao alocar memória para a fila circular");
        exit(EXIT_FAILURE);
    }
    queue->front = 0;
    queue->rear = -1; // Rear will point to the last added element's index
    queue->size = 0;
}

void freeQueue(CircularQueue* queue) {
    if (queue && queue->data) {
        free(queue->data);
        queue->data = NULL;
    }
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    queue->capacity = 0;
}

bool isFull(CircularQueue* queue) {
    return queue->size == queue->capacity;
}

bool isEmpty(CircularQueue* queue) {
    return queue->size == 0;
}

// Enqueue operation for Circular Queue
void enqueue(CircularQueue* queue, MachineData data) {
    if (isFull(queue)) {
        // Overwrite the oldest element (at 'front') if the queue is full.
        // This acts as a form of "data stream buffering" or R2 restriction.
        queue->data[queue->front] = data; // Overwrite
        queue->front = (queue->front + 1) % queue->capacity; // Move front
    } else {
        queue->rear = (queue->rear + 1) % queue->capacity;
        queue->data[queue->rear] = data;
        queue->size++;
    }
}

// Dequeue operation for Circular Queue
bool dequeue(CircularQueue* queue, MachineData* data) {
    if (isEmpty(queue)) {
        return false; // Queue is empty, cannot dequeue
    }
    *data = queue->data[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    if (isEmpty(queue)) { // Reset if queue becomes empty
        queue->front = 0;
        queue->rear = -1;
    }
    return true;
}

// Peek operation
bool peek(CircularQueue* queue, MachineData* data) {
    if (isEmpty(queue)) {
        return false;
    }
    *data = queue->data[queue->front];
    return true;
}

void removerAspas(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '\"') *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

void parseCSV(CircularQueue* queue) {
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
        enqueue(queue, d);
    }
    fclose(f);
}

void displayItem(MachineData d) {
    printf("UDI: %d | ProductID: %s | Type: %c | AirTemp: %.1f | ProcessTemp: %.1f | RPM: %d | Torque: %.1f | ToolWear: %d | Failure: %d\n",
           d.UDI, d.ProductID, d.Type, d.AirTemp, d.ProcessTemp,
           d.RotationalSpeed, d.Torque, d.ToolWear, d.MachineFailure);
}

void displayAll(CircularQueue* queue) {
    if (isEmpty(queue)) {
        printf("Fila vazia.\n");
        return;
    }
    int i;
    for (i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        displayItem(queue->data[index]);
    }
}

// Função para inserir novas amostras manualmente (Versão Corrigida V2 - com validação de formato)
void insertManualSample(CircularQueue* queue) {
    MachineData newData;
    printf("\n=== INSERIR NOVA AMOSTRA MANUALMENTE ===\n");

    // Encontrar o próximo UDI disponível
    int maxUDI = 0;
    if (!isEmpty(queue)) {
        for (int i = 0; i < queue->size; i++) {
            int index = (queue->front + i) % queue->capacity;
            if (queue->data[index].UDI > maxUDI) {
                maxUDI = queue->data[index].UDI;
            }
        }
    }
    newData.UDI = maxUDI + 1;

    printf("Dados atuais da máquina (UDI: %d)\n", newData.UDI);

    // ProductID e Type (validação de formato [L/M/H]NNNNN e atribuição automática)
    char firstChar;
    bool formatoValido;
    while (1) {
        printf("ProductID (formato L/M/H seguido por 5 dígitos, ex: M12345): ");
        formatoValido = true;

        if (scanf("%9s", newData.ProductID) == 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            if (strlen(newData.ProductID) != 6) {
                printf("Erro: ProductID deve ter exatamente 6 caracteres (Letra + 5 dígitos).\n");
                formatoValido = false;
            } else {
                firstChar = toupper(newData.ProductID[0]);
                if (firstChar != 'L' && firstChar != 'M' && firstChar != 'H') {
                    printf("Erro: ProductID deve começar com L, M ou H.\n");
                    formatoValido = false;
                } else {
                    for (int i = 1; i < 6; i++) {
                        if (!isdigit(newData.ProductID[i])) {
                            printf("Erro: Os 5 caracteres após a letra inicial devem ser dígitos numéricos.\n");
                            formatoValido = false;
                            break;
                        }
                    }
                }
            }

            if (formatoValido) {
                newData.Type = firstChar;
                printf("Tipo definido automaticamente como: %c\n", newData.Type);
                break;
            }
        } else {
            printf("Erro na leitura do ProductID. Tente novamente.\n");
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
    }

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
            int c_fail1; while ((c_fail1 = getchar()) != '\n' && c_fail1 != EOF);
            if (temp_bool == 0 || temp_bool == 1) {
                newData.MachineFailure = (bool)temp_bool;
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_fail1; while ((c_fail1 = getchar()) != '\n' && c_fail1 != EOF);
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

    int confirm_choice;
    while (1) {
        printf("\nConfirmar inserção? (0-Não, 1-Sim): ");
        if (scanf("%d", &confirm_choice) == 1) {
            int c_confirm; while ((c_confirm = getchar()) != '\n' && c_confirm != EOF);

            if (confirm_choice == 1) {
                enqueue(queue, newData);
                printf("Amostra inserida com sucesso!\n");
                break;
            } else if (confirm_choice == 0) {
                printf("Inserção cancelada.\n");
                break;
            } else {
                printf("Entrada inválida. Por favor, digite 0 para Não ou 1 para Sim.\n");
            }
        } else {
            printf("Entrada inválida. Por favor, digite um número (0 ou 1).\n");
            int c_confirm; while ((c_confirm = getchar()) != '\n' && c_confirm != EOF);
        }
    }
}

void searchByProductID(CircularQueue* queue, const char* pid) {
    bool achou = false;
    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        if (strcmp(queue->data[index].ProductID, pid) == 0) {
            displayItem(queue->data[index]);
            achou = true;
        }
    }
    if (!achou) printf("Nenhum item com ProductID %s\n", pid);
}

void searchByType(CircularQueue* queue, char type) {
    bool achou = false;
    type = toupper(type);
    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        if (toupper(queue->data[index].Type) == type) {
            displayItem(queue->data[index]);
            achou = true;
        }
    }
    if (!achou) printf("Nenhum item do tipo %c\n", type);
}

void searchByMachineFailure(CircularQueue* queue, bool f) {
    bool achou = false;
    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        if (queue->data[index].MachineFailure == f) {
            displayItem(queue->data[index]);
            achou = true;
        }
    }
    if (!achou) printf("Nenhum item com falha %d\n", f);
}

// Removing an arbitrary element from a circular queue is not a standard, efficient operation.
// It would involve shifting elements or using a more complex data structure.
// For this translation, I will omit `removeByProductID` as it doesn't align with
// the optimized circular queue paradigm, which is primarily FIFO.
// If this functionality is crucial, a different data structure should be considered.
void removeUnsupported(const char* feature) {
    printf("Operação \"%s\" não suportada ou não otimizada para uma fila circular.\n", feature);
}

void displayStats(const char* title, float avg, float max, float min, float stdDev) {
    printf("\n%s:\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           title, avg, max, min, stdDev);
}

void calculateStatistics(CircularQueue* queue) {
    if (queue->size == 0) {
        printf("Fila vazia. Nenhum dado para análise.\n");
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

    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        MachineData current_data = queue->data[index];

        // Cálculos para ToolWear
        twSum += current_data.ToolWear;
        if (current_data.ToolWear > twMax) twMax = current_data.ToolWear;
        if (current_data.ToolWear < twMin) twMin = current_data.ToolWear;
        
        // Cálculos para Torque
        tqSum += current_data.Torque;
        if (current_data.Torque > tqMax) tqMax = current_data.Torque;
        if (current_data.Torque < tqMin) tqMin = current_data.Torque;
        
        // Cálculos para RotationalSpeed
        rsSum += current_data.RotationalSpeed;
        if (current_data.RotationalSpeed > rsMax) rsMax = current_data.RotationalSpeed;
        if (current_data.RotationalSpeed < rsMin) rsMin = current_data.RotationalSpeed;
        
        // Cálculos para diferença de temperatura
        float diff = current_data.ProcessTemp - current_data.AirTemp;
        tempDiffSum += diff;
        if (diff > tempDiffMax) tempDiffMax = diff;
        if (diff < tempDiffMin) tempDiffMin = diff;
    }

    // Cálculo das médias
    float twAvg = twSum / queue->size;
    float tqAvg = tqSum / queue->size;
    float rsAvg = (float)rsSum / queue->size;
    float tempDiffAvg = tempDiffSum / queue->size;

    // Segunda passada para calcular desvios padrão
    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        MachineData current_data = queue->data[index];
        twSqDiffSum += pow(current_data.ToolWear - twAvg, 2);
        tqSqDiffSum += pow(current_data.Torque - tqAvg, 2);
        rsSqDiffSum += pow(current_data.RotationalSpeed - rsAvg, 2);
        float diff = current_data.ProcessTemp - current_data.AirTemp;
        tempDiffSqDiffSum += pow(diff - tempDiffAvg, 2);
    }

    // Cálculo dos desvios padrão
    float twStdDev = sqrt(twSqDiffSum / queue->size);
    float tqStdDev = sqrt(tqSqDiffSum / queue->size);
    float rsStdDev = sqrt(rsSqDiffSum / queue->size);
    float tempDiffStdDev = sqrt(tempDiffSqDiffSum / queue->size);

    // Exibição dos resultados
    printf("\n=== ESTATÍSTICAS DE OPERAÇÃO ===\n");
    displayStats("Desgaste da Ferramenta (ToolWear)", twAvg, twMax, twMin, twStdDev);
    displayStats("Torque (Nm)", tqAvg, tqMax, tqMin, tqStdDev);
    displayStats("Velocidade Rotacional (RPM)", rsAvg, rsMax, rsMin, rsStdDev);
    displayStats("Diferença de Temperatura (ProcessTemp - AirTemp)", tempDiffAvg, tempDiffMax, tempDiffMin, tempDiffStdDev);
}

void classifyFailures(CircularQueue* queue) {
    if (queue->size == 0) {
        printf("Fila vazia. Nenhum dado para análise.\n");
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

    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        MachineData current_data = queue->data[index];

        int typeIndex = -1;
        switch (toupper(current_data.Type)) {
            case 'L': typeIndex = 0; break;
            case 'M': typeIndex = 1; break;
            case 'H': typeIndex = 2; break;
        }

        if (typeIndex != -1) {
            stats[typeIndex].total++;
            
            if (current_data.TWF) {
                stats[typeIndex].twf++;
                totalFailures[0]++;
            }
            if (current_data.HDF) {
                stats[typeIndex].hdf++;
                totalFailures[1]++;
            }
            if (current_data.PWF) {
                stats[typeIndex].pwf++;
                totalFailures[2]++;
            }
            if (current_data.OSF) {
                stats[typeIndex].osf++;
                totalFailures[3]++;
            }
            if (current_data.RNF) {
                stats[typeIndex].rnf++;
                totalFailures[4]++;
            }
        }
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

void advancedFilter(CircularQueue* queue) {
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
    
    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        MachineData current_data = queue->data[index];
        bool match = true;
        
        // Verificar cada critério
        if (criteria[0] && (current_data.ToolWear < minVal[0] || current_data.ToolWear > maxVal[0])) {
            match = false;
        }
        if (match && criteria[1] && (current_data.Torque < minVal[1] || current_data.Torque > maxVal[1])) {
            match = false;
        }
        if (match && criteria[2] && (current_data.RotationalSpeed < minVal[2] || current_data.RotationalSpeed > maxVal[2])) {
            match = false;
        }
        if (match && criteria[3]) {
            float tempDiff = current_data.ProcessTemp - current_data.AirTemp;
            if (tempDiff < minVal[3] || tempDiff > maxVal[3]) {
                match = false;
            }
        }
        if (match && criteria[4] && toupper(current_data.Type) != typeFilter) {
            match = false;
        }
        if (match && criteria[5] && current_data.MachineFailure != failureFilter) {
            match = false;
        }
        
        if (match) {
            printf("UDI: %d | ProductID: %s | Type: %c | ", 
                   current_data.UDI, current_data.ProductID, current_data.Type);
            
            if (criteria[0]) printf("ToolWear: %d | ", current_data.ToolWear);
            if (criteria[1]) printf("Torque: %.1f | ", current_data.Torque);
            if (criteria[2]) printf("RPM: %d | ", current_data.RotationalSpeed);
            if (criteria[3]) {
                float tempDiff = current_data.ProcessTemp - current_data.AirTemp;
                printf("TempDiff: %.1f | ", tempDiff);
            }
            if (criteria[5]) printf("Failure: %d | ", current_data.MachineFailure);
            
            printf("\n");
            matches++;
        }
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
    printf("6. Operação de remoção não suportada (removido ProductID)\n"); // Changed this menu item
    printf("7. Estatísticas\n");
    printf("8. Classificar Falhas\n");
    printf("9. Filtro Avançado\n");
    printf("10. Executar Benchmarks\n");   
	printf("11. Executar restrições\n"); 
    printf("12. Sair\n");
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

void generateRandomData(CircularQueue* queue, int count) {
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
        enqueue(queue, d);
    }
}

void benchmark_insertion(int num_elements) {
    CircularQueue tmp;
    initQueue(&tmp, num_elements); // Capacity equal to elements to insert
    HighPrecisionTimer t;
    start_timer(&t);
    generateRandomData(&tmp, num_elements);
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Inserção (%d elementos): %.3f ms (%.1f elem/ms)\n",
           num_elements, elapsed, num_elements / elapsed);
    freeQueue(&tmp);
}

void benchmark_search(CircularQueue* queue) {
    if (isEmpty(queue)) { printf("Fila vazia para busca\n"); return; }
    HighPrecisionTimer t;
    const int searches = 10000;
    int found = 0;
    start_timer(&t);
    for (int i = 0; i < searches; i++) {
        char id[10];
        snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
        for (int j = 0; j < queue->size; j++) {
            int index = (queue->front + j) % queue->capacity;
            if (strcmp(queue->data[index].ProductID, id) == 0) { found++; break; }
        }
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Busca (%d ops): encontrados=%d | tempo=%.3f ms (%.1f ops/ms)\n",
           searches, found, elapsed, searches / elapsed);
}

void benchmark_dequeue(CircularQueue* queue) { // Renamed from benchmark_removal
    if (isEmpty(queue)) { printf("Fila vazia para remoção (dequeue)\n"); return; }
    CircularQueue tmp;
    initQueue(&tmp, queue->capacity);
    // Copy elements for testing dequeue without altering original
    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        enqueue(&tmp, queue->data[index]);
    }

    HighPrecisionTimer t;
    const int dequeues = tmp.size / 2 > 0 ? tmp.size / 2 : 1; // Dequeue about half the elements
    MachineData dummy;
    start_timer(&t);
    for (int i = 0; i < dequeues; i++) {
        dequeue(&tmp, &dummy);
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Dequeue (%d ops): %.3f ms (%.1f ops/ms)\n",
           dequeues, elapsed, dequeues / elapsed);
    freeQueue(&tmp);
}

// Função para calcular o tamanho de uma `MachineData` (equivalente a um "nó" de dados)
size_t calculate_data_size() {
    return sizeof(MachineData);
}

// Função para estimar o uso total de memória da fila
void estimate_memory_usage(CircularQueue* queue) {
    if (queue == NULL) {
        printf("Fila inválida\n");
        return;
    }
    
    size_t data_element_size = calculate_data_size();
    size_t total_memory = queue->capacity * data_element_size + sizeof(CircularQueue); // data array + queue struct itself
    
    printf("\n=== ESTIMATIVA DE USO DE MEMÓRIA ===\n");
    printf("Tamanho por elemento de dado (MachineData): %zu bytes\n", data_element_size);
    printf("Capacidade da fila: %d\n", queue->capacity);
    printf("Número atual de elementos: %d\n", queue->size);
    printf("Memória total estimada (array de dados + estrutura da fila): %zu bytes (%.2f KB)\n", 
           total_memory, (float)total_memory / 1024);
    
    printf("\nComparação com sizeof:\n");
    printf("sizeof(MachineData): %zu bytes\n", sizeof(MachineData));
    printf("sizeof(CircularQueue): %zu bytes\n", sizeof(CircularQueue));
    printf("Obs: Pode haver padding/alignment pelo compilador\n");
}

// Benchmark de tempo médio de acesso (simulando acesso aleatório via iteração)
void benchmark_random_access(CircularQueue* queue) {
    if (isEmpty(queue)) {
        printf("Fila vazia para teste de acesso aleatório\n");
        return;
    }

    const int accesses = 10000;
    HighPrecisionTimer t;
    int sum = 0; // Para evitar otimização

    start_timer(&t);
    for (int i = 0; i < accesses; i++) {
        int random_offset = rand() % queue->size; // Offset within current valid elements
        int index = (queue->front + random_offset) % queue->capacity;
        sum += queue->data[index].UDI; // Operação qualquer para evitar otimização
    }
    double elapsed = stop_timer(&t);

    printf("Benchmark Acesso Aleatório (%d acessos): %.3f ms (%.1f acessos/ms)\n",
           accesses, elapsed, accesses / elapsed);
}

// Benchmark de escalabilidade
void benchmark_scalability() {
    printf("\nBenchmark de Escalabilidade (com capacidade fixa):\n");
    int capacities[] = {1000, 5000, 10000, 20000, 50000};
    int num_capacities = sizeof(capacities) / sizeof(capacities[0]);

    for (int i = 0; i < num_capacities; i++) {
        CircularQueue queue;
        initQueue(&queue, capacities[i]);
        
        HighPrecisionTimer t;
        start_timer(&t);
        // Fill the queue to its capacity
        generateRandomData(&queue, capacities[i]);
        double elapsed = stop_timer(&t);
        
        printf("Capacidade: %6d elementos | Tempo de enchimento: %7.3f ms | Tempo por elemento: %.5f ms\n",
               capacities[i], elapsed, elapsed / capacities[i]);
        
        freeQueue(&queue);
    }
}

// Benchmark de latência média (operações combinadas)
void benchmark_combined_operations() {
    const int num_operations = 1000;
    CircularQueue queue;
    initQueue(&queue, DEFAULT_QUEUE_CAPACITY); // Initial queue for combined ops
    generateRandomData(&queue, 1000); // Start with some elements

    HighPrecisionTimer t;
    start_timer(&t);
    
    for (int i = 0; i < num_operations; i++) {
        // Operação de inserção (30% das vezes)
        if (rand() % 100 < 30) {
            MachineData d = {0};
            d.UDI = 10000 + i;
            snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
            enqueue(&queue, d); // Enqueue will handle full queue (overwrite oldest)
        }
        // Operação de busca (50% das vezes)
        else if (rand() % 100 < 80) { // 30% inserção + 50% busca = 80%
            char id[10];
            snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
            for (int j = 0; j < queue.size; j++) {
                int index = (queue.front + j) % queue.capacity;
                if (strcmp(queue.data[index].ProductID, id) == 0) break;
            }
        }
        // Operação de remoção (20% das vezes)
        else {
            MachineData dummy;
            dequeue(&queue, &dummy); // Dequeue from front
        }
    }
    
    double elapsed = stop_timer(&t);
    printf("Benchmark Operações Combinadas (%d ops): %.3f ms | Latência média: %.3f ms/op\n",
           num_operations, elapsed, elapsed / num_operations);
    
    freeQueue(&queue);
}

void run_all_benchmarks(CircularQueue* queue) {
    printf("\n=== INICIANDO BENCHMARKS COMPLETOS ===\n");
    
    // 1. Benchmark de Inserção (onto a fresh queue for precise timing)
    printf("\n1. Tempo de Inserção:\n");
    benchmark_insertion(1000);
    benchmark_insertion(10000);
    
    // 2. Benchmark de Dequeue (remoção da frente)
    printf("\n2. Tempo de Dequeue:\n");
    benchmark_dequeue(queue); // Use the main queue for this test
    
    // 3. Benchmark de Busca
    printf("\n3. Tempo de Busca:\n");
    benchmark_search(queue);
    
    // 4. Benchmark de Uso de Memória
    printf("\n4. Uso de Memória:\n");
    estimate_memory_usage(queue);
    
    // 5. Benchmark de Tempo Médio de Acesso
    printf("\n5. Tempo Médio de Acesso:\n");
    benchmark_random_access(queue);
    
    // 6. Benchmark de Escalabilidade
    printf("\n6. Escalabilidade:\n");
    benchmark_scalability();
    
    // 7. Benchmark de Latência Média
    printf("\n7. Latência Média (operações combinadas):\n");
    benchmark_combined_operations();
    
    printf("\n=== BENCHMARKS CONCLUÍDOS ===\n");
}

void generateAnomalousData(CircularQueue* queue, int count, int max_size) {
    // R2: Limitação de tamanho is handled by enqueue automatically overwriting
    // when the queue is full. The 'max_size' parameter now controls the capacity.
    // Ensure the queue is initialized with this max_size.
    
    for (int i = 0; i < count; i++) {
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

        enqueue(queue, d); // enqueue automatically handles R2 (overwriting)

        if (i > 0 && i % 100 == 0) {
            Sleep(50); // R13: Delay artificial por lote
        }
    }
}

// R24: Ordenação por algoritmo ineficiente - adapted for a circular queue.
// Note: Sorting a queue in place is not a standard or efficient operation.
// This function will copy the queue elements to a temporary array, sort it, and display.
// The queue itself remains unsorted.
void selectionSortQueueData(CircularQueue* queue) {
    if (isEmpty(queue) || queue->size < 2) return;

    // Copy queue data to a temporary array for sorting
    MachineData* temp_array = (MachineData*)malloc(sizeof(MachineData) * queue->size);
    if (temp_array == NULL) {
        perror("Erro ao alocar memória para ordenação");
        return;
    }

    for (int i = 0; i < queue->size; i++) {
        int index = (queue->front + i) % queue->capacity;
        temp_array[i] = queue->data[index];
    }

    // Apply selection sort to the temporary array
    for (int i = 0; i < queue->size - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < queue->size; j++) {
            if (temp_array[j].UDI < temp_array[min_idx].UDI) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            MachineData temp = temp_array[i];
            temp_array[i] = temp_array[min_idx];
            temp_array[min_idx] = temp;
        }
    }

    // Display the sorted data (the queue itself is not modified)
    printf("\nDados da fila (ordenados por UDI para demonstração R24):\n");
    for (int i = 0; i < queue->size; i++) {
        displayItem(temp_array[i]);
    }

    free(temp_array);
}

void run_restricted_benchmarks() {
    printf("\n=== BENCHMARK COM RESTRIÇÕES ATIVADAS ===\n");

    CircularQueue queue;
    int restricted_capacity = 500; // R2: Limitação de tamanho (max_size)
    initQueue(&queue, restricted_capacity);

    HighPrecisionTimer t;
    start_timer(&t);

    // R2, R10, R13, R18 applied during data generation
    generateAnomalousData(&queue, 1000, restricted_capacity); // Try to insert 1000 into a queue of 500

    double elapsed = stop_timer(&t);

    printf("\nTempo total de geração de dados (com 4 restrições aplicadas): %.3f ms\n", elapsed);
    printf("Elementos finais na fila (máximo %d): %d\n", restricted_capacity, queue.size);

    // R24 – Ordenação por algoritmo ineficiente
    printf("\nAplicando R24: ordenação ineficiente (selection sort) para visualização...\n");
    selectionSortQueueData(&queue); 

    // Benchmarks após restrições
    benchmark_search(&queue);
    benchmark_dequeue(&queue); // Test dequeue after restricted data generation
    benchmark_random_access(&queue);
    estimate_memory_usage(&queue);

    freeQueue(&queue);

    printf("\n=== FIM DOS TESTES COM RESTRIÇÕES ===\n");
}

int main() {
    CircularQueue queue;
    initQueue(&queue, DEFAULT_QUEUE_CAPACITY); // Initialize with a default capacity
    parseCSV(&queue);

    int choice;
    char input[64];
    do {
        displayMenu();
        if (!fgets(input, sizeof(input), stdin)) {
            printf("Erro de leitura. Saindo...\n");
            break;
        }
        choice = atoi(input);
        // Consume any remaining characters in the input buffer if atoi didn't read the whole line
        if (strchr(input, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        switch (choice) {
            case 1:
                displayAll(&queue);
                break;
            case 2:
                printf("Digite o ProductID: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0';
                    searchByProductID(&queue, input);
                }
                break;
            case 3: {
                printf("Digite o Type (M/L/H): ");
                char type_char_array[10]; // Use an array to read char with fgets
                if (fgets(type_char_array, sizeof(type_char_array), stdin) && type_char_array[0] != '\n') {
                    searchByType(&queue, type_char_array[0]);
                } else {
                    printf("Entrada inválida para Type.\n");
                }
                break;
            }
            case 4: {
                printf("Digite o MachineFailure (0 ou 1): ");
                if (fgets(input, sizeof(input), stdin)) {
                    int failure_val = atoi(input);
                    searchByMachineFailure(&queue, (bool)failure_val);
                }
                break;
            }
            case 5:
    			insertManualSample(&queue);
    			break;
            case 6: { // MODIFICADO AQUI
                MachineData removed_data;
                if (dequeue(&queue, &removed_data)) {
                    printf("Item removido do início da fila (UDI: %d, ProductID: %s).\n", 
                           removed_data.UDI, removed_data.ProductID);
                } else {
                    printf("A fila está vazia. Nenhum item para remover.\n");
                }
                break;
            }
            case 7:
                calculateStatistics(&queue);
                break;
            case 8:
                classifyFailures(&queue);
                break;
            case 9:
                advancedFilter(&queue);
                break;
            case 10:
                run_all_benchmarks(&queue);
                break;
            case 11:
            	run_restricted_benchmarks();
				break;
            case 12:
                printf("Saindo...\n");
                break;
            default:
                printf("Opção inválida. Tente novamente.\n");
        }
    } while (choice != 12);

    freeQueue(&queue);
    return 0;
}