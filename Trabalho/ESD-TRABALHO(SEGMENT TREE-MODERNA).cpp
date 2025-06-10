#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <profileapi.h>
#include <psapi.h>

#define MAX_LINHA 2048
#define MAX_PRODUCTS 100000  // Capacidade inicial aumentada

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

typedef struct {
    MachineData* data;
    int size;
    int capacity;
    
    // Estruturas para estatísticas rápidas
    float* maxToolWear;
    float* minToolWear;
    float* sumToolWear;
    
    float* maxTorque;
    float* minTorque;
    float* sumTorque;
    
    int* maxRPM;
    int* minRPM;
    int* sumRPM;
    
    float* maxTempDiff;
    float* minTempDiff;
    float* sumTempDiff;
} SegmentTree;

// Funções auxiliares para a Segment Tree
int nextPowerOfTwo(int n) {
    int power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

void initSegmentTree(SegmentTree* st, int capacity) {
    st->capacity = nextPowerOfTwo(capacity);
    st->size = 0;
    
    // Alocar espaço para os dados
    st->data = (MachineData*)malloc(2 * st->capacity * sizeof(MachineData));
    
    // Alocar estruturas para estatísticas
    st->maxToolWear = (float*)malloc(2 * st->capacity * sizeof(float));
    st->minToolWear = (float*)malloc(2 * st->capacity * sizeof(float));
    st->sumToolWear = (float*)malloc(2 * st->capacity * sizeof(float));
    
    st->maxTorque = (float*)malloc(2 * st->capacity * sizeof(float));
    st->minTorque = (float*)malloc(2 * st->capacity * sizeof(float));
    st->sumTorque = (float*)malloc(2 * st->capacity * sizeof(float));
    
    st->maxRPM = (int*)malloc(2 * st->capacity * sizeof(int));
    st->minRPM = (int*)malloc(2 * st->capacity * sizeof(int));
    st->sumRPM = (int*)malloc(2 * st->capacity * sizeof(int));
    
    st->maxTempDiff = (float*)malloc(2 * st->capacity * sizeof(float));
    st->minTempDiff = (float*)malloc(2 * st->capacity * sizeof(float));
    st->sumTempDiff = (float*)malloc(2 * st->capacity * sizeof(float));
    
    if (!st->data || !st->maxToolWear || !st->minToolWear || !st->sumToolWear ||
        !st->maxTorque || !st->minTorque || !st->sumTorque ||
        !st->maxRPM || !st->minRPM || !st->sumRPM ||
        !st->maxTempDiff || !st->minTempDiff || !st->sumTempDiff) {
        perror("Falha ao alocar memória para Segment Tree");
        exit(EXIT_FAILURE);
    }
}

void resizeSegmentTree(SegmentTree* st) {
    int new_capacity = st->capacity * 2;
    
    // Realocar dados
    st->data = (MachineData*)realloc(st->data, 2 * new_capacity * sizeof(MachineData));
    
    // Realocar estruturas de estatísticas
    st->maxToolWear = (float*)realloc(st->maxToolWear, 2 * new_capacity * sizeof(float));
    st->minToolWear = (float*)realloc(st->minToolWear, 2 * new_capacity * sizeof(float));
    st->sumToolWear = (float*)realloc(st->sumToolWear, 2 * new_capacity * sizeof(float));
    
    st->maxTorque = (float*)realloc(st->maxTorque, 2 * new_capacity * sizeof(float));
    st->minTorque = (float*)realloc(st->minTorque, 2 * new_capacity * sizeof(float));
    st->sumTorque = (float*)realloc(st->sumTorque, 2 * new_capacity * sizeof(float));
    
    st->maxRPM = (int*)realloc(st->maxRPM, 2 * new_capacity * sizeof(int));
    st->minRPM = (int*)realloc(st->minRPM, 2 * new_capacity * sizeof(int));
    st->sumRPM = (int*)realloc(st->sumRPM, 2 * new_capacity * sizeof(int));
    
    st->maxTempDiff = (float*)realloc(st->maxTempDiff, 2 * new_capacity * sizeof(float));
    st->minTempDiff = (float*)realloc(st->minTempDiff, 2 * new_capacity * sizeof(float));
    st->sumTempDiff = (float*)realloc(st->sumTempDiff, 2 * new_capacity * sizeof(float));
    
    if (!st->data || !st->maxToolWear || !st->minToolWear || !st->sumToolWear ||
        !st->maxTorque || !st->minTorque || !st->sumTorque ||
        !st->maxRPM || !st->minRPM || !st->sumRPM ||
        !st->maxTempDiff || !st->minTempDiff || !st->sumTempDiff) {
        perror("Falha ao realocar memória para Segment Tree");
        exit(EXIT_FAILURE);
    }
    
    st->capacity = new_capacity;
}

void updateNode(SegmentTree* st, int pos) {
    // Atualiza as estatísticas para o nó na posição pos
    int left = 2 * pos;
    int right = 2 * pos + 1;
    
    // ToolWear
    st->maxToolWear[pos] = fmax(st->maxToolWear[left], st->maxToolWear[right]);
    st->minToolWear[pos] = fmin(st->minToolWear[left], st->minToolWear[right]);
    st->sumToolWear[pos] = st->sumToolWear[left] + st->sumToolWear[right];
    
    // Torque
    st->maxTorque[pos] = fmax(st->maxTorque[left], st->maxTorque[right]);
    st->minTorque[pos] = fmin(st->minTorque[left], st->minTorque[right]);
    st->sumTorque[pos] = st->sumTorque[left] + st->sumTorque[right];
    
    // RPM
    st->maxRPM[pos] = (st->maxRPM[left] > st->maxRPM[right]) ? st->maxRPM[left] : st->maxRPM[right];
    st->minRPM[pos] = (st->minRPM[left] < st->minRPM[right]) ? st->minRPM[left] : st->minRPM[right];
    st->sumRPM[pos] = st->sumRPM[left] + st->sumRPM[right];
    
    // TempDiff
    float tempDiffLeft = st->data[left].ProcessTemp - st->data[left].AirTemp;
    float tempDiffRight = st->data[right].ProcessTemp - st->data[right].AirTemp;
    st->maxTempDiff[pos] = fmax(tempDiffLeft, tempDiffRight);
    st->minTempDiff[pos] = fmin(tempDiffLeft, tempDiffRight);
    st->sumTempDiff[pos] = tempDiffLeft + tempDiffRight;
}

void append(SegmentTree* st, MachineData data) {
    if (st->size >= st->capacity) {
        resizeSegmentTree(st);
    }
    
    int pos = st->capacity + st->size;
    st->data[pos] = data;
    
    // Inicializar estatísticas para a folha
    st->maxToolWear[pos] = data.ToolWear;
    st->minToolWear[pos] = data.ToolWear;
    st->sumToolWear[pos] = data.ToolWear;
    
    st->maxTorque[pos] = data.Torque;
    st->minTorque[pos] = data.Torque;
    st->sumTorque[pos] = data.Torque;
    
    st->maxRPM[pos] = data.RotationalSpeed;
    st->minRPM[pos] = data.RotationalSpeed;
    st->sumRPM[pos] = data.RotationalSpeed;
    
    float tempDiff = data.ProcessTemp - data.AirTemp;
    st->maxTempDiff[pos] = tempDiff;
    st->minTempDiff[pos] = tempDiff;
    st->sumTempDiff[pos] = tempDiff;
    
    st->size++;
    
    // Atualizar a árvore
    for (pos >>= 1; pos >= 1; pos >>= 1) {
        updateNode(st, pos);
    }
}

void removerAspas(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '\"') *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

void parseCSV(SegmentTree* st) {
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
        
        append(st, d);
    }
    
    fclose(f);
}

void displayItem(MachineData d) {
    printf("UDI: %d | ProductID: %s | Type: %c | AirTemp: %.1f | ProcessTemp: %.1f | RPM: %d | Torque: %.1f | ToolWear: %d | Failure: %d\n",
           d.UDI, d.ProductID, d.Type, d.AirTemp, d.ProcessTemp,
           d.RotationalSpeed, d.Torque, d.ToolWear, d.MachineFailure);
}

void displayAll(SegmentTree* st) {
    for (int i = 0; i < st->size; i++) {
        displayItem(st->data[st->capacity + i]);
    }
}

// Função para inserir novas amostras manualmente
void insertManualSample(SegmentTree* st) {
    MachineData newData;
    printf("\n=== INSERIR NOVA AMOSTRA MANUALMENTE ===\n");

    // Encontrar o próximo UDI disponível
    int maxUDI = 0;
    for (int i = 0; i < st->size; i++) {
        if (st->data[st->capacity + i].UDI > maxUDI) {
            maxUDI = st->data[st->capacity + i].UDI;
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
                append(st, newData);
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

void searchByProductID(SegmentTree* st, const char* pid) {
    bool achou = false;
    for (int i = 0; i < st->size; i++) {
        if (strcmp(st->data[st->capacity + i].ProductID, pid) == 0) {
            displayItem(st->data[st->capacity + i]);
            achou = true;
        }
    }
    if (!achou) printf("Nenhum item com ProductID %s\n", pid);
}

void searchByType(SegmentTree* st, char type) {
    bool achou = false;
    type = toupper(type);
    for (int i = 0; i < st->size; i++) {
        if (toupper(st->data[st->capacity + i].Type) == type) {
            displayItem(st->data[st->capacity + i]);
            achou = true;
        }
    }
    if (!achou) printf("Nenhum item do tipo %c\n", type);
}

void searchByMachineFailure(SegmentTree* st, bool f) {
    bool achou = false;
    for (int i = 0; i < st->size; i++) {
        if (st->data[st->capacity + i].MachineFailure == f) {
            displayItem(st->data[st->capacity + i]);
            achou = true;
        }
    }
    if (!achou) printf("Nenhum item com falha %d\n", f);
}

bool removeByProductID(SegmentTree* st, const char* pid) {
    bool removed = false;
    int newSize = 0;
    
    for (int i = 0; i < st->size; i++) {
        if (strcmp(st->data[st->capacity + i].ProductID, pid) != 0) {
            st->data[st->capacity + newSize] = st->data[st->capacity + i];
            newSize++;
        } else {
            removed = true;
        }
    }
    
    st->size = newSize;
    
    // Reconstruir a árvore
    for (int i = st->capacity; i < st->capacity + st->size; i++) {
        // Atualizar estatísticas nas folhas
        st->maxToolWear[i] = st->data[i].ToolWear;
        st->minToolWear[i] = st->data[i].ToolWear;
        st->sumToolWear[i] = st->data[i].ToolWear;
        
        st->maxTorque[i] = st->data[i].Torque;
        st->minTorque[i] = st->data[i].Torque;
        st->sumTorque[i] = st->data[i].Torque;
        
        st->maxRPM[i] = st->data[i].RotationalSpeed;
        st->minRPM[i] = st->data[i].RotationalSpeed;
        st->sumRPM[i] = st->data[i].RotationalSpeed;
        
        float tempDiff = st->data[i].ProcessTemp - st->data[i].AirTemp;
        st->maxTempDiff[i] = tempDiff;
        st->minTempDiff[i] = tempDiff;
        st->sumTempDiff[i] = tempDiff;
    }
    
    // Atualizar nós internos
    for (int i = st->capacity - 1; i >= 1; i--) {
        updateNode(st, i);
    }
    
    return removed;
}

void displayStats(const char* title, float avg, float max, float min, float stdDev) {
    printf("\n%s:\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           title, avg, max, min, stdDev);
}

void calculateStatistics(SegmentTree* st) {
    if (st->size == 0) {
        printf("Lista vazia. Nenhum dado para análise.\n");
        return;
    }

    // Usando a Segment Tree para obter estatísticas rapidamente
    float twAvg = st->sumToolWear[1] / st->size;
    float twMax = st->maxToolWear[1];
    float twMin = st->minToolWear[1];
    
    float tqAvg = st->sumTorque[1] / st->size;
    float tqMax = st->maxTorque[1];
    float tqMin = st->minTorque[1];
    
    float rsAvg = (float)st->sumRPM[1] / st->size;
    int rsMax = st->maxRPM[1];
    int rsMin = st->minRPM[1];
    
    float tempDiffAvg = st->sumTempDiff[1] / st->size;
    float tempDiffMax = st->maxTempDiff[1];
    float tempDiffMin = st->minTempDiff[1];

    // Segunda passada para calcular desvios padrão (ainda necessário)
    float twSqDiffSum = 0;
    float tqSqDiffSum = 0;
    float rsSqDiffSum = 0;
    float tempDiffSqDiffSum = 0;
    
    for (int i = 0; i < st->size; i++) {
        twSqDiffSum += pow(st->data[st->capacity + i].ToolWear - twAvg, 2);
        tqSqDiffSum += pow(st->data[st->capacity + i].Torque - tqAvg, 2);
        rsSqDiffSum += pow(st->data[st->capacity + i].RotationalSpeed - rsAvg, 2);
        float diff = st->data[st->capacity + i].ProcessTemp - st->data[st->capacity + i].AirTemp;
        tempDiffSqDiffSum += pow(diff - tempDiffAvg, 2);
    }

    // Cálculo dos desvios padrão
    float twStdDev = sqrt(twSqDiffSum / st->size);
    float tqStdDev = sqrt(tqSqDiffSum / st->size);
    float rsStdDev = sqrt(rsSqDiffSum / st->size);
    float tempDiffStdDev = sqrt(tempDiffSqDiffSum / st->size);

    // Exibição dos resultados
    printf("\n=== ESTATÍSTICAS DE OPERAÇÃO ===\n");
    displayStats("Desgaste da Ferramenta (ToolWear)", twAvg, twMax, twMin, twStdDev);
    displayStats("Torque (Nm)", tqAvg, tqMax, tqMin, tqStdDev);
    displayStats("Velocidade Rotacional (RPM)", rsAvg, rsMax, rsMin, rsStdDev);
    displayStats("Diferença de Temperatura (ProcessTemp - AirTemp)", tempDiffAvg, tempDiffMax, tempDiffMin, tempDiffStdDev);
}

void classifyFailures(SegmentTree* st) {
    if (st->size == 0) {
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

    for (int i = 0; i < st->size; i++) {
        int typeIndex = -1;
        switch (toupper(st->data[st->capacity + i].Type)) {
            case 'L': typeIndex = 0; break;
            case 'M': typeIndex = 1; break;
            case 'H': typeIndex = 2; break;
        }

        if (typeIndex != -1) {
            stats[typeIndex].total++;
            
            if (st->data[st->capacity + i].TWF) {
                stats[typeIndex].twf++;
                totalFailures[0]++;
            }
            if (st->data[st->capacity + i].HDF) {
                stats[typeIndex].hdf++;
                totalFailures[1]++;
            }
            if (st->data[st->capacity + i].PWF) {
                stats[typeIndex].pwf++;
                totalFailures[2]++;
            }
            if (st->data[st->capacity + i].OSF) {
                stats[typeIndex].osf++;
                totalFailures[3]++;
            }
            if (st->data[st->capacity + i].RNF) {
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

void advancedFilter(SegmentTree* st) {
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
    
    for (int i = 0; i < st->size; i++) {
        MachineData data = st->data[st->capacity + i];
        bool match = true;
        
        // Verificar cada critério
        if (criteria[0] && (data.ToolWear < minVal[0] || data.ToolWear > maxVal[0])) {
            match = false;
        }
        if (match && criteria[1] && (data.Torque < minVal[1] || data.Torque > maxVal[1])) {
            match = false;
        }
        if (match && criteria[2] && (data.RotationalSpeed < minVal[2] || data.RotationalSpeed > maxVal[2])) {
            match = false;
        }
        if (match && criteria[3]) {
            float tempDiff = data.ProcessTemp - data.AirTemp;
            if (tempDiff < minVal[3] || tempDiff > maxVal[3]) {
                match = false;
            }
        }
        if (match && criteria[4] && toupper(data.Type) != typeFilter) {
            match = false;
        }
        if (match && criteria[5] && data.MachineFailure != failureFilter) {
            match = false;
        }
        
        if (match) {
            printf("UDI: %d | ProductID: %s | Type: %c | ", 
                   data.UDI, data.ProductID, data.Type);
            
            if (criteria[0]) printf("ToolWear: %d | ", data.ToolWear);
            if (criteria[1]) printf("Torque: %.1f | ", data.Torque);
            if (criteria[2]) printf("RPM: %d | ", data.RotationalSpeed);
            if (criteria[3]) {
                float tempDiff = data.ProcessTemp - data.AirTemp;
                printf("TempDiff: %.1f | ", tempDiff);
            }
            if (criteria[5]) printf("Failure: %d | ", data.MachineFailure);
            
            printf("\n");
            matches++;
        }
    }
    
    printf("\nTotal de máquinas que atendem aos critérios: %d\n", matches);
}

// Timer de alta precisão
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;
} HighPrecisionTimer;

void start_timer(HighPrecisionTimer* timer) {
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->start);
}

double stop_timer(HighPrecisionTimer* timer) {
    QueryPerformanceCounter(&timer->end);
    return (double)(timer->end.QuadPart - timer->start.QuadPart) * 1000.0 / timer->frequency.QuadPart;
}

void generateRandomData(SegmentTree* st, int count) {
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
        append(st, d);
    }
}

void benchmark_insertion(SegmentTree* st, int num_elements) {
    HighPrecisionTimer t;
    start_timer(&t);
    SegmentTree tmp;
    initSegmentTree(&tmp, num_elements);
    generateRandomData(&tmp, num_elements);
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Inserção (%d elementos): %.3f ms (%.1f elem/ms)\n",
           num_elements, elapsed, num_elements / elapsed);
    
    // Liberar memória
    free(tmp.data);
    free(tmp.maxToolWear);
    free(tmp.minToolWear);
    free(tmp.sumToolWear);
    free(tmp.maxTorque);
    free(tmp.minTorque);
    free(tmp.sumTorque);
    free(tmp.maxRPM);
    free(tmp.minRPM);
    free(tmp.sumRPM);
    free(tmp.maxTempDiff);
    free(tmp.minTempDiff);
    free(tmp.sumTempDiff);
}

void benchmark_search(SegmentTree* st) {
    if (st->size == 0) { printf("Lista vazia para busca\n"); return; }
    HighPrecisionTimer t;
    const int searches = 10000;
    int found = 0;
    start_timer(&t);
    for (int i = 0; i < searches; i++) {
        char id[10];
        snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
        for (int j = 0; j < st->size; j++) {
            if (strcmp(st->data[st->capacity + j].ProductID, id) == 0) { 
                found++; 
                break; 
            }
        }
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Busca (%d ops): encontrados=%d | tempo=%.3f ms (%.1f ops/ms)\n",
           searches, found, elapsed, searches / elapsed);
}

void benchmark_removal(SegmentTree* st) {
    if (st->size == 0) { printf("Lista vazia para remoção\n"); return; }
    SegmentTree tmp;
    initSegmentTree(&tmp, st->size);
    for (int i = 0; i < st->size; i++) {
        append(&tmp, st->data[st->capacity + i]);
    }
    
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
    
    // Liberar memória
    free(tmp.data);
    free(tmp.maxToolWear);
    free(tmp.minToolWear);
    free(tmp.sumToolWear);
    free(tmp.maxTorque);
    free(tmp.minTorque);
    free(tmp.sumTorque);
    free(tmp.maxRPM);
    free(tmp.minRPM);
    free(tmp.sumRPM);
    free(tmp.maxTempDiff);
    free(tmp.minTempDiff);
    free(tmp.sumTempDiff);
}

void estimate_memory_usage(SegmentTree* st) {
    if (st == NULL) {
        printf("Segment Tree inválida\n");
        return;
    }
    
    size_t node_size = sizeof(MachineData) + 
                       sizeof(float) * 9 +  // 9 floats (max/min/sum para 3 métricas)
                       sizeof(int) * 6;     // 6 ints (max/min/sum para RPM)
    
    size_t total_memory = (2 * st->capacity) * node_size;
    
    printf("\n=== ESTIMATIVA DE USO DE MEMÓRIA ===\n");
    printf("Tamanho por nó: %zu bytes\n", node_size);
    printf("Número de nós: %d (capacidade: %d)\n", 2 * st->capacity, st->capacity);
    printf("Memória total estimada: %zu bytes (%.2f KB)\n", 
           total_memory, (float)total_memory / 1024);
    
    printf("\nComparação com sizeof:\n");
    printf("sizeof(MachineData): %zu bytes\n", sizeof(MachineData));
    printf("Obs: Pode haver padding/alignment pelo compilador\n");
}

void benchmark_random_access(SegmentTree* st) {
    if (st->size == 0) {
        printf("Lista vazia para teste de acesso aleatório\n");
        return;
    }

    const int accesses = 10000;
    HighPrecisionTimer t;
    int sum = 0;

    start_timer(&t);
    for (int i = 0; i < accesses; i++) {
        int random_pos = rand() % st->size;
        sum += st->data[st->capacity + random_pos].UDI;
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
        SegmentTree st;
        initSegmentTree(&st, sizes[i]);
        
        HighPrecisionTimer t;
        start_timer(&t);
        generateRandomData(&st, sizes[i]);
        double elapsed = stop_timer(&t);
        
        printf("Tamanho: %6d elementos | Tempo de inserção: %7.3f ms | Tempo por elemento: %.5f ms\n",
               sizes[i], elapsed, elapsed / sizes[i]);
        
        // Liberar memória
        free(st.data);
        free(st.maxToolWear);
        free(st.minToolWear);
        free(st.sumToolWear);
        free(st.maxTorque);
        free(st.minTorque);
        free(st.sumTorque);
        free(st.maxRPM);
        free(st.minRPM);
        free(st.sumRPM);
        free(st.maxTempDiff);
        free(st.minTempDiff);
        free(st.sumTempDiff);
    }
}

void benchmark_combined_operations() {
    const int num_operations = 1000;
    SegmentTree st;
    initSegmentTree(&st, 1000);
    generateRandomData(&st, 1000);

    HighPrecisionTimer t;
    start_timer(&t);
    
    for (int i = 0; i < num_operations; i++) {
        // Operação de inserção (30% das vezes)
        if (rand() % 100 < 30) {
            MachineData d = {0};
            d.UDI = 10000 + i;
            snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
            append(&st, d);
        }
        // Operação de busca (50% das vezes)
        else if (rand() % 100 < 80) {
            char id[10];
            snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
            for (int j = 0; j < st.size; j++) {
                if (strcmp(st.data[st.capacity + j].ProductID, id) == 0) break;
            }
        }
        // Operação de remoção (20% das vezes)
        else {
            char id[10];
            snprintf(id, sizeof(id), "M%07d", rand() % 1000000);
            removeByProductID(&st, id);
        }
    }
    
    double elapsed = stop_timer(&t);
    printf("Benchmark Operações Combinadas (%d ops): %.3f ms | Latência média: %.3f ms/op\n",
           num_operations, elapsed, elapsed / num_operations);
    
    // Liberar memória
    free(st.data);
    free(st.maxToolWear);
    free(st.minToolWear);
    free(st.sumToolWear);
    free(st.maxTorque);
    free(st.minTorque);
    free(st.sumTorque);
    free(st.maxRPM);
    free(st.minRPM);
    free(st.sumRPM);
    free(st.maxTempDiff);
    free(st.minTempDiff);
    free(st.sumTempDiff);
}

void run_all_benchmarks(SegmentTree* st) {
    printf("\n=== INICIANDO BENCHMARKS COMPLETOS ===\n");
    
    // 1. Benchmark de Inserção
    printf("\n1. Tempo de Inserção:\n");
    benchmark_insertion(st, 1000);
    benchmark_insertion(st, 10000);
    
    // 2. Benchmark de Remoção
    printf("\n2. Tempo de Remoção:\n");
    benchmark_removal(st);
    
    // 3. Benchmark de Busca
    printf("\n3. Tempo de Busca:\n");
    benchmark_search(st);
    
    // 4. Benchmark de Uso de Memória
    printf("\n4. Uso de Memória:\n");
    estimate_memory_usage(st);
    
    // 5. Benchmark de Tempo Médio de Acesso
    printf("\n5. Tempo Médio de Acesso:\n");
    benchmark_random_access(st);
    
    // 6. Benchmark de Escalabilidade
    printf("\n6. Escalabilidade:\n");
    benchmark_scalability();
    
    // 7. Benchmark de Latência Média
    printf("\n7. Latência Média (operações combinadas):\n");
    benchmark_combined_operations();
    
    printf("\n=== BENCHMARKS CONCLUÍDOS ===\n");
}

void generateAnomalousData(SegmentTree* st, int count, int max_size) {
    for (int i = 0; i < count; i++) {
        if (max_size > 0 && st->size >= max_size) {
            // Remover o primeiro elemento para manter o tamanho máximo
            removeByProductID(st, st->data[st->capacity].ProductID);
        }

        if (i % 100 == 0) {
            Sleep(1); // Interrupções periódicas
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

        // Inserção de anomalias
        if (rand() % 10 == 0) {
            d.UDI = -1;
            d.AirTemp = -999.0f;
            d.Type = 'X';
        }

        append(st, d);

        if (i > 0 && i % 100 == 0) {
            Sleep(50); // Delay artificial por lote
        }
    }
}

void selectionSort(SegmentTree* st) {
    if (st == NULL || st->size < 2) return;

    for (int i = 0; i < st->size - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < st->size; j++) {
            if (st->data[st->capacity + j].UDI < st->data[st->capacity + min_idx].UDI) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            // Trocar os elementos
            MachineData temp = st->data[st->capacity + i];
            st->data[st->capacity + i] = st->data[st->capacity + min_idx];
            st->data[st->capacity + min_idx] = temp;
        }
    }
    
    // Reconstruir a árvore após a ordenação
    for (int i = st->capacity; i < st->capacity + st->size; i++) {
        // Atualizar estatísticas nas folhas
        st->maxToolWear[i] = st->data[i].ToolWear;
        st->minToolWear[i] = st->data[i].ToolWear;
        st->sumToolWear[i] = st->data[i].ToolWear;
        
        st->maxTorque[i] = st->data[i].Torque;
        st->minTorque[i] = st->data[i].Torque;
        st->sumTorque[i] = st->data[i].Torque;
        
        st->maxRPM[i] = st->data[i].RotationalSpeed;
        st->minRPM[i] = st->data[i].RotationalSpeed;
        st->sumRPM[i] = st->data[i].RotationalSpeed;
        
        float tempDiff = st->data[i].ProcessTemp - st->data[i].AirTemp;
        st->maxTempDiff[i] = tempDiff;
        st->minTempDiff[i] = tempDiff;
        st->sumTempDiff[i] = tempDiff;
    }
    
    // Atualizar nós internos
    for (int i = st->capacity - 1; i >= 1; i--) {
        updateNode(st, i);
    }
}

void run_restricted_benchmarks() {
    printf("\n=== BENCHMARK COM RESTRIÇÕES ATIVADAS ===\n");

    SegmentTree st;
    initSegmentTree(&st, 1000);

    HighPrecisionTimer t;
    start_timer(&t);

    // Gerar dados com restrições
    generateAnomalousData(&st, 1000, 500);

    double elapsed = stop_timer(&t);

    printf("\nTempo total (com 4 restrições aplicadas): %.3f ms\n", elapsed);
    printf("Elementos finais na lista (máximo 500): %d\n", st.size);

    // Ordenação por algoritmo ineficiente
    printf("\nAplicando ordenação ineficiente (selection sort)...\n");
    selectionSort(&st);

    // Benchmarks após restrições
    benchmark_search(&st);
    benchmark_removal(&st);
    benchmark_random_access(&st);
    estimate_memory_usage(&st);

    // Liberar memória
    free(st.data);
    free(st.maxToolWear);
    free(st.minToolWear);
    free(st.sumToolWear);
    free(st.maxTorque);
    free(st.minTorque);
    free(st.sumTorque);
    free(st.maxRPM);
    free(st.minRPM);
    free(st.sumRPM);
    free(st.maxTempDiff);
    free(st.minTempDiff);
    free(st.sumTempDiff);

    printf("\n=== FIM DOS TESTES COM RESTRIÇÕES ===\n");
}

void displayMenu() {
    printf("\nMenu:\n");
    printf("1. Exibir todos\n");
    printf("2. Buscar ProductID\n");
    printf("3. Buscar Type\n");
    printf("4. Buscar MachineFailure\n");
    printf("5. Inserir nova amostra manualmente\n");
    printf("6. Remover ProductID\n");
    printf("7. Estatísticas\n");
    printf("8. Classificar Falhas\n");
    printf("9. Filtro Avançado\n");
    printf("10. Executar Benchmarks\n");   
    printf("11. Executar restrições\n"); 
    printf("12. Sair\n");
    printf("Escolha: ");
}

void freeSegmentTree(SegmentTree* st) {
    free(st->data);
    free(st->maxToolWear);
    free(st->minToolWear);
    free(st->sumToolWear);
    free(st->maxTorque);
    free(st->minTorque);
    free(st->sumTorque);
    free(st->maxRPM);
    free(st->minRPM);
    free(st->sumRPM);
    free(st->maxTempDiff);
    free(st->minTempDiff);
    free(st->sumTempDiff);
    
    st->data = NULL;
    st->size = 0;
    st->capacity = 0;
}

int main() {
    SegmentTree st;
    initSegmentTree(&st, MAX_PRODUCTS);
    parseCSV(&st);

    int choice;
    char input[64];
    do {
        displayMenu();
        if (!fgets(input, sizeof(input), stdin)) break;
        choice = atoi(input);
        switch (choice) {
            case 1:
                displayAll(&st);
                break;
            case 2:
                printf("Digite o ProductID: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0';
                    searchByProductID(&st, input);
                }
                break;
            case 3: {
                printf("Digite o Type (M/L/H): ");
                char type;
                if (scanf(" %c", &type)) {
                    while (getchar() != '\n');
                    searchByType(&st, type);
                }
                break;
            }
            case 4: {
                printf("Digite o MachineFailure (0 ou 1): ");
                int failure;
                if (scanf("%d", &failure)) {
                    while (getchar() != '\n');
                    searchByMachineFailure(&st, failure);
                }
                break;
            }
            case 5:
                insertManualSample(&st);
                break;
            case 6: {
                printf("Digite o ProductID para remover: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0';
                    if (removeByProductID(&st, input))
                        printf("Item(s) removido(s) com sucesso.\n");
                    else
                        printf("Nenhum item encontrado com o ProductID: %s\n", input);
                }
                break;
            }
            case 7:
                calculateStatistics(&st);
                break;
            case 8:
                classifyFailures(&st);
                break;
            case 9:
                advancedFilter(&st);
                break;
            case 10:
                run_all_benchmarks(&st);
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

    freeSegmentTree(&st);
    return 0;
}