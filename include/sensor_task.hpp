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

#pragma endregion

//===============================================================================================//
//========================================//PARAR ROBO//=========================================//
//===============================================================================================//

#pragma region PARAR ROBO

// IR 3: freia primeiro, sinaliza depois. Nada que possa bloquear (LEDs, BT) vem antes do freio
void stopRobot(void *pvParameters) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    travarMotores();                          // roboParado + running = false + freio

    ledsParada();                             // LED embutido aceso e enderecaveis vermelhos
    SerialBT.println("//=====//PAROU (IR 3): motores travados ate reiniciar//=====//");
    Serial.println("//=====//PAROU (IR 3): motores travados ate reiniciar//=====//");

    for (;;) {
        brakeMotors();                        // Reforca o freio a cada 1 s
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void irMonitorTask(void *pvParameters) {      // Le o receptor IR a cada IR_LEITURA_MS
    // Iniciado aqui, e nao no setup (nucleo 1), para a ISR de 50 us do IRremote ficar no nucleo 0
    // e nao interromper a logica de combate
    IrReceiver.begin(IR_RECIEVE_PIN, DISABLE_LED_FEEDBACK, LED_PIN);

    for (;;) {
        if (IrReceiver.decode()) {
            uint16_t comando = IrReceiver.decodedIRData.command;
            IrReceiver.resume();

            if (comando == 0x2) {             // IR 3: parada definitiva
                xTaskNotifyGive(stopRobotHandle);
            } else {
                xQueueSend(irQueue, &comando, 0);   // Fila cheia: descarta (ninguem esta lendo)
                if (largadaLiberada) xTaskNotifyGive(IRCommandHandle);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(IR_LEITURA_MS));
    }
}

#pragma endregion

//===============================================================================================//
//====================================//LEITURA DOS SENSORES//===================================//
//===============================================================================================//

//======================================//Leitura Sensores//=====================================//

#pragma region LEITURA SENSOR

void IRAM_ATTR readSensors() {
    portENTER_CRITICAL_ISR(&sensorMux);   // Indica estrutura critica: prioridade de execucao

    value_JS_D = lerGPIO(JSUMO_DIR_PIN);
    value_JS_E = lerGPIO(JSUMO_ESQ_PIN);

    // IRs sao ativos em LOW: 0 no pino = adversario visto
    value_IR_D = !lerGPIO(IR_DIR_PIN);
    value_IR_E = !lerGPIO(IR_ESQ_PIN);

    portEXIT_CRITICAL_ISR(&sensorMux);    // Fim da estrutura critica

    if (value_JS_E || value_IR_E) ultimoLado = vistoEsquerda;
    else if (value_JS_D || value_IR_D) ultimoLado = vistoDireita;

    // Variavel para verificar se uma tarefa de maior prioridade foi despertada
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Dispara a leitura de linha e LDR; ela acorda a logica de combate quando termina
    vTaskNotifyGiveFromISR(analogSensorHandle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {           // Se uma tarefa de maior prioridade foi despertada
        portYIELD_FROM_ISR();                 // Forca desligamento do robo (prioridade maior)
    }
}

#pragma endregion

//=====================================//Sensores Analogicos//===================================//

#pragma region SENSORES ANALOGICOS

/*
!INFO | Linha e LDR analogicos
-----------------------------------
O ADC nao pode ser lido dentro da ISR, por isso linha e LDR sao lidos nesta task, que a ISR acorda
a cada ciclo do timer. Ao terminar, ela acorda a fightingLogicTask, que assim sempre decide com as
leituras do ciclo atual. Linha: ADC abaixo de LINHA_TRESHOLD = linha branca. LDR: media movel de
LDR_JANELA_FILTRO amostras (atenua o ruido dos motores) e abaixo de LDR_TRESHOLD = sombra do
adversario na rampa. Os pinos precisam ser do ADC1 (GPIO 32-39)

Nao use analogRead aqui: no core Arduino 2.x ele refaz pinMode e atenuacao a cada chamada, e 3
leituras passam de 300us. A task entao ocupa 100% do nucleo 0, a IDLE0 nao roda e o watchdog
de tarefas reinicia o ESP. O ADC e configurado uma vez e lido com adc1_get_raw

O timer dos sensores e ligado por esta task (nucleo 0), e nao pelo setup (nucleo 1): a ISR fica no
mesmo nucleo da task que ela acorda, e o nucleo 1 fica so com o combate
*/
void startTimer();

void analogSensorTask(void *pvParameters) {
    adc1_channel_t canalLinhaE = (adc1_channel_t)digitalPinToAnalogChannel(LINHA_ESQ_PIN);
    adc1_channel_t canalLinhaD = (adc1_channel_t)digitalPinToAnalogChannel(LINHA_DIR_PIN);
    adc1_channel_t canalLdr    = (adc1_channel_t)digitalPinToAnalogChannel(LDR_PIN);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(canalLinhaE, ADC_ATTEN_DB_12);   // Mesma escala do analogRead padrao
    adc1_config_channel_atten(canalLinhaD, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(canalLdr, ADC_ATTEN_DB_12);
    adc_power_acquire();                      // ADC sempre ligado (evita glitch nos GPIO 36 e 39)

    uint16_t amostrasLdr[LDR_JANELA_FILTRO];
    uint16_t inicial = adc1_get_raw(canalLdr); // Preenche o filtro para nao disparar no inicio
    for (int i = 0; i < LDR_JANELA_FILTRO; i++) amostrasLdr[i] = inicial;
    uint32_t somaLdr = (uint32_t)inicial * LDR_JANELA_FILTRO;
    int indiceLdr = 0;

    // O handle precisa existir antes da primeira ISR: aqui a task pode rodar antes de o
    // xTaskCreatePinnedToCore que a criou devolver o handle
    analogSensorHandle = xTaskGetCurrentTaskHandle();
    startTimer();

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // Acordada pela ISR dos sensores
        int64_t inicioCiclo = esp_timer_get_time();

        int linhaE = adc1_get_raw(canalLinhaE);
        int linhaD = adc1_get_raw(canalLinhaD);

        somaLdr -= amostrasLdr[indiceLdr];
        amostrasLdr[indiceLdr] = adc1_get_raw(canalLdr);
        somaLdr += amostrasLdr[indiceLdr];
        indiceLdr = (indiceLdr + 1) % LDR_JANELA_FILTRO;
        int ldrFiltrado = somaLdr / LDR_JANELA_FILTRO;

        adc_QRE_E = linhaE;
        adc_QRE_D = linhaD;
        adc_LDR = ldrFiltrado;

        value_QRE_E = linhaE < LINHA_TRESHOLD;
        value_QRE_D = linhaD < LINHA_TRESHOLD;
        value_LDR = ldrFiltrado < LDR_TRESHOLD;

        // Com as leituras deste ciclo prontas, acorda a logica de combate (so durante a luta)
        if (running) xTaskNotifyGive(fightingLogicHandle);

        uint32_t duracao = (uint32_t)(esp_timer_get_time() - inicioCiclo);
        if (duracao > adcCicloMaxUs) adcCicloMaxUs = duracao;
        // Protecao: se o ciclo ficou longo demais, cede o nucleo 0 para a IDLE0 alimentar o watchdog
        if (duracao > ADC_CICLO_LIMITE_US) vTaskDelay(pdMS_TO_TICKS(1));
    }
}

#pragma endregion

//================================//Interpreta controle remoto//=================================//

#pragma region INTERPRETA IR

// Acordada pela irMonitorTask a cada comando IR, so depois de o modoAUTO liberar a largada
void handleIRCommand(void *pvParameters) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint16_t comandoAtual;
        while (largadaLiberada && receberComandoIR(comandoAtual)) {
            // Indica estar pronto para AUTO se recebe 1 (0x0) do controle
            if (comandoAtual == 0x0) {
                Serial.println("Pronto");
                SerialBT.println("Pronto");
                ready = true;                 // Pronto para iniciar a movimentacao
                AnnihilationModeLeds();       // LEDs vermelhos para a sede de ser campeao
                blinkLED(1, 25);              // Pisca o LED builtin se recebe IR 1
            }

            // Inicia movimento AUTO se recebe 2 (0x1) do controle. Depois de um IR 3, nunca mais
            if (comandoAtual == 0x1 && ready && !roboParado) {
                // Notifica a task de estrategia
                clearLeds();                  // Apaga LEDs enderecaveis
                largadaLiberada = false;      // Nao recebe mais comandos: fica bloqueada para sempre
                xTaskNotifyGive(openingsHandle);
            }
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
        //    - FIM_TEMPO: apos tempoFinalizacaoMs desde o fim da iniciacao muda de vez para Busca Ofensiva
        if (finalization == FIM_TEMPO) {
            if (modoLuta != buscaOfensiva && modoLuta != retorno && modoLuta != desengate) {
                if ((millis() - tempoFighting) >= tempoFinalizacaoMs) {
                    modoLuta = buscaOfensiva;
                    modoLutaOriginal = buscaOfensiva;
                }
            }
        }

        // RETORNO DE LINHA | Prioridade sobre tudo, inclusive o ataque por LDR: um falso positivo
        // do LDR nao pode empurrar o robo para fora do dohyo
        if (value_QRE_E || value_QRE_D) {
            if (modoLuta != retorno) {
                if (value_QRE_E && !value_QRE_D) viuLinha = linhaESQ;
                else if (value_QRE_D && !value_QRE_E) viuLinha = linhaDIR;
                else viuLinha = linhaAMBAS;
                modoLuta = retorno;
            }
        }

        // DESENGATE E ATAQUE LDR (nao age durante o Retorno)
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
    timerAlarmWrite(sensorTimer, PERIODO_SENSORES_US, true);  // Periodo em us (configuravel via NVS)
    timerAlarmEnable(sensorTimer);            // Ligar o timer
}

#pragma endregion

//======================================//Task de alocacao//=====================================//

#pragma region TASK ALOCACAO

void setupSensorTask() {
    irQueue = xQueueCreate(IR_FILA_TAMANHO, sizeof(uint16_t));   // Antes da irMonitorTask

    xTaskCreatePinnedToCore(                  // Parar o robo
        stopRobot,                            // Funcao da tarefa
        "StopRobot",                          // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        17,                                   // Prioridade 17: a maior das tasks do projeto
        &stopRobotHandle,                     // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );
    
    xTaskCreatePinnedToCore(                  // Monitorar o sinal IR para parar
        irMonitorTask,                        // Funcao da tarefa
        "IR_Monitor",                         // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        15,                                   // Prioridade 15 (abaixo da leitura de linha)
        NULL,                                 // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );
    
    xTaskCreatePinnedToCore(                  // Le linha e LDR (analogicos) e liga o timer dos sensores
        analogSensorTask,                     // Funcao da tarefa
        "AnalogSensor",                       // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        16,                                   // Prioridade 16: a linha e a leitura mais critica
        NULL,                                 // Handle: a propria task preenche antes de ligar o timer
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
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif