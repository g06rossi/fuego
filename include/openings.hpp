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

#pragma region RECUO

const OpeningStep recuoEsquerda[] = {
    {-255, -153,  180},
    {   0,    0,    0}
};

const OpeningStep recuoDireita[] = {
    {-153, -255,  180},
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

const OpeningStep curvaBordaDummy[] = {
    {0, 0, 0}
};

const OpeningStep* const TABELA_MACROS_ESQ[] = {
    frentao,            // 0
    frentinho,          // 1
    curvaEsquerda,      // 2
    curvaoEsquerda,     // 3
    emVEsquerda,        // 4
    vzinhoEsquerda,     // 5
    vzaoEsquerda,       // 6
    recuoEsquerda,      // 7
    desempateEsquerda,  // 8
    curvaBordaDummy     // 9
};

const OpeningStep* const TABELA_MACROS_DIR[] = {
    frentao,            // 0
    frentinho,          // 1
    curvaDireita,       // 2
    curvaoDireita,      // 3
    emVDireita,         // 4
    vzinhoDireita,      // 5
    vzaoDireita,        // 6
    recuoDireita,       // 7
    desempateDireita,   // 8
    curvaBordaDummy     // 9
};

const int NUM_MACROS = sizeof(TABELA_MACROS_ESQ) / sizeof(TABELA_MACROS_ESQ[0]);

#pragma endregion

//===============================================================================================//
//======================================//NOMES DAS MACROS//=====================================//
//===============================================================================================//

#pragma region NOMES

// Nomes na mesma ordem das tabelas TABELA_MACROS_ESQ / TABELA_MACROS_DIR (indice BT = posicao + 1)
const char* const NOMES_MACROS[] = {
    "Frentao",
    "Frentinho",
    "Curva",
    "Curvao",
    "Em V",
    "Vzinho",
    "Vzao",
    "Recuo",
    "Desempate",
    "Curva de Borda"
};

#pragma endregion

//===============================================================================================//
//====================================//EXECUTAR ABERTURA//======================================//
//===============================================================================================//

#pragma region EXECUTAR

// Executa as estrategias sequenciais e personalizadas
void executarOpening(const OpeningStep strategySequence[]) {
    // Lida com haste quando necessario
    if (estadoAsa == asaAberta) xTaskNotifyGive(openServoHandle);
    xTaskNotifyGive(swSensorHandle);          // Entra no modo furtivo

    // Loop de passos -> sai do loop quando o delay for igual a 0
    for (int i = 0; strategySequence[i].delayMs > 0; ++i) {
        moverMotores(strategySequence[i].speedLeft, strategySequence[i].speedRight);
        vTaskDelay(pdMS_TO_TICKS(strategySequence[i].delayMs));
    }

    if(!modoFurtivo) xTaskNotifyGive(swSensorHandle);          // Sai do modo furtivo
    // Para o robo ao final da execucao
    moverMotores(0, 0);
}

#pragma endregion

//===============================================================================================//
//====================================//TESTES SENSOR MOTOR//====================================//
//===============================================================================================//

#pragma region TESTES

// Teste de sensor: imprime a cada 50ms a leitura atual de todos os sensores do Fuego e espelha
// nos LEDs enderecaveis (JS_E, IR_E, LDR, IR_D, JS_D)
void testSensors() {
    char linha[128];
    for(;;) {
        indicarSensoresTeste(value_JS_E, value_IR_E, value_LDR, value_IR_D, value_JS_D);

        snprintf(linha, sizeof(linha),
            "SENSORES -> IR_E:%d IR_D:%d | JS_E:%d JS_D:%d | QRE_E:%d QRE_D:%d | LDR:%d",
            (int)value_IR_E, (int)value_IR_D, (int)value_JS_E, (int)value_JS_D,
            (int)value_QRE_E, (int)value_QRE_D, (int)value_LDR);
        Serial.println(linha);
        SerialBT.println(linha);

        vTaskDelay(pdMS_TO_TICKS(50));
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
    if (estadoAsa == asaAberta) xTaskNotifyGive(openServoHandle);
    xTaskNotifyGive(swSensorHandle);          // Entra no modo furtivo

    // Passo 1: Giro
    if (direction == esquerda) moverMotores(-230, 230);
    else moverMotores(230, -230);
    vTaskDelay(pdMS_TO_TICKS(65));

    // Passo 2: Avanco ate achar a linha (127 é o 50/100 de Fumacinha)
    moverMotores(127, 127);
    while (viuLinha == linhaNADA) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Passo 3: Retorno
    moverMotores(-255, -255);
    vTaskDelay(pdMS_TO_TICKS(80));

    if (direction == esquerda) moverMotores(230, -230);
    else moverMotores(-230, 230);
    vTaskDelay(pdMS_TO_TICKS(170));

    if (!modoFurtivo) xTaskNotifyGive(swSensorHandle);          // Sai do modo furtivo
    moverMotores(0, 0);
}

// Executa a iniciacao escolhida nas etapas do BT (macro da tabela, personalizada ou nenhuma)
void openingsLutaBT() {
    const OpeningStep* sequencia = nullptr;

    if (modoBT == MODO_PERSONALIZADA) {
        SerialBT.println("//=====//ESTRATEGIA PERSONALIZADA INICIADA//=====//");
        sequencia = customOpening;

    } else if (macroIndex > 0 && macroIndex <= NUM_MACROS) {
        SerialBT.printf("//=====//%s INICIADO//=====//\n", NOMES_MACROS[macroIndex - 1]);
        
        if (macroIndex == 10) { // Curva de Borda
            executarCurvaDeBorda();
            return;
        } else {
            sequencia = (direction == direita ? TABELA_MACROS_DIR : TABELA_MACROS_ESQ)[macroIndex - 1];
        }
    }

    if (sequencia != nullptr) {
        executarOpening(sequencia);

    } else {                                  // Iterativo puro (inicia somente modo iterativo)
        SerialBT.println("//=====//ITERATIVO PURO INICIADO//=====//");
        if (estadoAsa == asaAberta) xTaskNotifyGive(openServoHandle);
        if (modoFurtivo) xTaskNotifyGive(swSensorHandle);
        moverMotores(0, 0);
    }
}

#pragma endregion
 
//===============================================================================================//
//===================================//CONFIG ESTRATEGIA NOVA//==================================//
//===============================================================================================//

#pragma region PERSONALIZACAO

// Interpreta e armazena o passo da estrategia personalizada
void parseAndStoreStep(const char* buffer) {
    int vel_esq, vel_dir, delay_ms;           // Variaveis para armazenar os valores parseados

    // Usa sscanf para parsear os tres inteiros separados por virgula
    int num_parsed = sscanf(buffer, "%d,%d,%d", &vel_esq, &vel_dir, &delay_ms);

    if (num_parsed != 3) {                    // Se nao tem 3 valores, o formato esta incorreto
        SerialBT.println("Formato invalido! Use: vel_esq,vel_dir,delay");
        Serial.println("Formato invalido! Use: vel_esq,vel_dir,delay");
        return;
    }

    if (customOpeningCount < (MAX_STEPS - 1)) {    // Verifica se ha espaco disponivel no array
        customOpening[customOpeningCount].speedLeft = vel_esq;
        customOpening[customOpeningCount].speedRight = vel_dir;
        customOpening[customOpeningCount].delayMs = delay_ms;
        customOpeningCount++;                // Incrementa o contador de passos
        
        Serial.printf("Passo adicionado: E:%d, D:%d, Delay:%dms\n", vel_esq, vel_dir, delay_ms);
        SerialBT.printf("Passo adicionado: E:%d, D:%d, Delay:%dms\n", vel_esq, vel_dir, delay_ms);
    } else {
        SerialBT.println("Limite de passos da estratégia atingido!");
        Serial.println("Limite de passos da estratégia atingido!");
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
