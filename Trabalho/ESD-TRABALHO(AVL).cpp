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

// Estrutura de dados para MachineData (mantida igual)
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

// Estrutura do nó da Árvore AVL
typedef struct AVLNode {
    MachineData data;
    int key;                // Usaremos UDI como chave
    struct AVLNode* left;
    struct AVLNode* right;
    int height;             // Altura do nó
} AVLNode;

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

// Estrutura da Árvore AVL
typedef struct {
    AVLNode* root;
    int size;
} AVLTree;

// Funções auxiliares para Árvore AVL
int max(int a, int b) {
    return (a > b) ? a : b;
}

int height(AVLNode* node) {
    if (node == NULL)
        return 0;
    return node->height;
}

int getBalance(AVLNode* node) {
    if (node == NULL)
        return 0;
    return height(node->left) - height(node->right);
}

AVLNode* createNode(int key, MachineData data) {
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    node->key = key;
    node->data = data;
    node->left = NULL;
    node->right = NULL;
    node->height = 1; // Novo nó é inicialmente adicionado como folha
    return node;
}

// Rotação à direita
AVLNode* rightRotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;

    // Realiza a rotação
    x->right = y;
    y->left = T2;

    // Atualiza alturas
    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;
}

// Rotação à esquerda
AVLNode* leftRotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;

    // Realiza a rotação
    y->left = x;
    x->right = T2;

    // Atualiza alturas
    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;

    return y;
}

// Inserção na Árvore AVL
AVLNode* insertAVL(AVLNode* node, int key, MachineData data) {
    // 1. Inserção normal de BST
    if (node == NULL)
        return createNode(key, data);

    if (key < node->key)
        node->left = insertAVL(node->left, key, data);
    else if (key > node->key)
        node->right = insertAVL(node->right, key, data);
    else // Chaves iguais não são permitidas (ou atualiza os dados)
        return node;

    // 2. Atualiza a altura deste nó ancestral
    node->height = 1 + max(height(node->left), height(node->right));

    // 3. Obtém o fator de balanceamento deste nó ancestral
    int balance = getBalance(node);

    // Se o nó ficar desbalanceado, então temos 4 casos

    // Caso Esquerda Esquerda
    if (balance > 1 && key < node->left->key)
        return rightRotate(node);

    // Caso Direita Direita
    if (balance < -1 && key > node->right->key)
        return leftRotate(node);

    // Caso Esquerda Direita
    if (balance > 1 && key > node->left->key) {
        node->left = leftRotate(node->left);
        return rightRotate(node);
    }

    // Caso Direita Esquerda
    if (balance < -1 && key < node->right->key) {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }

    // Retorna o ponteiro do nó (inalterado)
    return node;
}

// Encontra o nó com o menor valor (usado na remoção)
AVLNode* minValueNode(AVLNode* node) {
    AVLNode* current = node;
    while (current->left != NULL)
        current = current->left;
    return current;
}

// Remoção na Árvore AVL
AVLNode* deleteAVL(AVLNode* root, int key) {
    // 1. Realiza a remoção padrão de BST
    if (root == NULL)
        return root;

    if (key < root->key)
        root->left = deleteAVL(root->left, key);
    else if (key > root->key)
        root->right = deleteAVL(root->right, key);
    else {
        // Nó com apenas um filho ou sem filhos
        if ((root->left == NULL) || (root->right == NULL)) {
            AVLNode* temp = root->left ? root->left : root->right;

            // Caso sem filhos
            if (temp == NULL) {
                temp = root;
                root = NULL;
            } else // Caso com um filho
                *root = *temp; // Copia os conteúdos do filho não vazio

            free(temp);
        } else {
            // Nó com dois filhos: obtém o sucessor in-order (menor na subárvore direita)
            AVLNode* temp = minValueNode(root->right);

            // Copia os dados do sucessor in-order para este nó
            root->key = temp->key;
            root->data = temp->data;

            // Remove o sucessor in-order
            root->right = deleteAVL(root->right, temp->key);
        }
    }

    // Se a árvore tinha apenas um nó, então retorna
    if (root == NULL)
        return root;

    // 2. Atualiza a altura do nó atual
    root->height = 1 + max(height(root->left), height(root->right));

    // 3. Obtém o fator de balanceamento deste nó
    int balance = getBalance(root);

    // Se o nó ficar desbalanceado, então temos 4 casos

    // Caso Esquerda Esquerda
    if (balance > 1 && getBalance(root->left) >= 0)
        return rightRotate(root);

    // Caso Esquerda Direita
    if (balance > 1 && getBalance(root->left) < 0) {
        root->left = leftRotate(root->left);
        return rightRotate(root);
    }

    // Caso Direita Direita
    if (balance < -1 && getBalance(root->right) <= 0)
        return leftRotate(root);

    // Caso Direita Esquerda
    if (balance < -1 && getBalance(root->right) > 0) {
        root->right = rightRotate(root->right);
        return leftRotate(root);
    }

    return root;
}

// Busca na Árvore AVL
AVLNode* searchAVL(AVLNode* root, int key) {
    if (root == NULL || root->key == key)
        return root;

    if (root->key < key)
        return searchAVL(root->right, key);

    return searchAVL(root->left, key);
}

// Inicializa a Árvore AVL
void initAVLTree(AVLTree* tree) {
    tree->root = NULL;
    tree->size = 0;
}

// Função para inserir na Árvore AVL (wrapper)
void insertAVLTree(AVLTree* tree, int key, MachineData data) {
    tree->root = insertAVL(tree->root, key, data);
    tree->size++;
}

// Função para remover da Árvore AVL (wrapper)
void deleteAVLTree(AVLTree* tree, int key) {
    tree->root = deleteAVL(tree->root, key);
    tree->size--;
}

// Função para buscar na Árvore AVL (wrapper)
AVLNode* searchAVLTree(AVLTree* tree, int key) {
    return searchAVL(tree->root, key);
}

// Função para liberar a memória da Árvore AVL
void freeAVLTree(AVLNode* node) {
    if (node != NULL) {
        freeAVLTree(node->left);
        freeAVLTree(node->right);
        free(node);
    }
}

// Funções auxiliares (mantidas iguais)
void removerAspas(char* str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '\"')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

// Parse CSV adaptado para Árvore AVL
void parseCSV(AVLTree* tree) {
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
        insertAVLTree(tree, d.UDI, d); // Insere na Árvore AVL usando UDI como chave
    }
    fclose(f);
}

// Função para exibir um item (mantida igual)
void displayItem(MachineData d) {
    printf("UDI: %d | ProductID: %s | Type: %c | AirTemp: %.1f | ProcessTemp: %.1f | RPM: %d | Torque: %.1f | ToolWear: %d | Failure: %d\n",
           d.UDI, d.ProductID, d.Type, d.AirTemp, d.ProcessTemp,
           d.RotationalSpeed, d.Torque, d.ToolWear, d.MachineFailure);
}

// Função para exibir todos os itens em ordem (usando percurso in-order)
void displayAllInOrder(AVLNode* node) {
    if (node != NULL) {
        displayAllInOrder(node->left);
        displayItem(node->data);
        displayAllInOrder(node->right);
    }
}

// Wrapper para displayAll
void displayAll(AVLTree* tree) {
    displayAllInOrder(tree->root);
}

void insertManualSample(AVLTree* tree) {
    MachineData newData;
    printf("\n=== INSERIR NOVA AMOSTRA MANUALMENTE ===\n");

    // Encontrar o próximo UDI disponível (maior UDI + 1)
    int maxUDI = 0;
    AVLNode* current = tree->root;
    while (current != NULL && current->right != NULL) {
        current = current->right;
    }
    if (current != NULL) {
        maxUDI = current->data.UDI;
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
                insertAVLTree(tree, newData.UDI, newData);
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

// Função para buscar por ProductID (percurso em ordem)
void searchByProductID(AVLNode* node, const char* pid) {
    if (node != NULL) {
        searchByProductID(node->left, pid);
        if (strcmp(node->data.ProductID, pid) == 0) {
            displayItem(node->data);
        }
        searchByProductID(node->right, pid);
    }
}

// Wrapper para searchByProductID
void searchByProductIDTree(AVLTree* tree, const char* pid) {
    searchByProductID(tree->root, pid);
}

// Função para buscar por Type (percurso em ordem)
void searchByType(AVLNode* node, char type) {
    if (node != NULL) {
        searchByType(node->left, type);
        if (toupper(node->data.Type) == toupper(type)) {
            displayItem(node->data);
        }
        searchByType(node->right, type);
    }
}

// Wrapper para searchByType
void searchByTypeTree(AVLTree* tree, char type) {
    searchByType(tree->root, type);
}

// Função para buscar por MachineFailure (percurso em ordem)
void searchByMachineFailure(AVLNode* node, bool f) {
    if (node != NULL) {
        searchByMachineFailure(node->left, f);
        if (node->data.MachineFailure == f) {
            displayItem(node->data);
        }
        searchByMachineFailure(node->right, f);
    }
}

// Wrapper para searchByMachineFailure
void searchByMachineFailureTree(AVLTree* tree, bool f) {
    searchByMachineFailure(tree->root, f);
}

// Função para remover por ProductID (busca linear)
bool removeByProductID(AVLTree* tree, const char* pid) {
    // Implementação simplificada: busca linear para encontrar o UDI correspondente
    // Pode ser ineficiente para grandes conjuntos de dados
    
    bool removed = false;
    AVLNode* current = tree->root;
    // Usaremos uma pilha para percurso in-order iterativo
    AVLNode* stack[1000]; // Tamanho arbitrário grande
    int top = -1;
    
    while (current != NULL || top != -1) {
        while (current != NULL) {
            stack[++top] = current;
            current = current->left;
        }
        
        current = stack[top--];
        
        if (strcmp(current->data.ProductID, pid) == 0) {
            int udi = current->data.UDI;
            current = current->right; // Avança antes de remover
            deleteAVLTree(tree, udi);
            removed = true;
        } else {
            current = current->right;
        }
    }
    
    return removed;
}

// Funções para estatísticas (adaptadas para AVL)
void calculateStatistics(AVLTree* tree) {
    if (tree->size == 0) {
        printf("Árvore vazia. Nenhum dado para análise.\n");
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

    // Percorre a árvore para calcular estatísticas
    AVLNode* stack[1000];
    int top = -1;
    AVLNode* current = tree->root;
    
    while (current != NULL || top != -1) {
        while (current != NULL) {
            stack[++top] = current;
            current = current->left;
        }
        
        current = stack[top--];
        
        // Processa o nó atual
        twSum += current->data.ToolWear;
        if (current->data.ToolWear > twMax) twMax = current->data.ToolWear;
        if (current->data.ToolWear < twMin) twMin = current->data.ToolWear;

        tqSum += current->data.Torque;
        if (current->data.Torque > tqMax) tqMax = current->data.Torque;
        if (current->data.Torque < tqMin) tqMin = current->data.Torque;

        rsSum += current->data.RotationalSpeed;
        if (current->data.RotationalSpeed > rsMax) rsMax = current->data.RotationalSpeed;
        if (current->data.RotationalSpeed < rsMin) rsMin = current->data.RotationalSpeed;

        float diff = current->data.ProcessTemp - current->data.AirTemp;
        tempDiffSum += diff;
        if (diff > tempDiffMax) tempDiffMax = diff;
        if (diff < tempDiffMin) tempDiffMin = diff;

        current = current->right;
    }

    // Calcula médias
    float twAvg = twSum / tree->size;
    float tqAvg = tqSum / tree->size;
    float rsAvg = (float)rsSum / tree->size;
    float tempDiffAvg = tempDiffSum / tree->size;

    // Segundo percurso para calcular desvios padrão
    current = tree->root;
    top = -1;
    
    while (current != NULL || top != -1) {
        while (current != NULL) {
            stack[++top] = current;
            current = current->left;
        }
        
        current = stack[top--];
        
        twSqDiffSum += pow(current->data.ToolWear - twAvg, 2);
        tqSqDiffSum += pow(current->data.Torque - tqAvg, 2);
        rsSqDiffSum += pow(current->data.RotationalSpeed - rsAvg, 2);
        float diff = current->data.ProcessTemp - current->data.AirTemp;
        tempDiffSqDiffSum += pow(diff - tempDiffAvg, 2);

        current = current->right;
    }

    float twStdDev = sqrt(twSqDiffSum / tree->size);
    float tqStdDev = sqrt(tqSqDiffSum / tree->size);
    float rsStdDev = sqrt(rsSqDiffSum / tree->size);
    float tempDiffStdDev = sqrt(tempDiffSqDiffSum / tree->size);

    printf("\n=== ESTATÍSTICAS DE OPERAÇÃO ===\n");
    printf("\nDesgaste da Ferramenta (ToolWear):\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           twAvg, twMax, twMin, twStdDev);
    printf("\nTorque (Nm):\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           tqAvg, tqMax, tqMin, tqStdDev);
    printf("\nVelocidade Rotacional (RPM):\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           rsAvg, rsMax, rsMin, rsStdDev);
    printf("\nDiferença de Temperatura (ProcessTemp - AirTemp):\nMédia=%.2f | Máximo=%.2f | Mínimo=%.2f | Desvio=%.2f\n",
           tempDiffAvg, tempDiffMax, tempDiffMin, tempDiffStdDev);
}

// Função para classificar falhas (adaptada para AVL)
void classifyFailures(AVLTree* tree) {
    if (tree->size == 0) {
        printf("Árvore vazia. Nenhum dado para análise.\n");
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

    // Percorre a árvore para coletar estatísticas
    AVLNode* stack[1000];
    int top = -1;
    AVLNode* current = tree->root;
    
    while (current != NULL || top != -1) {
        while (current != NULL) {
            stack[++top] = current;
            current = current->left;
        }
        
        current = stack[top--];
        
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

        current = current->right;
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

// Função para filtro avançado (adaptada para AVL)
void advancedFilter(AVLTree* tree) {
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
    
    // Percorre a árvore
    AVLNode* stack[1000];
    int top = -1;
    AVLNode* current = tree->root;
    
    while (current != NULL || top != -1) {
        while (current != NULL) {
            stack[++top] = current;
            current = current->left;
        }
        
        current = stack[top--];
        
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

        current = current->right;
    }

    printf("\nTotal de máquinas que atendem aos critérios: %d\n", matches);
}

// Função para exibir menu (mantida igual)
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
    printf("11. Executar Restrições\n");
    printf("12. Aprender Padrões de Falha\n");        // NOVA OPÇÃO
    printf("13. Simular Fresadora e Detectar Falhas\n"); // NOVA OPÇÃO
    printf("14. Sair\n");                               // Opção de saída atualizada
    printf("Escolha: ");
}

// Funções de benchmark adaptadas para AVL
// Timer de alta precisão
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER frequency;
} HighPrecisionTimer;

// Funções do timer de alta precisão
void start_timer(HighPrecisionTimer* timer) {
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->start);
}

double stop_timer(HighPrecisionTimer* timer) {
    QueryPerformanceCounter(&timer->end);
    double elapsed = (double)(timer->end.QuadPart - timer->start.QuadPart) * 1000.0 / timer->frequency.QuadPart;
    return elapsed;
}

void generateRandomData(AVLTree* tree, int count) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < count; i++) {
        MachineData d = {0};
        d.UDI = 10000 + i + rand() % 100000; // Garantir UDI único para AVL
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
        insertAVLTree(tree, d.UDI, d);
    }
}

void benchmark_insertion(AVLTree* tree, int num_elements) {
    HighPrecisionTimer t;
    start_timer(&t);
    AVLTree tmp;
    initAVLTree(&tmp);
    generateRandomData(&tmp, num_elements);
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Inserção (%d elementos): %.3f ms (%.1f elem/ms)\n",
           num_elements, elapsed, num_elements / elapsed);
    freeAVLTree(tmp.root);
}

void benchmark_search(AVLTree* tree) {
    if (tree->size == 0) {
        printf("Árvore vazia para busca\n");
        return;
    }
    HighPrecisionTimer t;
    const int searches = 10000;
    int found = 0;
    
    // Encontra o menor e maior UDI para o intervalo de busca
    AVLNode* minNode = tree->root;
    while (minNode != NULL && minNode->left != NULL) {
        minNode = minNode->left;
    }
    AVLNode* maxNode = tree->root;
    while (maxNode != NULL && maxNode->right != NULL) {
        maxNode = maxNode->right;
    }
    
    int minUDI = minNode ? minNode->key : 0;
    int maxUDI = maxNode ? maxNode->key : 0;
    
    start_timer(&t);
    for (int i = 0; i < searches; i++) {
        int search_udi = minUDI + (rand() % (maxUDI - minUDI + 1));
        if (searchAVLTree(tree, search_udi) != NULL) {
            found++;
        }
    }
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Busca (%d ops): encontrados=%d | tempo=%.3f ms (%.1f ops/ms)\n",
           searches, found, elapsed, searches / elapsed);
}

void benchmark_removal(AVLTree* tree) {
    if (tree->size == 0) {
        printf("Árvore vazia para remoção\n");
        return;
    }
    AVLTree tmp;
    initAVLTree(&tmp);
    
    // Copia os elementos para uma árvore temporária
    AVLNode* stack[1000];
    int top = -1;
    AVLNode* current = tree->root;
    
    while (current != NULL || top != -1) {
        while (current != NULL) {
            stack[++top] = current;
            current = current->left;
        }
        
        current = stack[top--];
        insertAVLTree(&tmp, current->key, current->data);
        current = current->right;
    }
    
    HighPrecisionTimer t;
    const int removals = 1000;
    start_timer(&t);
    
    // Encontra o menor e maior UDI para o intervalo de remoção
    AVLNode* minNode = tmp.root;
    while (minNode != NULL && minNode->left != NULL) {
        minNode = minNode->left;
    }
    AVLNode* maxNode = tmp.root;
    while (maxNode != NULL && maxNode->right != NULL) {
        maxNode = maxNode->right;
    }
    
    int minUDI = minNode ? minNode->key : 0;
    int maxUDI = maxNode ? maxNode->key : 0;
    
    for (int i = 0; i < removals; i++) {
        if (tmp.size > 0) {
            int remove_udi = minUDI + (rand() % (maxUDI - minUDI + 1));
            deleteAVLTree(&tmp, remove_udi);
        }
    }
    
    double elapsed = stop_timer(&t);
    printf("\nBenchmark Remoção (%d ops): %.3f ms (%.1f ops/ms)\n",
           removals, elapsed, removals / elapsed);
    freeAVLTree(tmp.root);
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
    size += sizeof(AVLNode*) * 2; // left e right
    size += sizeof(int); // height

    return size;
}

void estimate_memory_usage(AVLTree* tree) {
    if (tree == NULL) {
        printf("Árvore inválida\n");
        return;
    }

    size_t node_size = calculate_node_size();
    size_t total_memory = tree->size * node_size + sizeof(AVLTree);
    
    printf("\n=== ESTIMATIVA DE USO DE MEMÓRIA ===\n");
    printf("Tamanho por nó: %zu bytes\n", node_size);
    printf("Número de nós: %d\n", tree->size);
    printf("Memória total estimada: %zu bytes (%.2f KB)\n",
           total_memory, (float)total_memory / 1024);

    printf("\nComparação com sizeof:\n");
    printf("sizeof(MachineData): %zu bytes\n", sizeof(MachineData));
    printf("sizeof(AVLNode): %zu bytes\n", sizeof(AVLNode));
    printf("Obs: Pode haver padding/alignment pelo compilador\n");
}

void benchmark_random_access(AVLTree* tree) {
    if (tree->size == 0) {
        printf("Árvore vazia para teste de acesso aleatório\n");
        return;
    }

    const int accesses = 10000;
    HighPrecisionTimer t;
    int sum = 0;

    // Encontra o menor e maior UDI para o intervalo de acesso
    AVLNode* minNode = tree->root;
    while (minNode != NULL && minNode->left != NULL) {
        minNode = minNode->left;
    }
    AVLNode* maxNode = tree->root;
    while (maxNode != NULL && maxNode->right != NULL) {
        maxNode = maxNode->right;
    }
    
    int minUDI = minNode ? minNode->key : 0;
    int maxUDI = maxNode ? maxNode->key : 0;

    start_timer(&t);
    for (int i = 0; i < accesses; i++) {
        int random_udi = minUDI + (rand() % (maxUDI - minUDI + 1));
        AVLNode* found = searchAVLTree(tree, random_udi);
        if (found != NULL) {
            sum += found->data.UDI;
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
        AVLTree tree;
        initAVLTree(&tree);

        HighPrecisionTimer t;
        start_timer(&t);
        generateRandomData(&tree, sizes[i]);
        double elapsed = stop_timer(&t);

        printf("Tamanho: %6d elementos | Tempo de inserção: %7.3f ms | Tempo por elemento: %.5f ms\n",
               sizes[i], elapsed, elapsed / sizes[i]);

        freeAVLTree(tree.root);
    }
}

void benchmark_combined_operations(AVLTree* tree) {
    const int num_operations = 1000;
    HighPrecisionTimer t;
    start_timer(&t);

    // Encontra o menor e maior UDI para o intervalo de operações
    AVLNode* minNode = tree->root;
    while (minNode != NULL && minNode->left != NULL) {
        minNode = minNode->left;
    }
    AVLNode* maxNode = tree->root;
    while (maxNode != NULL && maxNode->right != NULL) {
        maxNode = maxNode->right;
    }
    
    int minUDI = minNode ? minNode->key : 0;
    int maxUDI = maxNode ? maxNode->key : 0;

    for (int i = 0; i < num_operations; i++) {
        if (rand() % 100 < 30) {
            // Inserção
            MachineData d = {0};
            d.UDI = maxUDI + 1 + i; // Novo UDI único
            snprintf(d.ProductID, sizeof(d.ProductID), "M%07d", rand() % 1000000);
            insertAVLTree(tree, d.UDI, d);
            maxUDI = d.UDI; // Atualiza o máximo
        } else if (rand() % 100 < 80) {
            // Busca
            int search_udi = minUDI + (rand() % (maxUDI - minUDI + 1));
            searchAVLTree(tree, search_udi);
        } else {
            // Remoção (apenas se houver elementos)
            if (tree->size > 0) {
                int remove_udi = minUDI + (rand() % (maxUDI - minUDI + 1));
                deleteAVLTree(tree, remove_udi);
            }
        }
    }

    double elapsed = stop_timer(&t);
    printf("Benchmark Operações Combinadas (%d ops): %.3f ms | Latência média: %.3f ms/op\n",
           num_operations, elapsed, elapsed / num_operations);
}

void run_all_benchmarks(AVLTree* tree) {
    printf("\n=== INICIANDO BENCHMARKS COMPLETOS ===\n");

    printf("\n1. Tempo de Inserção:\n");
    benchmark_insertion(tree, 1000);
    benchmark_insertion(tree, 10000);

    printf("\n2. Tempo de Remoção:\n");
    benchmark_removal(tree);

    printf("\n3. Tempo de Busca:\n");
    benchmark_search(tree);

    printf("\n4. Uso de Memória:\n");
    estimate_memory_usage(tree);

    printf("\n5. Tempo Médio de Acesso:\n");
    benchmark_random_access(tree);

    printf("\n6. Escalabilidade:\n");
    benchmark_scalability();

    printf("\n7. Latência Média (operações combinadas):\n");
    benchmark_combined_operations(tree);

    printf("\n=== BENCHMARKS CONCLUÍDOS ===\n");
}

// Função para remover o primeiro elemento (para restrição R2)
void removeFirst(AVLTree* tree) {
    if (tree == NULL || tree->root == NULL) return;
    
    // Encontra o nó com o menor UDI (mais à esquerda)
    AVLNode* current = tree->root;
    while (current->left != NULL) {
        current = current->left;
    }
    
    deleteAVLTree(tree, current->key);
}

void generateAnomalousData(AVLTree* tree, int count, int max_size) {
    for (int i = 0; i < count; i++) {
        if (max_size > 0 && tree->size >= max_size) {
            removeFirst(tree); // R2: Limitação de tamanho
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

        insertAVLTree(tree, d.UDI, d); // Insere na Árvore AVL

        if (i > 0 && i % 100 == 0) {
            Sleep(50); // R13: Delay artificial por lote
        }
    }
}

void run_restricted_benchmarks() {
    printf("\n=== BENCHMARK COM RESTRIÇÕES ATIVADAS ===\n");

    AVLTree tree;
    initAVLTree(&tree);

    HighPrecisionTimer t;
    start_timer(&t);

    // R2, R10, R13, R18
    generateAnomalousData(&tree, 1000, 500);

    double elapsed = stop_timer(&t);

    printf("\nTempo total (com 4 restrições aplicadas): %.3f ms\n", elapsed);
    printf("Elementos finais na árvore (máximo 500): %d\n", tree.size);

    // Benchmarks após restrições
    benchmark_search(&tree);
    benchmark_removal(&tree);
    benchmark_random_access(&tree);
    estimate_memory_usage(&tree);

    freeAVLTree(tree.root);

    printf("\n=== FIM DOS TESTES COM RESTRIÇÕES ===\n");
}

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

// Função auxiliar para percorrer a AVL e aprender padrões
void learnFailurePatternsRecursive(AVLNode* node, FailurePatternList* patterns) {
    if (node == NULL) {
        return;
    }

    learnFailurePatternsRecursive(node->left, patterns); // Percorre a subárvore esquerda

    if (node->data.MachineFailure) { // Se houver falha na máquina
        FailurePattern fp;
        // Para este exemplo, armazena os valores exatos da instância de falha como um padrão.
        // Um sistema mais robusto poderia calcular faixas, médias, etc.
        fp.minAirTemp = fp.maxAirTemp = node->data.AirTemp;
        fp.minProcessTemp = fp.maxProcessTemp = node->data.ProcessTemp;
        fp.minRotationalSpeed = fp.maxRotationalSpeed = node->data.RotationalSpeed;
        fp.minTorque = fp.maxTorque = node->data.Torque;
        fp.minToolWear = fp.maxToolWear = node->data.ToolWear;
        
        fp.hadTWF = node->data.TWF;
        fp.hadHDF = node->data.HDF;
        fp.hadPWF = node->data.PWF;
        fp.hadOSF = node->data.OSF;
        fp.hadRNF = node->data.RNF;

        addFailurePattern(patterns, fp);
    }

    learnFailurePatternsRecursive(node->right, patterns); // Percorre a subárvore direita
}

// APRENDE OS PADRÕES DE FALHA A PARTIR DOS DADOS EXISTENTES NA ÁRVORE AVL
// Opção 12 do menu.
void learnFailurePatterns(AVLTree* tree, FailurePatternList* patterns) {
    if (tree->size == 0) {
        printf("Árvore AVL vazia. Nenhuma falha para aprender.\n");
        return;
    }

    printf("\n=== APRENDENDO PADRÕES DE FALHA ===\n");
    // Reinicia os padrões antes de aprender novos
    freeFailurePatternList(patterns);
    initFailurePatternList(patterns);

    learnFailurePatternsRecursive(tree->root, patterns);
    printf("Aprendidos %d padrões de falha a partir dos dados existentes.\n", patterns->count);
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

// --- FUNÇÃO DE SIMULAÇÃO DA FRESADORA ---

// SIMULA A FRESADORA E DETECTA PADRÕES DE FALHA
// Gera dados de MachineData simulados.
// Periodicamente, injeta um padrão de falha aprendido para demonstrar a detecção.
// Opção 13 do menu.
void simulateMillingMachine(AVLTree* tree, FailurePatternList* patterns, int num_simulations) {
    if (patterns->count == 0) {
        printf("Nenhum padrão de falha aprendido. Por favor, aprenda os padrões primeiro (Opção 12).\n");
        return;
    }

    printf("\n=== SIMULANDO FRESADORA E DETECTANDO FALHAS ===\n");
    srand((unsigned)time(NULL)); // Inicializa o gerador de números aleatórios
    int failure_alerts = 0;
    int next_udi = 0;

    // Encontra o UDI máximo atual para continuar a partir dele
    // (Reutilizando a lógica de insertManualSample para encontrar o maior UDI)
    AVLNode* current_max_udi_node = tree->root;
    while (current_max_udi_node != NULL && current_max_udi_node->right != NULL) {
        current_max_udi_node = current_max_udi_node->right;
    }
    if (current_max_udi_node != NULL) {
        next_udi = current_max_udi_node->data.UDI;
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

        // Adiciona os dados simulados à árvore AVL
        insertAVLTree(tree, simulatedData.UDI, simulatedData);
    }
    printf("\nSimulação concluída. Total de alertas de falha: %d\n", failure_alerts);
}

int main() {
    AVLTree tree;
    initAVLTree(&tree);
    parseCSV(&tree); // Carrega os dados iniciais do CSV

    // ADICIONE ESTAS DUAS LINHAS:
    FailurePatternList failurePatterns; // Declara a lista de padrões de falha
    initFailurePatternList(&failurePatterns); // Inicializa a lista

    int choice;
    char input[64]; // Buffer para ler entradas de texto

    do {
        // --- SEU displayMenu() ATUALIZADO AQUI ---
        // (Já foi fornecido acima na seção 3)
        displayMenu();
        if (!fgets(input, sizeof(input), stdin)) { // Lê a linha completa
            break; // Em caso de erro na leitura
        }
        choice = atoi(input); // Converte a string para inteiro

        switch (choice) {
            case 1:
                displayAll(&tree);
                break;
            case 2: {
                printf("Digite o ProductID para buscar: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0'; // Remove o newline
                    searchByProductIDTree(&tree, input); // Função adaptada para AVL
                }
                break;
            }
            case 3: {
                printf("Digite o Tipo para buscar (L, M, H): ");
                if (fgets(input, sizeof(input), stdin)) {
                    searchByTypeTree(&tree, toupper(input[0])); // Função adaptada para AVL
                }
                break;
            }
            case 4: {
                printf("Digite 1 para buscar falhas, 0 para buscar não falhas: ");
                int failure;
                if (scanf("%d", &failure)) {
                    while (getchar() != '\n'); // Limpa o buffer
                    searchByMachineFailureTree(&tree, failure); // Função adaptada para AVL
                }
                break;
            }
            case 5:
    			insertManualSample(&tree);
    			break;
            case 6: {
                printf("Digite o ProductID para remover: ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = '\0';
                    if (removeByProductID(&tree, input)) // Função adaptada para AVL
                        printf("Item(s) removido(s) com sucesso.\n");
                    else
                        printf("Nenhum item encontrado com o ProductID: %s\n", input);
                }
                break;
            }
            case 7:
                calculateStatistics(&tree); // Função adaptada para AVL
                break;
            case 8:
                classifyFailures(&tree); // Função adaptada para AVL
                break;
            case 9:
                advancedFilter(&tree); // Função adaptada para AVL
                break;
            case 10:
                run_all_benchmarks(&tree); // Função adaptada para AVL
                break;
            case 11:
            	run_restricted_benchmarks(); // Função adaptada para AVL
				break;
            // ADICIONE ESTES NOVOS CASES:
            case 12: // Nova opção: Aprender Padrões de Falha
                learnFailurePatterns(&tree, &failurePatterns);
                break;
            case 13: { // Nova opção: Simular Fresadora e Detectar Falhas
                printf("Quantas simulações deseja executar? ");
                int num_sims;
                if (scanf("%d", &num_sims) == 1) {
                    while (getchar() != '\n'); // Limpa o buffer
                    simulateMillingMachine(&tree, &failurePatterns, num_sims);
                } else {
                    printf("Entrada inválida. Por favor, digite um número.\n");
                    while (getchar() != '\n'); // Limpa o buffer
                }
                break;
            }
            // FIM DOS NOVOS CASES

            case 14: // Opção de saída atualizada
                printf("Saindo...\n");
                break;
            default:
                printf("Opção inválida. Tente novamente.\n");
        }
    } while (choice != 14); // Condição de saída atualizada

    freeAVLTree(tree.root); // Libera a árvore AVL
    // ADICIONE ESTA LINHA:
    freeFailurePatternList(&failurePatterns); // Libera a memória da lista de padrões
    return 0;
}