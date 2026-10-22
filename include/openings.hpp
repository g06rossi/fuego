//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef OPENINGS_H
#define OPENINGS_H

#include <defines.hpp>                        // Definicoes globais
#include <functions.hpp>                      // Funcoes auxiliares
#include <move.hpp>                           // Funcoes de movivmentacao de motores

#pragma endregion

//===============================================================================================//
//===================================//ABERTURAS SEQUENCIAIS//===================================//
//===============================================================================================//

#pragma region STRATS SEQUENCIAIS

/*
!INFO | Estrutura das aberturas sequenciais
-----------------------------------
A estrutura das aberturas sequenciais deve seguir o formato
const OpeningStep nome[] = {
    {   a,    b,    c}, 
    {   d,    e,    f}, 
    [...]
    {   0,    0,    0}                        // Indica que a sequencia acabou
};
*/

#pragma endregion

//==========================================//FRENTAO E FRENTINHO//==========================================//

#pragma region FRENTAO E FRENTINHO

const OpeningStep frentao[] = {
    {255, 255, 200},
    {  0,   0,   0}
};

const OpeningStep frentinho[] = {
    {127, 127, 200},
    {  0,   0,   0}
};

#pragma endregion

//==============================================//CURVA//=============================================//

#pragma region CURVA

const OpeningStep curvaEsquerda[] = {
    {-255,  255,  100},
    { 255,  102,  144},
    { 255, -255,  120},
    { 127,  127,   75},
    {   0,    0,    0}
};

const OpeningStep curvaDireita[] = {
    { 255, -255,  100},
    { 127,  255,  144},
    {-255,  255,  130},
    { 127,  127,   75},
    {   0,    0,    0}
};

#pragma endregion

//==============================================//CURVAO//============================================//

#pragma region CURVAO

const OpeningStep curvaoEsquerda[] = {
    {-255,  255,   70},
    { 255,   51,  170},
    { 255,   20,  300},
    { 255, -255,  120},
    { 255,  255,   30},
    {   0,    0,    0}
};

const OpeningStep curvaoDireita[] = {
    { 255, -255,   70},
    {  69,  255,  150},
    {  41,  255,  220},
    {-255,  255,  150},
    { 255,  255,   60},
    {   0,    0,    0}
};

#pragma endregion

//===============================================//EM V//=============================================//

#pragma region EM V

const OpeningStep emVEsquerda[] = {
    {-255,  255,   40},
    { 255,  255,  155},
    { 255, -255,  190},
    { 255,  255,  170},
    {   0,    0,    0}
};

const OpeningStep emVDireita[] = {
    { 255, -255,   45},
    { 255,  255,  190},
    {-255,  255,  155},
    { 255,  255,  200},
    {   0,    0,    0}
};

#pragma endregion

//==============================================//VZINHO//============================================//

#pragma region VZINHO

const OpeningStep vzinhoEsquerda[] = {
    { 255,  255,  190},
    { 255, -255,  180},
    { 255,  255,  180},
    {   0,    0,    0}
};

const OpeningStep vzinhoDireita[] = {
    { 255,  255,  190},
    {-255,  255,  155},
    { 255,  255,  190},
    {   0,    0,    0}
};

#pragma endregion

//===============================================//VZAO//=============================================//

#pragma region VZAO

const OpeningStep vzaoEsquerda[] = {
    {-255,  255,   90},
    { 255,  255,  145},
    { 255, -255,  200},
    { 255,  255,  170},
    {   0,    0,    0}
};

const OpeningStep vzaoDireita[] = {
    { 255, -255,   90},
    { 255,  255,  170},
    {-255,  255,  145},
    { 255,  255,  170},
    {   0,    0,    0}
};

#pragma endregion

//===============================================//RECUO//============================================//

#pragma region GIRO

const OpeningStep giroEsquerda[] = {
    { -255, 255,  180},
    {   0,    0,    0}
};

const OpeningStep giroDireita[] = {
    { 255, -255,  180},
    {   0,    0,    0}
};

#pragma endregion

//=============================================//DESEMPATE//==========================================//

#pragma region DESEMPATE

const OpeningStep desempateEsquerda[] = {
    { 255,  216,  120},
    { 255, -255,  180},
    {   0,    0,    0}
};

const OpeningStep desempateDireita[] = {
    { 216,  255,  120},
    {-255,  255,  180},
    {   0,    0,    0}
};

#pragma endregion

//===============================================================================================//
//==================================//TABELAS DE ABERTURAS//=====================================//
//===============================================================================================//

#pragma region TABELAS

// A Curva de Borda e uma funcao (executarCurvaDeBorda), nao uma matriz: avanca ate achar a linha
const int curvaBordaVelGiro                 = 230;   // Giros de entrada e de saida (no eixo)
const unsigned long curvaBordaGiroMs        = 65;    // Giro de entrada
const int curvaBordaVelAvanco               = 127;   // Avanco ate um sensor de linha ver a borda
const int curvaBordaVelRe                   = 255;
const unsigned long curvaBordaReMs          = 80;
const unsigned long curvaBordaGiroVoltaMs   = 170;   // Giro de saida

/*
!INFO | Iniciacoes padrao, NVS e historico
-----------------------------------
As matrizes acima sao os PADROES. No boot, carregarMacros() copia os padroes para macros[] (RAM) e
aplica por cima o que estiver salvo na NVS: posicoes sobrescritas e posicoes novas criadas pelo
historico da estrategia personalizada. Tudo que executa, lista ou imprime iniciacoes usa macros[].
O indice do BT e a posicao + 1
*/

struct MacroPadrao {
    const char* nome;
    const OpeningStep* esq;                   // nullptr = Curva de Borda (funcao, sem matriz)
    const OpeningStep* dir;
};

const MacroPadrao MACROS_PADRAO[] = {
    {"Frentao",        frentao,           frentao},
    {"Frentinho",      frentinho,         frentinho},
    {"Curva",          curvaEsquerda,     curvaDireita},
    {"Curvao",         curvaoEsquerda,    curvaoDireita},
    {"Em V",           emVEsquerda,       emVDireita},
    {"Vzinho",         vzinhoEsquerda,    vzinhoDireita},
    {"Vzao",           vzaoEsquerda,      vzaoDireita},
    {"Giro",           giroEsquerda,      giroDireita},
    {"Desempate",      desempateEsquerda, desempateDireita},
    {"Curva de Borda", nullptr,           nullptr},
};
const int NUM_MACROS_PADRAO = sizeof(MACROS_PADRAO) / sizeof(MACROS_PADRAO[0]);

struct Macro {
    char nome[NOME_MACRO_MAX];
    bool curvaBorda;
    OpeningStep esq[MAX_STEPS];               // Terminadas em {0, 0, 0}
    OpeningStep dir[MAX_STEPS];
};

Macro macros[MAX_MACROS];
int numMacros = 0;

// Ultimas personalizadas enviadas (buffer circular). No menu, 1 = a mais nova
OpeningStep historico[HISTORICO_TAMANHO][MAX_STEPS];
int historicoQtd = 0;
int historicoProximo = 0;

#pragma endregion

//===============================================================================================//
//===================================//INICIACOES NA NVS//=======================================//
//===============================================================================================//

#pragma region INICIACOES NVS

int contarPassos(const OpeningStep* passos) {
    int n = 0;
    while (n < MAX_STEPS - 1 && passos[n].delayMs > 0) n++;
    return n;
}

// Copia a matriz e fecha com {0, 0, 0}. Espelhar troca as rodas: transforma a versao de um lado
// na do outro
void copiarPassos(OpeningStep* destino, const OpeningStep* origem, bool espelhar = false) {
    int n = contarPassos(origem);
    for (int i = 0; i < n; i++) {
        destino[i].speedLeft  = espelhar ? origem[i].speedRight : origem[i].speedLeft;
        destino[i].speedRight = espelhar ? origem[i].speedLeft  : origem[i].speedRight;
        destino[i].delayMs    = origem[i].delayMs;
    }
    destino[n] = {0, 0, 0};
}

bool passosIguais(const OpeningStep* a, const OpeningStep* b) {
    int n = contarPassos(a);
    if (n != contarPassos(b)) return false;
    for (int i = 0; i < n; i++) {
        if (a[i].speedLeft != b[i].speedLeft || a[i].speedRight != b[i].speedRight ||
            a[i].delayMs != b[i].delayMs) return false;
    }
    return true;
}

// Formato compacto na NVS: velocidades cabem em int16 e tempos (ate PASSO_MAX_MS) em uint16
struct PassoNVS {
    int16_t velEsq;
    int16_t velDir;
    uint16_t tempoMs;
};

struct MacroNVS {
    char nome[NOME_MACRO_MAX];
    uint8_t nEsq;
    uint8_t nDir;
    PassoNVS esq[MAX_STEPS - 1];
    PassoNVS dir[MAX_STEPS - 1];
};

struct HistoricoNVS {
    uint8_t n;
    PassoNVS passos[MAX_STEPS - 1];
};

uint8_t empacotarPassos(PassoNVS* destino, const OpeningStep* origem) {
    int n = contarPassos(origem);
    for (int i = 0; i < n; i++) destino[i] = {(int16_t)origem[i].speedLeft, (int16_t)origem[i].speedRight,
                                               (uint16_t)origem[i].delayMs};
    return n;
}

void desempacotarPassos(OpeningStep* destino, const PassoNVS* origem, uint8_t n) {
    if (n > MAX_STEPS - 1) n = MAX_STEPS - 1;
    for (int i = 0; i < n; i++) destino[i] = {origem[i].velEsq, origem[i].velDir, origem[i].tempoMs};
    destino[n] = {0, 0, 0};
}

// Padroes do codigo + o que estiver salvo na NVS. Chamar no setup
void carregarMacros() {
    numMacros = NUM_MACROS_PADRAO;
    for (int i = 0; i < NUM_MACROS_PADRAO; i++) {
        strlcpy(macros[i].nome, MACROS_PADRAO[i].nome, NOME_MACRO_MAX);
        macros[i].curvaBorda = (MACROS_PADRAO[i].esq == nullptr);
        if (!macros[i].curvaBorda) {
            copiarPassos(macros[i].esq, MACROS_PADRAO[i].esq);
            copiarPassos(macros[i].dir, MACROS_PADRAO[i].dir);
        }
    }

    if (!preferences.begin(NVS_NAMESPACE_MACROS, true)) return;   // Nada salvo ainda

    int total = preferences.getUChar("n", NUM_MACROS_PADRAO);
    if (total > MAX_MACROS) total = MAX_MACROS;
    char chave[8];
    MacroNVS registro;
    for (int i = 0; i < total; i++) {
        snprintf(chave, sizeof(chave), "m%d", i);
        if (!preferences.isKey(chave)) continue;
        if (preferences.getBytes(chave, &registro, sizeof(registro)) != sizeof(registro)) continue;
        registro.nome[NOME_MACRO_MAX - 1] = '\0';
        strlcpy(macros[i].nome, registro.nome, NOME_MACRO_MAX);
        macros[i].curvaBorda = false;
        desempacotarPassos(macros[i].esq, registro.esq, registro.nEsq);
        desempacotarPassos(macros[i].dir, registro.dir, registro.nDir);
        if (i + 1 > numMacros) numMacros = i + 1;
    }

    historicoQtd = preferences.getUChar("hq", 0);
    if (historicoQtd > HISTORICO_TAMANHO) historicoQtd = HISTORICO_TAMANHO;
    historicoProximo = preferences.getUChar("hp", 0) % HISTORICO_TAMANHO;
    HistoricoNVS item;
    for (int i = 0; i < HISTORICO_TAMANHO; i++) {
        snprintf(chave, sizeof(chave), "h%d", i);
        historico[i][0] = {0, 0, 0};
        if (preferences.isKey(chave) && preferences.getBytes(chave, &item, sizeof(item)) == sizeof(item)) {
            desempacotarPassos(historico[i], item.passos, item.n);
        }
    }
    preferences.end();
}

// Grava a iniciacao da posicao (base 0) na NVS. Devolve false se a NVS nao abrir
bool salvarMacroNVS(int posicao) {
    if (!preferences.begin(NVS_NAMESPACE_MACROS, false)) return false;
    MacroNVS registro = {};
    strlcpy(registro.nome, macros[posicao].nome, NOME_MACRO_MAX);
    registro.nEsq = empacotarPassos(registro.esq, macros[posicao].esq);
    registro.nDir = empacotarPassos(registro.dir, macros[posicao].dir);
    char chave[8];
    snprintf(chave, sizeof(chave), "m%d", posicao);
    preferences.putBytes(chave, &registro, sizeof(registro));
    preferences.putUChar("n", numMacros);
    preferences.end();
    return true;
}

// Item do historico na ordem do menu (1 = mais nova)
OpeningStep* historicoItem(int indice) {
    return historico[(historicoProximo - indice + 2 * HISTORICO_TAMANHO) % HISTORICO_TAMANHO];
}

// Guarda a matriz como a mais nova do historico (a mais antiga sai) e grava na NVS
bool adicionarHistorico(const OpeningStep* passos) {
    int posicao = historicoProximo;
    copiarPassos(historico[posicao], passos);
    historicoProximo = (historicoProximo + 1) % HISTORICO_TAMANHO;
    if (historicoQtd < HISTORICO_TAMANHO) historicoQtd++;

    if (!preferences.begin(NVS_NAMESPACE_MACROS, false)) return false;
    HistoricoNVS item = {};
    item.n = empacotarPassos(item.passos, historico[posicao]);
    char chave[8];
    snprintf(chave, sizeof(chave), "h%d", posicao);
    preferences.putBytes(chave, &item, sizeof(item));
    preferences.putUChar("hq", historicoQtd);
    preferences.putUChar("hp", historicoProximo);
    preferences.end();
    return true;
}

#pragma endregion

//===============================================================================================//
//====================================//EXECUTAR ABERTURA//======================================//
//===============================================================================================//

#pragma region EXECUTAR

// Abre a asa (se escolhida na etapa ASA) com ASA_ANTECEDENCIA_MS de vantagem sobre a macro. O servo
// roda na propria task, entao termina o curso em paralelo com o movimento
void abrirAsaAntesDeMover() {
    if (!abrirAsa) return;
    xTaskNotifyGive(openServoHandle);
    if (ASA_ANTECEDENCIA_MS > 0) vTaskDelay(pdMS_TO_TICKS(ASA_ANTECEDENCIA_MS));
}

// Inicio de toda iniciacao com movimento: JSumos sempre desligados e asa aberta antes de andar
void prepararIniciacao() {
    definirJSumos(false);
    abrirAsaAntesDeMover();
}

// Executa as estrategias sequenciais e personalizadas
void executarOpening(const OpeningStep strategySequence[]) {
    prepararIniciacao();

    // Loop de passos -> sai do loop quando o delay for igual a 0
    for (int i = 0; strategySequence[i].delayMs > 0; ++i) {
        moverMotores(strategySequence[i].speedLeft, strategySequence[i].speedRight);
        vTaskDelay(pdMS_TO_TICKS(strategySequence[i].delayMs));
    }

    definirJSumos(!furtivoMovimentacao);
    // Para o robo ao final da execucao
    moverMotores(0, 0);
}

#pragma endregion

//===============================================================================================//
//====================================//TESTES SENSOR MOTOR//====================================//
//===============================================================================================//

#pragma region TESTES

// Teste de sensor: imprime a cada 200ms a tabela de leitura de todos os sensores do Fuego
// e espelha nos LEDs enderecaveis (JS_E, IR_E, LDR, IR_D, JS_D)
void testSensors() {
    char tabela[512];
    for(;;) {
        indicarSensores(value_JS_E, value_IR_E, value_LDR, value_IR_D, value_JS_D);

        int pos = 0;
        pos += snprintf(tabela + pos, sizeof(tabela) - pos,
            "\n||==========||SENSORES||==========||\n"
            "Pino | Sensor        | Leitura\n"
            "-----|---------------|--------\n");
        pos += snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | JSUMO_DIR_PIN | %d\n", JSUMO_DIR_PIN, (int)value_JS_D);
        pos += snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | JSUMO_ESQ_PIN | %d\n", JSUMO_ESQ_PIN, (int)value_JS_E);
        pos += snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | IR_DIR_PIN    | %d\n", IR_DIR_PIN, (int)value_IR_D);
        pos += snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | IR_ESQ_PIN    | %d\n", IR_ESQ_PIN, (int)value_IR_E);
        pos += snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | LINHA_DIR_PIN | %d (ADC %d / limiar %d)\n",
            LINHA_DIR_PIN, (int)value_QRE_D, (int)adc_QRE_D, LINHA_TRESHOLD);
        pos += snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | LINHA_ESQ_PIN | %d (ADC %d / limiar %d)\n",
            LINHA_ESQ_PIN, (int)value_QRE_E, (int)adc_QRE_E, LINHA_TRESHOLD);
        snprintf(tabela + pos, sizeof(tabela) - pos, " %2d  | LDR_PIN       | %d (ADC %d / limiar %d)\n"
            "Ciclo ADC max: %lu us (limite %d)\n"
            "||============================||", LDR_PIN, (int)value_LDR, (int)adc_LDR, LDR_TRESHOLD,
            (unsigned long)adcCicloMaxUs, ADC_CICLO_LIMITE_US);

        Serial.println(tabela);
        SerialBT.println(tabela);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Teste de motor
void testMotors() {
    for(;;) {
        moverMotores(255, 255);
        SerialBT.println("FRENTE");
        Serial.println("FRENTE");
        vTaskDelay(pdMS_TO_TICKS(1000));

        moverMotores(-255, 255);
        SerialBT.println("ESQUERDA");
        Serial.println("ESQUERDA");
        vTaskDelay(pdMS_TO_TICKS(1000));

        moverMotores(255, -255);
        SerialBT.println("DIREITA");
        Serial.println("DIREITA");
        vTaskDelay(pdMS_TO_TICKS(1000));

        moverMotores(-255, -255);
        SerialBT.println("TRAS");
        Serial.println("TRAS");
        vTaskDelay(pdMS_TO_TICKS(1000));

        moverMotores(0, 0);
        SerialBT.println("PARADO");
        Serial.println("PARADO");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#pragma endregion

//===============================================================================================//
//===================================//SELECIONAR ESTRATEGIA//===================================//
//===============================================================================================//

#pragma region SELECIONAR

void executarCurvaDeBorda() {
    prepararIniciacao();

    // Passo 1: Giro
    if (direction == esquerda) moverMotores(-curvaBordaVelGiro, curvaBordaVelGiro);
    else moverMotores(curvaBordaVelGiro, -curvaBordaVelGiro);
    vTaskDelay(pdMS_TO_TICKS(curvaBordaGiroMs));

    // Passo 2: Avanco ate achar a linha
    moverMotores(curvaBordaVelAvanco, curvaBordaVelAvanco);
    while (!value_QRE_E && !value_QRE_D && !roboParado) {   // Travado pelo IR 3 nunca chegaria
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Passo 3: Retorno
    moverMotores(-curvaBordaVelRe, -curvaBordaVelRe);
    vTaskDelay(pdMS_TO_TICKS(curvaBordaReMs));

    if (direction == esquerda) moverMotores(curvaBordaVelGiro, -curvaBordaVelGiro);
    else moverMotores(-curvaBordaVelGiro, curvaBordaVelGiro);
    vTaskDelay(pdMS_TO_TICKS(curvaBordaGiroVoltaMs));

    definirJSumos(!furtivoMovimentacao);
    moverMotores(0, 0);
}

// Executa a iniciacao escolhida nas etapas do BT (macro de macros[] ou nenhuma)
void openingsLutaBT() {
    const OpeningStep* sequencia = nullptr;

    if (macroIndex > 0 && macroIndex <= numMacros) {
        Macro& macro = macros[macroIndex - 1];
        SerialBT.printf("//=====//%s INICIADO//=====//\n", macro.nome);

        if (macro.curvaBorda) {
            executarCurvaDeBorda();
            return;
        }
        sequencia = (direction == direita) ? macro.dir : macro.esq;
    }

    if (sequencia != nullptr) {
        executarOpening(sequencia);

    } else {                                  // Iterativo puro (inicia somente modo iterativo)
        SerialBT.println("//=====//ITERATIVO PURO INICIADO//=====//");
        definirJSumos(!furtivoMovimentacao);
        abrirAsaAntesDeMover();
        moverMotores(0, 0);
    }
}

#pragma endregion
 
//===============================================================================================//
//====================================//TASK DA ESTRATEGIA//=====================================//
//===============================================================================================//

#pragma region TASKS

// Inicia a estrategia inicial quando notificado pelo caractere '2' do controle remoto
void openingsTask(void *pvParameters) {
    for(;;) {
        // Aguarda ser chamado
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        resetFightingState();               // Reseta as variaveis de luta
        openingsLutaBT();                   // Executa a iniciacao selecionada nas etapas BT
        if (roboParado) continue;           // IR 3 durante a iniciacao: a luta nunca comeca
        tempoFighting = millis();
        running = true;                       // Ativa o robo
    }
}

void setupOpeningsTask() {
    xTaskCreatePinnedToCore(
        openingsTask,                       // Funcao da tarefa
        "OpeningsTask",                     // Nome da tarefa
        256 * 32,                             // Tamanho da pilha
        nullptr,                              // Parametros
        14,                                   // Prioridade 14
        &openingsHandle,                     // Handle
        APP_CPU_NUM                           // Nucleo 1 onde a tarefa sera executada
    );   
}

#pragma endregion
 
//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif
