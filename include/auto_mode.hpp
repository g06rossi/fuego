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

void reiniciarESP() {
    logBT("//=====//REINICIANDO ESP//=====//\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
}

char linhaBT[BT_BUFFER_SIZE];                 // Ultima linha completa recebida pelo BT
int linhaLen                   = 0;
unsigned long linhaUltimoMs    = 0;

// Acumula o que chega pelo BT e devolve true quando uma linha fica completa em linhaBT: no ENTER
// ou, para terminais que nao enviam ENTER, apos BT_NUM_TIMEOUT_MS sem novos caracteres
bool pollLinhaBT() {
    char c;
    bool completa = false;
    while (!completa && xQueueReceive(btQueue, &c, 0)) {
        if (c == '\n' || c == '\r') {
            completa = (linhaLen > 0);
        } else if (c >= 32 && linhaLen < BT_BUFFER_SIZE - 1) {
            linhaBT[linhaLen++] = c;
            linhaUltimoMs = millis();
        }
    }
    if (!completa && linhaLen > 0 && millis() - linhaUltimoMs >= BT_NUM_TIMEOUT_MS) completa = true;
    if (!completa) return false;

    linhaBT[linhaLen] = '\0';
    linhaLen = 0;
    return true;
}

// Espera uma linha completa. O texto vale ate a proxima leitura
const char* esperarLinhaBT() {
    while (!pollLinhaBT()) vTaskDelay(pdMS_TO_TICKS(5));
    return linhaBT;
}

// Linha com um unico caractere de comando (sem diferenciar maiuscula)
bool ehComando(const char* linha, char comando) {
    return linha[0] != '\0' && linha[1] == '\0' && tolower(linha[0]) == comando;
}

// Converte a linha em indice. Devolve -1 se nao for so digitos
int indiceDaLinha(const char* linha) {
    if (linha[0] == '\0') return -1;
    for (const char* c = linha; *c; c++) {
        if (*c < '0' || *c > '9') return -1;
    }
    return atoi(linha);
}

const char* nomeModoBT() {
    switch (modoBT) {
        case MODO_TESTE_SENSOR:  return "Teste de sensor";
        case MODO_TESTE_MOTOR:   return "Teste de motor";
        case MODO_PERSONALIZADA: return "Estrategia personalizada";
        case MODO_CONFIGURACAO:  return "Configuracao (pinos e parametros)";
        case MODO_MACROS:        return "Macros (ver valores das iniciacoes)";
        case MODO_RAPIDA_JSUMO_LIGADO:    return "Rapida JSumo Ligado";
        case MODO_RAPIDA_JSUMO_DESLIGADO: return "Rapida JSumo Desligado";
        default:                 return "Luta";
    }
}

// Configuracoes rapidas: tudo predefinido menos a iniciacao e a direcao
bool modoRapido() {
    return modoBT == MODO_RAPIDA_JSUMO_LIGADO || modoBT == MODO_RAPIDA_JSUMO_DESLIGADO;
}

void aplicarPresetRapido() {
    tipoAdversario = advRampaComIR;
    abrirAsa = true;
    modoLuta = buscaDefensiva;
    modoLutaOriginal = buscaDefensiva;
    finalization = FIM_LDR;
    furtivoMovimentacao = (modoBT == MODO_RAPIDA_JSUMO_DESLIGADO);
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
    bool rapida = modoRapido();               // Itens predefinidos ja contam como selecionados
    bool luta = (modoBT == MODO_LUTA || rapida);
    char valor[40];

    logBT("\n//=====//SELECAO ATUAL//=====//\n");
    linhaLogSelecao("Modo", currentStage > STAGE_MODE, nomeModoBT());

    if (luta || currentStage == STAGE_MODE) {
        linhaLogSelecao("Adversario", rapida || currentStage > STAGE_ADVERSARY, nomeAdversario());
        linhaLogSelecao("Asa", rapida || currentStage > STAGE_WING, abrirAsa ? "Aberta" : "Fechada");

        linhaLogSelecao("Iniciacao", currentStage > STAGE_INITIATION,
                        macroIndex == 0 ? "Nenhuma (iterativo puro)" : macros[macroIndex - 1].nome);

        linhaLogSelecao("Direcao", currentStage > STAGE_DIRECTION, direction == direita ? "Direita" : "Esquerda");
        linhaLogSelecao("Movimentacao", rapida || currentStage > STAGE_MOVEMENT, nomeMovimento());
        linhaLogSelecao("Furtivo Mov.", rapida || currentStage > STAGE_FURTIVE_MOVEMENT,
                        furtivoMovimentacao ? "Ativado" : "Desativado");
        linhaLogSelecao("Finalizacao", rapida || currentStage > STAGE_FINALIZATION,
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
                  "  3    | Estrategia personalizada (testa movimentos, sem luta)\n"
                  "  4    | Configuracao (pinos e parametros)\n"
                  "  5    | Macros (ver valores das iniciacoes)\n"
                  "  6    | Rapida JSumo Ligado\n"
                  "  7    | Rapida JSumo Desligado (furtivo)\n"
                  "       | Rapidas: Rampa c/ IR, asa aberta, Busca Defensiva,\n"
                  "       | finalizacao por LDR. So pergunta iniciacao e direcao\n");
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
            for (int i = 0; i < numMacros; i++) logBT(" %2d    | %s\n", i + 1, macros[i].nome);
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
            if (modoBT == MODO_LUTA) currentStage = STAGE_ADVERSARY;
            else if (modoRapido()) currentStage = STAGE_INITIATION;
            else currentStage = STAGE_DONE;   // Testes, Personalizada, Config e Macros: menu proprio
            break;
        case STAGE_ADVERSARY:        currentStage = STAGE_WING;             break;
        case STAGE_WING:             currentStage = STAGE_INITIATION;       break;
        case STAGE_INITIATION:       currentStage = STAGE_DIRECTION;        break;
        case STAGE_DIRECTION:
            currentStage = modoRapido() ? STAGE_DONE : STAGE_MOVEMENT;
            break;
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
            if (indice >= MODO_LUTA && indice <= MODO_RAPIDA_JSUMO_DESLIGADO) {
                modoBT = (ModoBT)indice;
                if (modoRapido()) aplicarPresetRapido();
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
            if (indice >= 0 && indice <= numMacros) macroIndex = indice;
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
        reiniciarESP();
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
            processarIndice(receivedChar);

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
            if (val < THRESHOLD_CONFIG[ti].minimo || val > THRESHOLD_CONFIG[ti].maximo) {
                logBT("Valor %d invalido (%d-%d)!\n", val, THRESHOLD_CONFIG[ti].minimo,
                      THRESHOLD_CONFIG[ti].maximo);
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

// Lista as iniciacoes de macros[] com o mesmo indice da etapa INICIACAO
void printListaIniciacoes() {
    for (int i = 0; i < numMacros; i++) logBT(" %2d    | %s\n", i + 1, macros[i].nome);
}

void printTabelaMacros() {
    logBT("\n//=====//MACROS DE INICIACAO//=====//\n"
          "Indice | Macro\n"
          "-------|---------------------------------------\n"
          "  0    | Todas\n");
    printListaIniciacoes();
    logBT("-------|---------------------------------------\n"
          "Envie o indice para ver os passos | '?' reimprime | 'R' reinicia o ESP\n\n");
}

void printPassosMacro(const char* titulo, const OpeningStep* passos) {
    logBT("%s\n"
          "Passo | Vel E | Vel D | Tempo (ms)\n"
          "------|-------|-------|-----------\n", titulo);
    for (int i = 0; i < contarPassos(passos); i++) {
        logBT(" %3d  | %5d | %5d | %6d\n", i + 1, passos[i].speedLeft, passos[i].speedRight,
              passos[i].delayMs);
    }
}

// Imprime os passos atuais de uma iniciacao (indice 1 = macros[0])
void printMacro(int indice) {
    const Macro& macro = macros[indice - 1];
    logBT("\n//=====//MACRO %d: %s//=====//\n", indice, macro.nome);

    if (macro.curvaBorda) {
        logBT("Macro especial (sem matriz), para o lado da DIRECAO escolhida:\n"
              "  1. Giro no eixo a %d por %lu ms\n"
              "  2. Frente a %d ate um sensor de linha ver a borda\n"
              "  3. Re a %d por %lu ms\n"
              "  4. Giro no eixo de volta a %d por %lu ms\n",
              curvaBordaVelGiro, curvaBordaGiroMs, curvaBordaVelAvanco,
              curvaBordaVelRe, curvaBordaReMs, curvaBordaVelGiro, curvaBordaGiroVoltaMs);
    } else if (passosIguais(macro.esq, macro.dir)) {
        printPassosMacro("Mesmos passos para ESQUERDA e DIREITA", macro.esq);
    } else {
        printPassosMacro("Direcao ESQUERDA", macro.esq);
        printPassosMacro("Direcao DIREITA", macro.dir);
    }
}

void verMacros() {
    printTabelaMacros();

    for (;;) {
        const char* linha = esperarLinhaBT();
        if (ehComando(linha, 'r')) reiniciarESP();
        if (ehComando(linha, '?')) {
            printTabelaMacros();
            continue;
        }

        int indice = indiceDaLinha(linha);
        if (indice == 0) {
            for (int i = 1; i <= numMacros; i++) printMacro(i);
        } else if (indice >= 1 && indice <= numMacros) {
            printMacro(indice);
        } else {
            logBT("Indice invalido (0-%d)!\n", numMacros);
            continue;
        }
        logBT("\nEnvie outro indice | '?' reimprime a lista\n\n");
    }
}

#pragma endregion

//===============================================================================================//
//==================================//ESTRATEGIA PERSONALIZADA//=================================//
//===============================================================================================//

#pragma region ESTRATEGIA PERSONALIZADA

/*
!INFO | Estrategia personalizada
-----------------------------------
Menu proprio para testar movimentos, sem luta: nada de etapas, Retorno ou busca depois. Dois
modos de envio (movimento completo e por passo) e um historico com as ultimas HISTORICO_TAMANHO
matrizes enviadas, gravado na NVS. Do historico, uma matriz pode ser executada de novo ou salva
como iniciacao (numa posicao existente ou nova), que passa a aparecer na etapa INICIACAO
*/

// Le "vel_esq,vel_dir,tempo_ms" com velocidades de -255 a 255 e tempo de 1 a PASSO_MAX_MS
bool lerPasso(const char* texto, OpeningStep& passo) {
    int esq, dir, tempo;
    if (sscanf(texto, "%d,%d,%d", &esq, &dir, &tempo) != 3) {
        logBT("Formato invalido! Use: vel_esq,vel_dir,tempo_ms\n");
        return false;
    }
    if (abs(esq) > 255 || abs(dir) > 255 || tempo < 1 || tempo > PASSO_MAX_MS) {
        logBT("Fora da faixa! Velocidades de -255 a 255 e tempo de 1 a %d ms\n", PASSO_MAX_MS);
        return false;
    }
    passo = {esq, dir, tempo};
    return true;
}

void iniciarMatrizPersonalizada() {
    customOpeningCount = 0;
    customOpening[0] = {0, 0, 0};
}

// Valida a linha e acrescenta o passo a customOpening
bool adicionarPassoPersonalizado(const char* texto) {
    if (customOpeningCount >= MAX_STEPS - 1) {
        logBT("Limite de %d passos atingido!\n", MAX_STEPS - 1);
        return false;
    }
    OpeningStep passo;
    if (!lerPasso(texto, passo)) return false;
    customOpening[customOpeningCount++] = passo;
    customOpening[customOpeningCount] = {0, 0, 0};
    logBT("Passo %d: E %d | D %d | %d ms\n", customOpeningCount, passo.speedLeft, passo.speedRight,
          passo.delayMs);
    return true;
}

void avisarSeTravado() {
    if (roboParado) logBT("//=====//ATENCAO: ROBO TRAVADO PELO IR 3. Reinicie para mover//=====//\n");
}

// Esvazia a fila de IR e devolve true se veio o botao 2 (comando 0x1). Outros botoes sao ignorados
bool recebeuDoisIR() {
    uint16_t comando;
    bool dois = false;
    while (receberComandoIR(comando)) {
        if (comando == 0x1) dois = true;
    }
    return dois;
}

void executarPersonalizada() {
    avisarSeTravado();
    logBT("//=====//EXECUTANDO (%d passos)//=====//\n", contarPassos(customOpening));
    executarOpening(customOpening);
    descartarComandosIR();                    // Descarta repeticoes do 2 recebidas durante o movimento
    logBT("//=====//MOVIMENTO CONCLUIDO//=====//\n");
}

void registrarNoHistorico() {
    if (adicionarHistorico(customOpening)) logBT("Matriz salva no historico.\n");
    else logBT("ERRO: NVS indisponivel, matriz NAO salva no historico.\n");
}

// Espera o 2 (linha "2" no BT ou botao 2 do controle IR). Devolve false se vier 'x'
bool aguardarLargada() {
    logBT("Envie 2 (BT ou controle IR) para andar | 'x' cancela\n");
    descartarComandosIR();
    for (;;) {
        if (recebeuDoisIR()) return true;     // Botao 2 do controle
        if (pollLinhaBT()) {
            if (strcmp(linhaBT, "2") == 0) return true;
            if (ehComando(linhaBT, 'x')) {
                logBT("Cancelado.\n");
                return false;
            }
            logBT("Envie 2 para andar ou 'x' para cancelar\n");
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Modo A: recebe a matriz inteira e so anda quando recebe o 2
void personalizadaCompleta() {
    iniciarMatrizPersonalizada();
    descartarComandosIR();
    logBT("\n//=====//MOVIMENTO COMPLETO//=====//\n"
          "Envie um passo por linha: vel_esq,vel_dir,tempo_ms (max. %d passos)\n"
          "Envie 2 (BT ou controle IR) para andar | 'x' cancela\n\n", MAX_STEPS - 1);

    for (;;) {
        bool largar = recebeuDoisIR();        // Botao 2 do controle
        if (pollLinhaBT()) {
            if (ehComando(linhaBT, 'x')) {
                logBT("Cancelado.\n");
                return;
            }
            if (strcmp(linhaBT, "2") == 0) largar = true;
            else adicionarPassoPersonalizado(linhaBT);
        }

        if (largar) {
            if (customOpeningCount == 0) {
                logBT("Envie pelo menos um passo antes do 2!\n");
            } else {
                executarPersonalizada();
                registrarNoHistorico();
                return;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Modo B: cada linha anda na hora; 'x' encerra, imprime a matriz e salva no historico
void personalizadaPorPasso() {
    iniciarMatrizPersonalizada();
    logBT("\n//=====//POR PASSO//=====//\n"
          "Cada linha vel_esq,vel_dir,tempo_ms anda na hora (max. %d passos)\n"
          "'x' encerra, imprime a matriz e salva no historico\n\n", MAX_STEPS - 1);

    for (;;) {
        if (pollLinhaBT()) {
            if (ehComando(linhaBT, 'x')) {
                if (customOpeningCount > 0) {
                    printPassosMacro("\nMatriz enviada", customOpening);
                    registrarNoHistorico();
                } else {
                    logBT("Nenhum passo enviado.\n");
                }
                return;
            }
            if (adicionarPassoPersonalizado(linhaBT)) {
                const OpeningStep& passo = customOpening[customOpeningCount - 1];
                moverMotores(passo.speedLeft, passo.speedRight);
                vTaskDelay(pdMS_TO_TICKS(passo.delayMs));
                moverMotores(0, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Espera um indice numerico. Devolve -1 se vier 'x' (cancela) ou algo que nao e numero
int lerIndiceOuCancelar() {
    const char* linha = esperarLinhaBT();
    if (ehComando(linha, 'x')) {
        logBT("Cancelado.\n");
        return -1;
    }
    int indice = indiceDaLinha(linha);
    if (indice < 0) logBT("Envie um numero!\n");
    return indice;
}

// Le um nome (ate NOME_MACRO_MAX - 1 letras). 'x' cancela; com permiteManter, '.' devolve vazio
bool lerNome(char* destino, bool permiteManter) {
    const char* linha = esperarLinhaBT();
    if (ehComando(linha, 'x')) {
        logBT("Cancelado.\n");
        return false;
    }
    if (permiteManter && strcmp(linha, ".") == 0) {
        destino[0] = '\0';
        return true;
    }
    strlcpy(destino, linha, NOME_MACRO_MAX);
    return true;
}

void informarGravacao(int indice) {
    if (salvarMacroNVS(indice - 1)) logBT("Salvo como iniciacao %d - %s\n", indice, macros[indice - 1].nome);
    else logBT("ERRO: NVS indisponivel, a iniciacao vale so ate reiniciar.\n");
}

// Sobrescreve uma iniciacao existente com a matriz de customOpening
bool salvarEmIniciacaoExistente() {
    logBT("\nIndice | Iniciacao a substituir\n"
          "-------|---------------------------------------\n");
    printListaIniciacoes();
    logBT("'x' cancela\n");
    int indice = lerIndiceOuCancelar();
    if (indice < 0) return false;
    if (indice < 1 || indice > numMacros) {
        logBT("Indice invalido!\n");
        return false;
    }
    Macro& macro = macros[indice - 1];
    if (macro.curvaBorda) {
        logBT("A Curva de Borda nao e uma matriz e nao pode ser substituida!\n");
        return false;
    }

    logBT("Salvar em qual lado?\n"
          "  0 | Esquerdo\n"
          "  1 | Direito\n"
          "  2 | Os dois (o direito recebe a matriz espelhada)\n"
          "'x' cancela\n");
    int lado = lerIndiceOuCancelar();
    if (lado < 0) return false;
    if (lado > 2) {
        logBT("Opcao invalida!\n");
        return false;
    }

    logBT("Nome atual: %s. Envie um novo nome (ate %d letras), '.' mantem | 'x' cancela\n",
          macro.nome, NOME_MACRO_MAX - 1);
    char nome[NOME_MACRO_MAX];
    if (!lerNome(nome, true)) return false;

    if (lado == 0 || lado == 2) copiarPassos(macro.esq, customOpening);
    if (lado == 1) copiarPassos(macro.dir, customOpening);
    if (lado == 2) copiarPassos(macro.dir, customOpening, true);
    if (nome[0] != '\0') strlcpy(macro.nome, nome, NOME_MACRO_MAX);
    informarGravacao(indice);
    return true;
}

// Cria uma iniciacao nova com a matriz de customOpening; o outro lado recebe a matriz espelhada
bool salvarEmIniciacaoNova() {
    if (numMacros >= MAX_MACROS) {
        logBT("Limite de %d iniciacoes atingido!\n", MAX_MACROS);
        return false;
    }
    logBT("Nome da nova iniciacao (ate %d letras) | 'x' cancela\n", NOME_MACRO_MAX - 1);
    char nome[NOME_MACRO_MAX];
    if (!lerNome(nome, false)) return false;

    logBT("A matriz e de qual lado? (o outro lado recebe a matriz espelhada)\n"
          "  0 | Esquerdo\n"
          "  1 | Direito\n"
          "'x' cancela\n");
    int lado = lerIndiceOuCancelar();
    if (lado < 0) return false;
    if (lado > 1) {
        logBT("Opcao invalida!\n");
        return false;
    }

    Macro& macro = macros[numMacros];
    strlcpy(macro.nome, nome, NOME_MACRO_MAX);
    macro.curvaBorda = false;
    copiarPassos(lado == 0 ? macro.esq : macro.dir, customOpening);
    copiarPassos(lado == 0 ? macro.dir : macro.esq, customOpening, true);
    numMacros++;
    informarGravacao(numMacros);
    return true;
}

// Acoes sobre um item do historico (ja copiado para customOpening)
void menuItemHistorico(int indice) {
    for (;;) {
        char titulo[32];
        snprintf(titulo, sizeof(titulo), "\nHistorico %d", indice);
        printPassosMacro(titulo, customOpening);
        logBT("\nIndice | Acao\n"
              "-------|---------------------------------------\n"
              "  0    | Executar de novo (anda ao receber 2)\n"
              "  1    | Salvar em uma iniciacao existente\n"
              "  2    | Salvar em uma nova iniciacao\n"
              "'x' volta ao historico\n\n");

        const char* linha = esperarLinhaBT();
        if (ehComando(linha, 'x')) return;
        switch (indiceDaLinha(linha)) {
            case 0:
                if (aguardarLargada()) executarPersonalizada();
                break;
            case 1:
                if (salvarEmIniciacaoExistente()) return;
                break;
            case 2:
                if (salvarEmIniciacaoNova()) return;
                break;
            default:
                logBT("Opcao invalida!\n");
                break;
        }
    }
}

void menuHistorico() {
    for (;;) {
        logBT("\n//=====//HISTORICO (1 = mais nova)//=====//\n"
              "Indice | Passos | Primeiro passo\n"
              "-------|--------|-----------------\n");
        if (historicoQtd == 0) logBT("  (vazio)\n");
        for (int i = 1; i <= historicoQtd; i++) {
            const OpeningStep* passos = historicoItem(i);
            logBT("  %d    |  %2d    | %d,%d,%d\n", i, contarPassos(passos), passos[0].speedLeft,
                  passos[0].speedRight, passos[0].delayMs);
        }
        logBT("Envie o indice para abrir | 'x' volta\n\n");

        const char* linha = esperarLinhaBT();
        if (ehComando(linha, 'x')) return;
        int indice = indiceDaLinha(linha);
        if (indice < 1 || indice > historicoQtd) {
            logBT("Indice invalido!\n");
            continue;
        }
        copiarPassos(customOpening, historicoItem(indice));
        customOpeningCount = contarPassos(customOpening);
        menuItemHistorico(indice);
    }
}

// Modo 3: nunca volta, so sai reiniciando o ESP
void menuPersonalizada() {
    for (;;) {
        logBT("\n//=====//ESTRATEGIA PERSONALIZADA//=====//\n"
              "Indice | Opcao\n"
              "-------|---------------------------------------\n"
              "  0    | Movimento completo (envia a matriz, anda ao receber 2)\n"
              "  1    | Por passo (cada linha anda na hora, 'x' encerra)\n"
              "  2    | Historico (ultimas %d matrizes enviadas)\n"
              "-------|---------------------------------------\n"
              "'R' reinicia o ESP\n\n", HISTORICO_TAMANHO);

        const char* linha = esperarLinhaBT();
        if (ehComando(linha, 'r')) reiniciarESP();
        switch (indiceDaLinha(linha)) {
            case 0: personalizadaCompleta(); break;
            case 1: personalizadaPorPasso(); break;
            case 2: menuHistorico();         break;
            default: logBT("Opcao invalida!\n"); break;
        }
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
    avisarSeTravado();                        // IR 3 recebido antes da selecao (ex.: no IDLE)

    currentStage = STAGE_MODE;
    printLogSelecao();
    printTabelaEtapa();                       // Tabela da primeira etapa (automatica)

    translateBT();                            // Percorre as etapas selecionadas por BT
    bootSenhaCancelar();                      // Fecha a janela da senha de boot

    // Testes, personalizada, configuracao e macros tem menu proprio e nao voltam
    if (modoBT == MODO_PERSONALIZADA) {
        menuPersonalizada();
    } else if (modoBT == MODO_TESTE_SENSOR) {
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
    descartarComandosIR();                    // Descarta IR recebido durante a selecao
    avisarSeTravado();
    largadaLiberada = true;                   // A irMonitorTask passa a repassar os comandos IR
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif
