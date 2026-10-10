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

// Imprime (printf) no monitor serial e no terminal Bluetooth. Textos maiores que o buffer local
// (como as tabelas das etapas) ganham um buffer do tamanho exato, para nunca serem cortados
void logBT(const char* formato, ...) {
    char texto[192];
    va_list args;
    va_start(args, formato);
    int tamanho = vsnprintf(texto, sizeof(texto), formato, args);
    va_end(args);
    if (tamanho < 0) return;

    char* saida = texto;
    if (tamanho >= (int)sizeof(texto)) {
        saida = (char*)malloc(tamanho + 1);
        if (saida == nullptr) return;
        va_start(args, formato);
        vsnprintf(saida, tamanho + 1, formato, args);
        va_end(args);
    }

    Serial.print(saida);
    SerialBT.print(saida);
    if (saida != texto) free(saida);
}

const char* nomeModoBT() {
    switch (modoBT) {
        case MODO_TESTE_SENSOR:  return "Teste de sensor";
        case MODO_TESTE_MOTOR:   return "Teste de motor";
        case MODO_PERSONALIZADA: return "Estrategia personalizada";
        case MODO_CONFIGURACAO:  return "Configuracao (pinos e parametros)";
        case MODO_MACROS:        return "Macros (ver valores das iniciacoes)";
        default:                 return "Luta";
    }
}

const char* nomeAdversario() {
    switch (tipoAdversario) {
        case advRampaSemIR:    return "Rampa Simples sem IR";
        case advAsaSemEmissor: return "Asa Sem Emissor";
        case advAsaComEmissor: return "Asa Com Emissor";
        default:               return "Rampa Simples com IR";
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

    if (luta || currentStage == STAGE_MODE) {
        linhaLogSelecao("Adversario", currentStage > STAGE_ADVERSARY, nomeAdversario());
        linhaLogSelecao("Asa", currentStage > STAGE_WING, abrirAsa ? "Aberta" : "Fechada");

        if (modoBT == MODO_PERSONALIZADA) {
            snprintf(valor, sizeof(valor), "Personalizada (%d passo(s))", customOpeningCount);
            linhaLogSelecao("Iniciacao", currentStage > STAGE_CUSTOM_OPENING, valor);
        } else {
            linhaLogSelecao("Iniciacao", currentStage > STAGE_INITIATION,
                            macroIndex == 0 ? "Nenhuma (iterativo puro)" : NOMES_MACROS[macroIndex - 1]);
        }

        linhaLogSelecao("Direcao", currentStage > STAGE_DIRECTION, direction == direita ? "Direita" : "Esquerda");
        linhaLogSelecao("Movimentacao", currentStage > STAGE_MOVEMENT, nomeMovimento());
        linhaLogSelecao("Furtivo Mov.", currentStage > STAGE_FURTIVE_MOVEMENT, furtivoMovimentacao ? "Ativado" : "Desativado");
        linhaLogSelecao("Finalizacao", currentStage > STAGE_FINALIZATION,
                        finalization == FIM_LDR ? "LDR" : "Tempo");
        if (finalization == FIM_TEMPO && currentStage > STAGE_FINALIZATION) {
            snprintf(valor, sizeof(valor), "%lu ms", tempoFinalizacaoMs);
            linhaLogSelecao("Tempo final", currentStage > STAGE_FINAL_TIME, valor);
        }
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
                  "  3    | Estrategia personalizada\n"
                  "  4    | Configuracao (pinos e parametros)\n"
                  "  5    | Macros (ver valores das iniciacoes)\n");
            break;

        case STAGE_ADVERSARY:
            logBT("//=====//ETAPA: ADVERSARIO//=====//\n"
                  "Indice | Tipo de adversario\n"
                  "-------|---------------------------------------\n"
                  "  0    | Rampa Simples com IR\n"
                  "  1    | Rampa Simples sem IR\n"
                  "  2    | Asa Sem Emissor\n"
                  "  3    | Asa Com Emissor\n");
            break;

        case STAGE_WING:
            logBT("//=====//ETAPA: ASA//=====//\n"
                  "Indice | Asa durante a luta\n"
                  "-------|---------------------------------------\n"
                  "  0    | Fechada\n"
                  "  1    | Aberta (abre no inicio da iniciacao)\n");
            break;

        case STAGE_CUSTOM_OPENING:
            logBT("//=====//ETAPA: ESTRATEGIA PERSONALIZADA//=====//\n"
                  "Envie um passo por linha: vel_esq,vel_dir,delay (max. %d passos)\n"
                  "Envie '.' para finalizar a estrategia\n", MAX_STEPS - 1);
            break;

        case STAGE_FURTIVE_MOVEMENT:
            logBT("//=====//ETAPA: FURTIVO NA MOVIMENTACAO//=====//\n"
                  "Indice | Modo furtivo\n"
                  "-------|---------------------------------------\n"
                  "  0    | Desativado (JSumos ligados na movimentacao)\n"
                  "  1    | Ativado (JSumos desligados na movimentacao)\n");
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
            logBT("//=====//ETAPA: FINALIZACAO//=====//\n"
                  "Indice | Finalizacao da estrategia\n"
                  "-------|---------------------------------------\n"
                  "  0    | Por LDR (ve o adversario)\n"
                  "  1    | Por tempo (o tempo e pedido em seguida)\n");
            break;

        case STAGE_FINAL_TIME:
            logBT("//=====//ETAPA: TEMPO DA FINALIZACAO (ultima)//=====//\n"
                  "Envie o tempo em ms ate a troca para Busca Ofensiva\n"
                  "Sugerido: %d ms\n", FINAL_TEMPO_MS);
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
            if (modoBT == MODO_LUTA || modoBT == MODO_PERSONALIZADA) currentStage = STAGE_ADVERSARY;
            else currentStage = STAGE_DONE;   // Testes e Config
            break;
        case STAGE_ADVERSARY:        currentStage = STAGE_WING;             break;
        case STAGE_WING:
            // Estrategia personalizada substitui a macro de iniciacao
            currentStage = (modoBT == MODO_PERSONALIZADA) ? STAGE_CUSTOM_OPENING : STAGE_INITIATION;
            break;
        case STAGE_CUSTOM_OPENING:   currentStage = STAGE_DIRECTION;        break;
        case STAGE_INITIATION:       currentStage = STAGE_DIRECTION;        break;
        case STAGE_DIRECTION:        currentStage = STAGE_MOVEMENT;         break;
        case STAGE_MOVEMENT:         currentStage = STAGE_FURTIVE_MOVEMENT; break;
        case STAGE_FURTIVE_MOVEMENT: currentStage = STAGE_FINALIZATION;     break;
        case STAGE_FINALIZATION:
            currentStage = (finalization == FIM_TEMPO) ? STAGE_FINAL_TIME : STAGE_DONE;
            break;
        default:                     currentStage = STAGE_DONE;             break;
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
            if (indice >= MODO_LUTA && indice <= MODO_MACROS) {
                modoBT = (ModoBT)indice;
                if (modoBT == MODO_PERSONALIZADA) {      // Reinicia o buffer personalizado
                    customOpeningCount = 0;
                    bufferIndex = 0;
                    customOpening[0] = {0, 0, 0};
                }
            } else valido = false;
            break;

        case STAGE_ADVERSARY:
            if (indice >= advRampaComIR && indice <= advAsaComEmissor) tipoAdversario = (TipoAdversario)indice;
            else valido = false;
            break;

        case STAGE_WING:
            if (indice == 0 || indice == 1) abrirAsa = (indice == 1);
            else valido = false;
            break;

        case STAGE_FURTIVE_MOVEMENT:
            if (indice == 0 || indice == 1) furtivoMovimentacao = (indice == 1);
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

        case STAGE_FINAL_TIME:
            if (indice > 0) tempoFinalizacaoMs = indice;
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
            indicarSensores(value_JS_E, value_IR_E, value_LDR, value_IR_D, value_JS_D);
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
//=================================//CONFIGURACAO (PINOS E NVS)//================================//
//===============================================================================================//

#pragma region CONFIGURACAO

void printTabelaPinos() {
    logBT("\n//=====//CONFIGURACAO (PINOS E PARAMETROS)//=====//\n"
          "ID | Componente            | Valor\n"
          "---|-----------------------|---------\n");
    for (int i = 0; i < NUM_PIN_CONFIG; i++) {
        logBT("%2d | %-21s | GPIO %d\n", i, PIN_CONFIG[i].nome, *PIN_CONFIG[i].pino);
    }
    for (int i = 0; i < NUM_THRESHOLD_CONFIG; i++) {
        logBT("%2d | %-21s | %d\n", NUM_PIN_CONFIG + i, THRESHOLD_CONFIG[i].nome, *THRESHOLD_CONFIG[i].valor);
    }
    logBT("---|-----------------------|---------\n"
          "Envie o ID para alterar | 'x' salva e reinicia\n\n");
}

void configurarPinos() {
    int selecionado = -1;
    int totalEntradas = NUM_PIN_CONFIG + NUM_THRESHOLD_CONFIG;

    printTabelaPinos();
    numLen = 0;

    auto aplicarValor = [&](int val) {
        if (selecionado < NUM_PIN_CONFIG) {
            if (val < 0 || val > 39) {
                logBT("GPIO %d invalida (0-39)!\n", val);
                return false;
            }
            uint8_t* pino = PIN_CONFIG[selecionado].pino;
            bool analogico = (pino == &LINHA_DIR_PIN || pino == &LINHA_ESQ_PIN || pino == &LDR_PIN);
            if (analogico && val < 32) {
                logBT("%s e analogico: use uma GPIO do ADC1 (32-39)!\n", PIN_CONFIG[selecionado].nome);
                return false;
            }
            *PIN_CONFIG[selecionado].pino = (uint8_t)val;
        } else {
            int ti = selecionado - NUM_PIN_CONFIG;
            if (val < 0 || val > THRESHOLD_CONFIG[ti].maximo) {
                logBT("Valor %d invalido (0-%d)!\n", val, THRESHOLD_CONFIG[ti].maximo);
                return false;
            }
            *THRESHOLD_CONFIG[ti].valor = val;
        }
        return true;
    };

    auto selecionarID = [&](int id) {
        if (id < 0 || id >= totalEntradas) {
            logBT("ID %d invalido!\n", id);
            return;
        }
        selecionado = id;
        if (id < NUM_PIN_CONFIG)
            logBT("%s [GPIO %d] -> Novo valor: ", PIN_CONFIG[id].nome, *PIN_CONFIG[id].pino);
        else
            logBT("%s [%d] -> Novo valor: ", THRESHOLD_CONFIG[id - NUM_PIN_CONFIG].nome,
                  *THRESHOLD_CONFIG[id - NUM_PIN_CONFIG].valor);
    };

    auto confirmarNumero = [&]() {
        numBuf[numLen] = '\0';
        numLen = 0;
        int val = atoi(numBuf);
        if (selecionado < 0) {
            selecionarID(val);
        } else {
            if (aplicarValor(val)) {
                selecionado = -1;
                printTabelaPinos();
            }
        }
    };

    for (;;) {
        char c;
        if (xQueueReceive(btQueue, &c, pdMS_TO_TICKS(10))) {
            if (c == 'x' || c == 'X') {
                salvarPinConfig();
                logBT("//=====//CONFIGURACAO SALVA. REINICIANDO...//=====//\n");
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP.restart();
            }
            if (c == '?' && selecionado < 0) {
                numLen = 0;
                printTabelaPinos();
            } else if (c >= '0' && c <= '9') {
                if (numLen < (int)sizeof(numBuf) - 1) numBuf[numLen++] = c;
                ultimoDigitoMs = millis();
            } else if (c == '\n' || c == '\r') {
                if (numLen > 0) confirmarNumero();
            }
        } else if (numLen > 0 && millis() - ultimoDigitoMs >= BT_NUM_TIMEOUT_MS) {
            confirmarNumero();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

#pragma endregion

//===============================================================================================//
//=====================================//VISUALIZAR MACROS//=====================================//
//===============================================================================================//

#pragma region VISUALIZAR MACROS

void printTabelaMacros() {
    logBT("\n//=====//MACROS DE INICIACAO//=====//\n"
          "Indice | Macro\n"
          "-------|---------------------------------------\n");
    for (int i = 0; i < NUM_MACROS; i++) logBT(" %2d    | %s\n", i + 1, NOMES_MACROS[i]);
    logBT("-------|---------------------------------------\n"
          "Envie o indice para ver os passos | '?' reimprime | 'R' reinicia o ESP\n\n");
}

void printPassosMacro(const char* titulo, const OpeningStep* passos) {
    logBT("%s\n"
          "Passo | Vel E | Vel D | Tempo (ms)\n"
          "------|-------|-------|-----------\n", titulo);
    for (int i = 0; passos[i].delayMs > 0; i++) {
        logBT(" %3d  | %5d | %5d | %6d\n", i + 1, passos[i].speedLeft, passos[i].speedRight,
              passos[i].delayMs);
    }
}

// Imprime os passos atuais da macro (mesmo indice da etapa INICIACAO)
void printMacro(int indice) {
    if (indice < 1 || indice > NUM_MACROS) {
        logBT("Indice %d invalido (1-%d)!\n", indice, NUM_MACROS);
        return;
    }
    const OpeningStep* esq = TABELA_MACROS_ESQ[indice - 1];
    const OpeningStep* dir = TABELA_MACROS_DIR[indice - 1];
    logBT("\n//=====//MACRO %d: %s//=====//\n", indice, NOMES_MACROS[indice - 1]);

    if (esq == curvaBordaDummy) {
        logBT("Macro especial (sem matriz), para o lado da DIRECAO escolhida:\n"
              "  1. Giro no eixo a %d por %lu ms\n"
              "  2. Frente a %d ate um sensor de linha ver a borda\n"
              "  3. Re a %d por %lu ms\n"
              "  4. Giro no eixo de volta a %d por %lu ms\n",
              curvaBordaVelGiro, curvaBordaGiroMs, curvaBordaVelAvanco,
              curvaBordaVelRe, curvaBordaReMs, curvaBordaVelGiro, curvaBordaGiroVoltaMs);
    } else if (esq == dir) {
        printPassosMacro("Mesmos passos para ESQUERDA e DIREITA", esq);
    } else {
        printPassosMacro("Direcao ESQUERDA", esq);
        printPassosMacro("Direcao DIREITA", dir);
    }
    logBT("\nEnvie outro indice | '?' reimprime a lista\n\n");
}

void verMacros() {
    printTabelaMacros();
    numLen = 0;

    for (;;) {
        char c;
        if (xQueueReceive(btQueue, &c, pdMS_TO_TICKS(10))) {
            if (c >= '0' && c <= '9') {
                if (numLen < (int)sizeof(numBuf) - 1) numBuf[numLen++] = c;
                ultimoDigitoMs = millis();
            } else if ((c == '\n' || c == '\r') && numLen > 0) {
                numBuf[numLen] = '\0';
                numLen = 0;
                printMacro(atoi(numBuf));
            } else if (c == '?') {
                numLen = 0;
                printTabelaMacros();
            } else if (c == 'R' || c == 'r') {
                logBT("//=====//REINICIANDO ESP//=====//\n");
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP.restart();
            }
        } else if (numLen > 0 && millis() - ultimoDigitoMs >= BT_NUM_TIMEOUT_MS) {
            numBuf[numLen] = '\0';
            numLen = 0;
            printMacro(atoi(numBuf));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
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

    // Testes e configuracao comecam assim que selecionados e nao voltam
    if (modoBT == MODO_TESTE_SENSOR) {
        logBT("//=====//TESTE SENSOR INICIADO//=====//\n");
        testSensors();
    } else if (modoBT == MODO_TESTE_MOTOR) {
        logBT("//=====//TESTE MOTOR INICIADO//=====//\n");
        testMotors();
    } else if (modoBT == MODO_CONFIGURACAO) {
        configurarPinos();
    } else if (modoBT == MODO_MACROS) {
        verMacros();
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
