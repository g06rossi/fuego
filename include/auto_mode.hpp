//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef AUTO_MODE_H
#define AUTO_MODE_H

#include <stdarg.h>

#include <defines.hpp>                        // Definicoes globais
#include <functions.hpp>                      // Funcoes auxiliares
#include <boot_mode.hpp>                      // NVS e senha de boot por IR
#include <sensor_task.hpp>                    // Funcoes de sensoreamento
#include <openings.hpp>                       // Estrategias iniciais do modo AUTO

char numBuf[8];                               // Digitos do indice recebido por BT
int numLen                     = 0;           // Quantidade de digitos no buffer
unsigned long ultimoDigitoMs   = 0;           // Instante do ultimo digito recebido

#pragma endregion

//===============================================================================================//
//=====================================//LOG E TABELAS BT//======================================//
//===============================================================================================//

#pragma region TABELAS BT

// Imprime (printf) no monitor serial e no terminal Bluetooth
void logBT(const char* formato, ...) {
    char texto[192];
    va_list args;
    va_start(args, formato);
    vsnprintf(texto, sizeof(texto), formato, args);
    va_end(args);
    Serial.print(texto);
    SerialBT.print(texto);
}

const char* nomeModoBT() {
    switch (modoBT) {
        case MODO_TESTE_SENSOR:  return "Teste de sensor";
        case MODO_TESTE_MOTOR:   return "Teste de motor";
        case MODO_PERSONALIZADA: return "Estrategia personalizada";
        default:                 return "Luta";
    }
}

const char* nomeMovimento() {
    switch (modoLuta) {
        case buscaOfensiva:  return "Busca Ofensiva";
        case buscaDefensiva: return "Busca Defensiva";
        case buscaLinha:     return "Busca Linha";
        case buscaPulsada:   return "Busca Pulsada";
        default:             return "Retorno";
    }
}

// Linha do log de selecao: [OK] se ja foi selecionado, [  ] se ainda falta
void linhaLogSelecao(const char* nome, bool feito, const char* valor) {
    logBT("  %s %-12s: %s\n", feito ? "[OK]" : "[  ]", nome, feito ? valor : "---");
}

// Indica o que ja foi selecionado e o que ainda falta
void printLogSelecao() {
    bool luta = (modoBT == MODO_LUTA || modoBT == MODO_PERSONALIZADA);
    char valor[40];

    logBT("\n//=====//SELECAO ATUAL//=====//\n");
    linhaLogSelecao("Modo", currentStage > STAGE_MODE, nomeModoBT());

    if (modoBT == MODO_PERSONALIZADA) {
        snprintf(valor, sizeof(valor), "%d passo(s)", customOpeningCount);
        linhaLogSelecao("Personalizada", currentStage > STAGE_CUSTOM_OPENING, valor);
    }

    if (luta || currentStage == STAGE_MODE) {
        linhaLogSelecao("Furtivo", currentStage > STAGE_FURTIVE, modoFurtivo ? "Ativado" : "Desativado");
        linhaLogSelecao("Direcao", currentStage > STAGE_DIRECTION, direction == direita ? "Direita" : "Esquerda");

        if (modoBT == MODO_PERSONALIZADA) {
            linhaLogSelecao("Iniciacao", true, "Personalizada");
        } else {
            linhaLogSelecao("Iniciacao", currentStage > STAGE_INITIATION,
                            macroIndex == 0 ? "Nenhuma (iterativo puro)" : NOMES_MACROS[macroIndex - 1]);
        }

        linhaLogSelecao("Movimentacao", currentStage > STAGE_MOVEMENT, nomeMovimento());
        linhaLogSelecao("Finalizacao", currentStage > STAGE_FINALIZATION,
                        finalization == FIM_LDR ? "LDR" : "Tempo (4s)");
    } else {
        logBT("  (demais etapas nao se aplicam a este modo)\n");
    }
    logBT("\n");
}

// Imprime a tabela de estrategias disponiveis na etapa atual
void printTabelaEtapa() {
    switch (currentStage) {
        case STAGE_MODE:
            logBT("//=====//ETAPA: MODO//=====//\n"
                  "Indice | Modo\n"
                  "-------|---------------------------------------\n"
                  "  0    | Luta\n"
                  "  1    | Teste de sensor\n"
                  "  2    | Teste de motor\n"
                  "  3    | Estrategia personalizada\n");
            break;

        case STAGE_CUSTOM_OPENING:
            logBT("//=====//ETAPA: ESTRATEGIA PERSONALIZADA//=====//\n"
                  "Envie um passo por linha: vel_esq,vel_dir,delay (max. %d passos)\n"
                  "Envie '.' para finalizar a estrategia\n", MAX_STEPS - 1);
            break;

        case STAGE_FURTIVE:
            logBT("//=====//ETAPA: MODO FURTIVO//=====//\n"
                  "Indice | Modo furtivo\n"
                  "-------|---------------------------------------\n"
                  "  0    | Desativado (JSumos ligados)\n"
                  "  1    | Ativado (JSumos desligados no inicio)\n");
            break;

        case STAGE_DIRECTION:
            logBT("//=====//ETAPA: DIRECAO//=====//\n"
                  "Indice | Direcao\n"
                  "-------|---------------------------------------\n"
                  "  0    | Esquerda\n"
                  "  1    | Direita\n");
            break;

        case STAGE_INITIATION:
            logBT("//=====//ETAPA: INICIACAO//=====//\n"
                  "Indice | Macro\n"
                  "-------|---------------------------------------\n"
                  "  0    | Nenhuma (iterativo puro)\n");
            for (int i = 0; i < NUM_MACROS; i++) logBT(" %2d    | %s\n", i + 1, NOMES_MACROS[i]);
            break;

        case STAGE_MOVEMENT:
            logBT("//=====//ETAPA: MOVIMENTACAO//=====//\n"
                  "Indice | Movimentacao iterativa\n"
                  "-------|---------------------------------------\n"
                  "  0    | Busca Ofensiva\n"
                  "  1    | Busca Defensiva\n"
                  "  2    | Busca Linha\n"
                  "  3    | Busca Pulsada\n");
            break;

        case STAGE_FINALIZATION:
            logBT("//=====//ETAPA: FINALIZACAO (ultima)//=====//\n"
                  "Indice | Finalizacao da estrategia\n"
                  "-------|---------------------------------------\n"
                  "  0    | Por LDR (ve o adversario)\n"
                  "  1    | Por tempo (%d ms)\n", FINAL_TEMPO_MS);
            break;

        default:
            break;
    }
    logBT("-------|---------------------------------------\n"
          "'?' reimprime | 'R' reinicia o ESP\n\n");
}

#pragma endregion

//===============================================================================================//
//====================================//INTERPRETA BLUETOOTH//===================================//
//===============================================================================================//

#pragma region INTERPRETA BT

// Avanca para a proxima etapa e imprime o log e a tabela dela
void avancarEtapa() {
    switch (currentStage) {
        case STAGE_MODE:
            if (modoBT == MODO_LUTA)               currentStage = STAGE_FURTIVE;
            else if (modoBT == MODO_PERSONALIZADA) currentStage = STAGE_CUSTOM_OPENING;
            else                                   currentStage = STAGE_DONE;   // Testes
            break;
        case STAGE_CUSTOM_OPENING:  currentStage = STAGE_FURTIVE;      break;
        case STAGE_FURTIVE:       currentStage = STAGE_DIRECTION;    break;
        case STAGE_DIRECTION:
            // Estrategia personalizada substitui a macro de iniciacao
            currentStage = (modoBT == MODO_PERSONALIZADA) ? STAGE_MOVEMENT : STAGE_INITIATION;
            break;
        case STAGE_INITIATION:    currentStage = STAGE_MOVEMENT;     break;
        case STAGE_MOVEMENT:      currentStage = STAGE_FINALIZATION; break;
        default:                  currentStage = STAGE_DONE;         break;
    }

    printLogSelecao();
    if (currentStage == STAGE_DONE) logBT("//=====//SELECAO CONCLUIDA//=====//\n");
    else printTabelaEtapa();
}

// Aplica o indice recebido na etapa atual. Indice invalido reimprime a tabela
void aplicarSelecao(int indice) {
    bool valido = true;

    switch (currentStage) {
        case STAGE_MODE:
            if (indice >= MODO_LUTA && indice <= MODO_PERSONALIZADA) {
                modoBT = (ModoBT)indice;
                if (modoBT == MODO_PERSONALIZADA) {      // Reinicia o buffer personalizado
                    customOpeningCount = 0;
                    bufferIndex = 0;
                    customOpening[0] = {0, 0, 0};
                }
            } else valido = false;
            break;

        case STAGE_FURTIVE:
            if (indice == 0 || indice == 1) modoFurtivo = (indice == 1);
            else valido = false;
            break;

        case STAGE_DIRECTION:
            if (indice == 0 || indice == 1) direction = (indice == 1) ? direita : esquerda;
            else valido = false;
            break;

        case STAGE_INITIATION:
            if (indice >= 0 && indice <= NUM_MACROS) macroIndex = indice;
            else valido = false;
            break;

        case STAGE_MOVEMENT:
            if (indice >= buscaOfensiva && indice <= buscaPulsada) {
                modoLuta = (ModoLuta)indice;
                modoLutaOriginal = modoLuta; // Guarda o modo original
            }
            else valido = false;
            break;

        case STAGE_FINALIZATION:
            if (indice == 0 || indice == 1) finalization = (indice == 1) ? FIM_TEMPO : FIM_LDR;
            else valido = false;
            break;

        default:
            valido = false;
            break;
    }

    if (!valido) {
        logBT("Indice %d invalido para esta etapa!\n", indice);
        printTabelaEtapa();
        return;
    }
    avancarEtapa();
}

// Valida o indice acumulado no buffer numerico
void confirmarIndice() {
    numBuf[numLen] = '\0';
    numLen = 0;
    aplicarSelecao(atoi(numBuf));
}

// Etapa de selecao por indice: acumula digitos e valida com ENTER ou por tempo
void processarIndice(char recebido) {
    if (recebido >= '0' && recebido <= '9') {
        if (numLen < (int)sizeof(numBuf) - 1) numBuf[numLen++] = recebido;
        ultimoDigitoMs = millis();

    } else if (recebido == '\n' || recebido == '\r') {
        if (numLen > 0) confirmarIndice();

    } else if (recebido == '?') {
        numLen = 0;
        printLogSelecao();
        printTabelaEtapa();

    } else if (recebido == 'R' || recebido == 'r') {
        logBT("//=====//REINICIANDO ESP//=====//\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    }
}

// Etapa da estrategia personalizada: um passo (vel_esq,vel_dir,delay) por linha, '.' finaliza
void processarPersonalizada(char recebido) {
    if (recebido == '\n') {
        btBuffer[bufferIndex] = '\0';
        if (bufferIndex == 0) return;         // Linha vazia (ex.: CR+LF)

        if (strcmp(btBuffer, ".") == 0) {
            // Passo final da estrategia: (0, 0, 0)
            // Estrutura necessaria para o processamento da sequencia
            customOpening[customOpeningCount] = {0, 0, 0};
            bufferIndex = 0;
            logBT("Estrategia personalizada finalizada (%d passos).\n", customOpeningCount);
            avancarEtapa();
            return;
        }
        parseAndStoreStep(btBuffer);
        bufferIndex = 0;                      // Limpa o buffer

    } else if (recebido >= 32) {
        if (bufferIndex < BT_BUFFER_SIZE - 1) {
            btBuffer[bufferIndex++] = recebido;
        } else {
            Serial.println("Erro: Buffer cheio!");
            bufferIndex = 0;
        }
    }
}

// Percorre as etapas ate a ultima ser selecionada
void translateBT() {
    numLen = 0;

    while (currentStage != STAGE_DONE) {
        char receivedChar;

        bootSenhaProcessar();                 // Janela da senha de boot por IR
        // Atualiza os LEDs com o status dos sensores (a senha em curso toma os LEDs)
        if (bootSenhaProgresso == 0 && !bootSenhaFechada) {
            indicarSensores(value_IR_E, value_LDR, value_IR_D);
        }

        if (xQueueReceive(btQueue, &receivedChar, pdMS_TO_TICKS(10))) {
            if (currentStage == STAGE_CUSTOM_OPENING) processarPersonalizada(receivedChar);
            else processarIndice(receivedChar);

        // Terminais sem ENTER: valida o indice apos um tempo sem novos digitos
        } else if (numLen > 0 && millis() - ultimoDigitoMs >= BT_NUM_TIMEOUT_MS) {
            confirmarIndice();
        }
        vTaskDelay(pdMS_TO_TICKS(1));         // Delay para o FreeRTOS
    }
}

#pragma endregion

//===============================================================================================//
//===================================//ESCOLHA DA ESTRATEGIA//===================================//
//===============================================================================================//

#pragma region ESCOLHA ESTRATEGIA

void modoAUTO() {
    setupOpeningsTask();                         // Aloca a funcao da estrategia de luta

    // Cria a fila BT para armazenar as informacoes (evita perdas na comunicacao)
    btQueue = xQueueCreate(BT_QUEUE_LENGTH, sizeof(char));

    Serial.println("//=====//Setup AUTO feita//=====//");
    SerialBT.register_callback(bt_callback);  // Funcao de callback para os dados

    vTaskDelay(pdMS_TO_TICKS(500));           // Pequeno atraso

    SerialBT.begin("Fuego Wu");                 // !INFO | SMOKER, O MAIOR!
    Serial.printf("Bluetooth iniciado. Tentando conectar");
    ledsModo(BOOT_AUTO);                      // LEDs na cor do modo AUTO
    vTaskDelay(pdMS_TO_TICKS(500));           // Pequeno atraso

    // Espera a conexao BT para continuar
    while (!SerialBT.hasClient()) {
        Serial.printf(".");
        bootSenhaProcessar();                 // Janela da senha de boot por IR
        vTaskDelay(pdMS_TO_TICKS(100));       // Espera em pequenos incrementos
    }
    Serial.println();
    Serial.println("Cliente Bluetooth conectado! Iniciando comunicacao");
    SerialBT.println("//=====//Bluetooth conectado!//=====//");

    currentStage = STAGE_MODE;
    printLogSelecao();
    printTabelaEtapa();                       // Tabela da primeira etapa (automatica)

    translateBT();                            // Percorre as etapas selecionadas por BT
    bootSenhaCancelar();                      // Fecha a janela da senha de boot

    // Testes de bancada comecam assim que selecionados e nao voltam
    if (modoBT == MODO_TESTE_SENSOR) {
        logBT("//=====//TESTE SENSOR INICIADO//=====//\n");
        testSensors();
    } else if (modoBT == MODO_TESTE_MOTOR) {
        logBT("//=====//TESTE MOTOR INICIADO//=====//\n");
        testMotors();
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    inicializado = true;                      // Indica que a configuracao acabou e o loop reinicia
    ready = false;
    ultimoComandoIR = 0xFFFF;                 // Descarta IR recebido durante a selecao
    xTaskNotifyGive(IRCommandHandle);         // Notifica a tarefa de interpretacao do IR
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif
