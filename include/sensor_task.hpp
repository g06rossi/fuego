//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include <defines.hpp>                        // Definicoes globais
#include <functions.hpp>                      // Funcoes auxiliares
#include <move.hpp>                           // Funcoes de movivmentacao de motores
#include <fighting.hpp>                       // Loop pos estretegia inicial do modo AUTO

bool gndLigado = true;

#pragma endregion

//===============================================================================================//
//========================================//PARAR ROBO//=========================================//
//===============================================================================================//

#pragma region PARAR ROBO

void stopRobot(void *pvParameters) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for(;;) {
        SerialBT.println("PAROU");
        running = false;                      // Para os loops de Defensivo e Ofensivo
        brakeMotors();                        // Trava os motores
        vTaskDelay(pdMS_TO_TICKS(1000));      // Delay para garantir a parada
    }
}

void irMonitorTask(void *pvParameters) {      // Monitora a cada 100 ms se o robo deve parar 
    for (;;) {
        if (IrReceiver.decode()) {
            // Armazena o comando decodificado em uma variavel global
            ultimoComandoIR = IrReceiver.decodedIRData.command;
            // Se recebe IR 3 (0x2), notifica a task de parada
            if (ultimoComandoIR == 0x2) {
                blinkLED(8, 25);
                xTaskNotifyGive(stopRobotHandle);
            }
            IrReceiver.resume();
        }
        vTaskDelay(pdMS_TO_TICKS(100));       // 100 ms para as outras tarefas serem executadas
    }
}

#pragma endregion

//===============================================================================================//
//====================================//LEITURA DOS SENSORES//===================================//
//===============================================================================================//

//======================================//Desliga Sensores//=====================================//

#pragma region DESLIGA SENSOR

void switchSensor(void *pvParameters) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gndLigado) {                    // Desliga o transistor e desconecta GND dos JSumos
            GPIO.out_w1tc = ((uint32_t)1 << NMOS_PIN);
            gndLigado = false;
        } else {                              // Liga o transistor e conecta GND dos JSumos
            GPIO.out_w1ts = ((uint32_t)1 << NMOS_PIN);
            gndLigado = true;
        }
    }
}

#pragma endregion

//======================================//Leitura Sensores//=====================================//

#pragma region LEITURA SENSOR

void IRAM_ATTR readSensors() {
    portENTER_CRITICAL_ISR(&sensorMux);   // Indica estrutura critica: prioridade de execucao

    value_JS_D = ((GPIO.in >> JSUMO_DIR_PIN) & 0x1);
    value_JS_E = ((GPIO.in >> JSUMO_ESQ_PIN) & 0x1);

    value_IR_D = ((GPIO.in >> IR_DIR_PIN) & 0x1);
    value_IR_E = ((GPIO.in >> IR_ESQ_PIN) & 0x1);

    value_QRE_D = ((GPIO.in1.val >> (LINHA_DIR_PIN - 32)) & 0x1);
    value_QRE_E = ((GPIO.in1.val >> (LINHA_ESQ_PIN - 32)) & 0x1);

    value_LDR = ((GPIO.in1.val >> (LDR_PIN - 32)) & 0x1);

    portEXIT_CRITICAL_ISR(&sensorMux);    // Fim da estrutura critica

    if (value_JS_E || value_IR_E) ultimoLado = vistoEsquerda;
    else if (value_JS_D || value_IR_D) ultimoLado = vistoDireita;

    // Variavel para verificar se uma tarefa de maior prioridade foi despertada
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Notifica a SensorTask
    vTaskNotifyGiveFromISR(fightingLogicHandle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {           // Se uma tarefa de maior prioridade foi despertada
        portYIELD_FROM_ISR();                 // Forca desligamento do robo (prioridade maior)
    }
}

#pragma endregion   

//================================//Interpreta controle remoto//=================================//

#pragma region INTERPRETA IR

void handleIRCommand(void *pvParameters) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Aguarda notificacao
    for (;;) {
        if (ultimoComandoIR != 0xFFFF) {      // Verifica se há um comando IR recebido
            uint16_t comandoAtual = ultimoComandoIR;
            ultimoComandoIR = 0xFFFF;

            // Indica estar pronto para AUTO se recebe 1 (0x0) do controle
            if (comandoAtual == 0x0) {
                Serial.println("Pronto");
                SerialBT.println("Pronto");
                ready = true;                 // Pronto para iniciar a movimentacao
                AnnihilationModeLeds();       // LEDs vermelhos para a sede de ser campeao
                blinkLED(1, 25);              // Pisca o LED builtin se recebe IR 1
            }

            // Inicia movimento AUTO se recebe 2 (0x1) do controle
            if (comandoAtual == 0x1 && ready) {
                // Notifica a task de estrategia
                clearLeds();                  // Apaga LEDs enderecaveis
                xTaskNotifyGive(openingsHandle);
                vTaskDelete(NULL);            // Encerra esta task
            }
            IrReceiver.resume();              // Limpa o buffer IR
        }
    }
}

#pragma endregion

//===============================================================================================//
//=====================================//LOGICA DE COMBATE//=====================================//
//===============================================================================================//

#pragma region LOGICA DE COMBATE

void fightingLogicTask(void *pvParameters) {
    for (;;) {                                // Define a logica de combate que sera ativada
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(!running) continue;                // Trava de seguranca da logica

        // FINALIZACAO | A movimentacao escolhida acaba e entra no modo de Busca Ofensiva:
        //    - FIM_TEMPO: apos FINAL_TEMPO_MS desde o fim da iniciacao muda de vez para Busca Ofensiva
        if (finalization == FIM_TEMPO) {
            if (modoLuta != buscaOfensiva && modoLuta != retorno && modoLuta != desengate) {
                if ((millis() - tempoFighting) >= FINAL_TEMPO_MS) {
                    modoLuta = buscaOfensiva;
                    modoLutaOriginal = buscaOfensiva;
                }
            }
        }

        // DESENGATE E ATAQUE LDR
        if (finalization == FIM_LDR) {
            if (value_LDR) {
                ldrAtacando = true;
                if (modoLuta != retorno) {
                    moverMotores(255, 255);
                    continue; // Pula o switch case e ataca direto
                }
            } else if (ldrAtacando) {
                ldrAtacando = false;
                if (modoLuta != retorno && modoLuta != desengate) {
                    modoLuta = desengate;
                }
            }
        }

        // RETORNO DE LINHA | Interrompe qualquer movimento se a linha for detectada
        if (value_QRE_E || value_QRE_D) {
            if (modoLuta != retorno) {
                if (value_QRE_E && !value_QRE_D) viuLinha = linhaESQ;
                else if (value_QRE_D && !value_QRE_E) viuLinha = linhaDIR;
                else viuLinha = linhaAMBAS;
                modoLuta = retorno;
            }
        }

//=======================================//Seleciona Modo//======================================//

        switch (modoLuta) {
            case retorno:
                modoRetorno();                // FSM | Evita sair do dohyo
                break;
            case desengate:
                modoDesengate();              // FSM | Desengate em S apos LDR
                break;
            case buscaOfensiva:
                modoBuscaOfensiva();          // Ataca o adversario de forma rapida
                break;
            case buscaDefensiva:
                modoBuscaDefensiva();         // Ataca o adversario devagar
                break;
            case buscaLinha:
                modoBuscaLinha();             // FSM | Acompanha o adversario
                break;
            case buscaPulsada:
                modoBuscaPulsada();           // FSM | Acompanha o adversario com passos
                break;
            default:
                modoBuscaOfensiva();
                break;
        }
    }
}

/*
!INFO | FSM (Finite State Machine) Cooperativa Nao-Bloqueante
-----------------------------------
A logica dos modos complexos segue uma Maquina de Estados Finitos (FSM) cooperativa. A transicao 
entre os estados e baseada em tempo ('millis()') ao inves de delays, garantindo que a funcao de 
modo retorne imediatamente a cada tick

Isso mantem a latencia do loop de controle principal proxima de zero, permitindo preempcao 
instantanea da estrategia atual por uma de maior prioridade baseada em novas leituras dos sensores
*/

#pragma endregion

//===============================================================================================//
//===========================================//SETUP//===========================================//
//===============================================================================================//

//===========================================//Timer//===========================================//

#pragma region SETUP TIMER

void startTimer() {
    sensorTimer = timerBegin(0, 80, true);    // Timer de 80 ticks
    timerAttachInterrupt(sensorTimer, &readSensors, true); // Qual funcao sera acordada
    timerAlarmWrite(sensorTimer, 300, true);  // Definir tempo (µs) aqui
    timerAlarmEnable(sensorTimer);            // Ligar o timer
}

#pragma endregion

//======================================//Task de alocacao//=====================================//

#pragma region TASK ALOCACAO

void setupSensorTask() {
    xTaskCreatePinnedToCore(                  // Parar o robo
        stopRobot,                            // Funcao da tarefa
        "StopRobot",                          // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        17,                                   // Prioridade 16
        &stopRobotHandle,                     // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );
    
    xTaskCreatePinnedToCore(                  // Monitorar o sinal IR para parar
        irMonitorTask,                        // Funcao da tarefa
        "IR_Monitor",                         // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        16,                                   // Prioridade 16
        NULL,                                 // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );
    
    xTaskCreatePinnedToCore(                  // Ligar e desligar os sensores
        switchSensor,                         // Funcao da tarefa
        "SwitchSensor",                       // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        15,                                   // Prioridade 15
        &swSensorHandle,                      // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );
    
    xTaskCreatePinnedToCore(                  // Monitorar o sinal IR para iniciar a luta
        handleIRCommand,                      // Funcao da tarefa
        "HandleIRCommand",                    // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        14,                                   // Prioridade 14
        &IRCommandHandle,                     // Handle
        APP_CPU_NUM                           // Nucleo 1 onde a tarefa sera executada
    ); 
    
    xTaskCreatePinnedToCore(                  // Lida com a logica de combate
        fightingLogicTask,                      // Funcao da tarefa
        "SensorTask",                         // Nome da tarefa
        256 * 32,                             // Tamanho da pilha
        nullptr,                              // Parametros
        14,                                   // Prioridade 14
        &fightingLogicHandle,                   // Handle
        APP_CPU_NUM                           // Nucleo 1 onde a tarefa sera executada
    );
    
    startTimer();                             // Configurar o timer dos sensores
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif