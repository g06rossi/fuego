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

void guardarRound();                          // auto_mode.hpp

// IR 3: freia primeiro, sinaliza depois. Nada que possa bloquear (LEDs, BT, flash) vem antes do freio
void stopRobot(void *pvParameters) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    travarMotores();                          // roboParado + running = false + freio

    ledsParada();                             // LED embutido aceso e enderecaveis vermelhos
    SerialBT.println("//=====//PAROU (IR 3): motores travados ate reiniciar//=====//");
    Serial.println("//=====//PAROU (IR 3): motores travados ate reiniciar//=====//");
    guardarRound();                           // Luta da RAM para a flash, com os motores ja travados

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

// Detector de linha de um sensor: limiar absoluto OU queda relativa a referencia do preto, com
// histerese na saida. A referencia so acompanha o sinal fora da linha
struct DetectorLinha {
    int32_t baseQ8 = -1;                      // Referencia do preto x256 (-1 = ainda sem leitura)
    bool naLinha = false;
};

volatile int baseLinhaE = 0;                  // Referencia do preto, para o Teste de sensor
volatile int baseLinhaD = 0;

bool atualizarLinha(DetectorLinha& detector, int adc) {
    if (detector.baseQ8 < 0) detector.baseQ8 = adc << 8;
    int base = detector.baseQ8 >> 8;

    int entrada = LINHA_TRESHOLD;
    if (LINHA_QUEDA > 0 && base - LINHA_QUEDA > entrada) entrada = base - LINHA_QUEDA;

    if (!detector.naLinha && adc < entrada) detector.naLinha = true;
    else if (detector.naLinha && adc > entrada + LINHA_HISTERESE) detector.naLinha = false;

    if (!detector.naLinha) detector.baseQ8 += ((adc << 8) - detector.baseQ8) >> LINHA_BASE_SHIFT;
    return detector.naLinha;
}

// Menor e maior ADC desde iniciarExtremos(), para o Teste de sensor sugerir limiares
volatile bool rastrearExtremos = false;
volatile int extMinLinhaE, extMaxLinhaE, extMinLinhaD, extMaxLinhaD, extMinLdr, extMaxLdr;

void iniciarExtremos() {
    rastrearExtremos = false;
    extMinLinhaE = extMinLinhaD = extMinLdr = 4095;
    extMaxLinhaE = extMaxLinhaD = extMaxLdr = 0;
    rastrearExtremos = true;
}

void atualizarExtremos(int linhaE, int linhaD, int ldr) {
    if (linhaE < extMinLinhaE) extMinLinhaE = linhaE;
    if (linhaE > extMaxLinhaE) extMaxLinhaE = linhaE;
    if (linhaD < extMinLinhaD) extMinLinhaD = linhaD;
    if (linhaD > extMaxLinhaD) extMaxLinhaD = linhaD;
    if (ldr < extMinLdr) extMinLdr = ldr;
    if (ldr > extMaxLdr) extMaxLdr = ldr;
}

/*
!INFO | Log de sensores
-----------------------------------
Cada amostra guarda o MENOR valor de cada sensor numa janela de LOG_JANELA_MS: uma passada pela
linha (~12 ms a 2 m/s) ou uma sombra no LDR nunca se perde, mesmo amostrando bem menos que o timer.
Tamanho: LOG_AMOSTRAS x 6 bytes = 4,8 KB, que cabe na NVS junto da configuracao e das iniciacoes
*/
#define LOG_JANELA_MS          10             //  AUTO | Janela de cada amostra do log (ms)
#define LOG_DURACAO_MS         8000           //  AUTO | Duracao do teste do log (ms)
#define LOG_AMOSTRAS           (LOG_DURACAO_MS / LOG_JANELA_MS)

struct AmostraLog {
    uint16_t linhaE;
    uint16_t linhaD;
    uint16_t ldr;
};

AmostraLog logSensores[LOG_AMOSTRAS];
volatile int logQtd = 0;
volatile bool logAtivo = false;
AmostraLog logJanela;
int64_t logInicioJanelaUs = 0;

void iniciarLog() {
    logAtivo = false;
    logQtd = 0;
    logJanela = {0xFFFF, 0xFFFF, 0xFFFF};
    logInicioJanelaUs = esp_timer_get_time();
    logAtivo = true;                          // Por ultimo: a analogSensorTask (nucleo 0) ja grava
}

void registrarLog(int linhaE, int linhaD, int ldr) {
    if (linhaE < logJanela.linhaE) logJanela.linhaE = linhaE;
    if (linhaD < logJanela.linhaD) logJanela.linhaD = linhaD;
    if (ldr < logJanela.ldr) logJanela.ldr = ldr;

    int64_t agora = esp_timer_get_time();
    if (agora - logInicioJanelaUs < LOG_JANELA_MS * 1000LL) return;

    logSensores[logQtd++] = logJanela;
    logJanela = {0xFFFF, 0xFFFF, 0xFFFF};
    logInicioJanelaUs = agora;
    if (logQtd >= LOG_AMOSTRAS) logAtivo = false;
}

/*
!INFO | Gravador de luta
-----------------------------------
Grava os primeiros GRAVADOR_DURACAO_MS da luta (contados do fim da iniciacao) na RAM, para exportar
em CSV depois do IR 3. Nada vai para a flash durante a luta: gravar nela desliga o cache dos dois
nucleos. Cada amostra resume uma janela de GRAVADOR_JANELA_MS: o MENOR ADC das linhas e do LDR, o
OU dos sensores digitais (viu pelo menos uma vez) e o ultimo PWM e estado da luta. As mudancas de
estado entram numa lista de eventos com o tempo do ciclo em que aconteceram
Tamanho: GRAVADOR_AMOSTRAS x 12 bytes = 36 KB, mais GRAVADOR_EVENTOS x 4 bytes = 2 KB
*/
#define GRAVADOR_JANELA_MS     10             //  AUTO | Janela de cada amostra da luta (ms)
#define GRAVADOR_DURACAO_MS    30000          //  AUTO | Tempo de luta gravado (ms)
#define GRAVADOR_AMOSTRAS      (GRAVADOR_DURACAO_MS / GRAVADOR_JANELA_MS)
#define GRAVADOR_EVENTOS       512            //  AUTO | Maximo de eventos gravados

// Bits do campo sensores da amostra
#define SENS_JS_E              0x01
#define SENS_IR_E              0x02
#define SENS_IR_D              0x04
#define SENS_JS_D              0x08
#define SENS_LINHA_E           0x10
#define SENS_LINHA_D           0x20
#define SENS_LDR               0x40

struct AmostraLuta {
    uint16_t linhaE;                          // Menor ADC da janela
    uint16_t linhaD;
    uint16_t ldr;
    int16_t pwmE;                             // Ultimo comando dos motores na janela
    int16_t pwmD;
    uint8_t sensores;                         // SENS_*: viu pelo menos uma vez na janela
    uint8_t estado;                           // modoLuta (bits 0-2), estadoFlanco (3-5), cargaTotal (6), ldrAtacando (7)
};

enum TipoEvento : uint8_t {
    evModo, evFlanco, evLinhaE, evLinhaD, evLdr, evCargaTotal, evLdrAtaque, evParada, evFim
};

struct EventoLuta {
    uint16_t tMs;                             // Desde o inicio da luta
    uint8_t tipo;                             // TipoEvento
    uint8_t valor;                            // Novo valor
};

AmostraLuta gravAmostras[GRAVADOR_AMOSTRAS];
EventoLuta gravEventos[GRAVADOR_EVENTOS];
volatile int gravQtd = 0;
volatile int gravEventosQtd = 0;
volatile bool gravAtivo = false;
volatile bool gravTerminou = false;           // Ha uma luta gravada para exportar
ModoLuta gravModoInicial = buscaOfensiva;     // Movimentacao escolhida (modoLutaOriginal no inicio)
AmostraLuta gravJanela;
int64_t gravInicioUs = 0;
int64_t gravFimJanelaUs = 0;
uint8_t gravEstadoAnterior = 0;
uint8_t gravSensoresAnterior = 0;

uint8_t estadoLutaAtual() {
    return (uint8_t)modoLuta | ((uint8_t)estadoFlanco << 3) | (cargaTotal << 6) | (ldrAtacando << 7);
}

void limparJanelaGravador() {
    gravJanela = {0xFFFF, 0xFFFF, 0xFFFF, 0, 0, 0, 0};
}

void registrarEvento(int64_t agora, uint8_t tipo, uint8_t valor) {
    if (gravEventosQtd >= GRAVADOR_EVENTOS) return;
    gravEventos[gravEventosQtd++] = {(uint16_t)((agora - gravInicioUs) / 1000), tipo, valor};
}

// Chamada pela openingsTask (nucleo 1) logo antes de ligar running
void iniciarGravador() {
    gravAtivo = false;
    gravQtd = 0;
    gravEventosQtd = 0;
    gravTerminou = false;
    gravModoInicial = modoLutaOriginal;
    limparJanelaGravador();
    gravEstadoAnterior = estadoLutaAtual();
    gravSensoresAnterior = 0;
    gravInicioUs = esp_timer_get_time();
    gravFimJanelaUs = gravInicioUs + GRAVADOR_JANELA_MS * 1000LL;
    registrarEvento(gravInicioUs, evModo, modoLuta);
    gravAtivo = true;                         // Por ultimo: a analogSensorTask (nucleo 0) ja grava
}

void encerrarGravador(int64_t agora, uint8_t motivo) {
    registrarEvento(agora, motivo, 0);
    gravTerminou = true;
    gravAtivo = false;
}

void registrarGravador(int linhaE, int linhaD, int ldr) {
    int64_t agora = esp_timer_get_time();
    if (roboParado) {
        encerrarGravador(agora, evParada);
        return;
    }

    // Fecha a janela. O fim avanca de um em um periodo para o tempo da amostra nao acumular atraso
    if (agora >= gravFimJanelaUs) {
        gravJanela.pwmE = novaVE;
        gravJanela.pwmD = novaVD;
        gravJanela.estado = estadoLutaAtual();
        gravAmostras[gravQtd++] = gravJanela;
        limparJanelaGravador();
        gravFimJanelaUs += GRAVADOR_JANELA_MS * 1000LL;
        if (gravQtd >= GRAVADOR_AMOSTRAS) {
            encerrarGravador(agora, evFim);
            return;
        }
    }

    uint8_t sensores = (value_JS_E  ? SENS_JS_E    : 0) | (value_IR_E  ? SENS_IR_E    : 0) |
                       (value_IR_D  ? SENS_IR_D    : 0) | (value_JS_D  ? SENS_JS_D    : 0) |
                       (value_QRE_E ? SENS_LINHA_E : 0) | (value_QRE_D ? SENS_LINHA_D : 0) |
                       (value_LDR   ? SENS_LDR     : 0);
    gravJanela.sensores |= sensores;
    if (linhaE < gravJanela.linhaE) gravJanela.linhaE = linhaE;
    if (linhaD < gravJanela.linhaD) gravJanela.linhaD = linhaD;
    if (ldr < gravJanela.ldr) gravJanela.ldr = ldr;

    uint8_t estado = estadoLutaAtual();
    uint8_t mudouEstado = estado ^ gravEstadoAnterior;
    if (mudouEstado & 0x07) registrarEvento(agora, evModo, estado & 0x07);
    if (mudouEstado & 0x38) registrarEvento(agora, evFlanco, (estado >> 3) & 0x07);
    if (mudouEstado & 0x40) registrarEvento(agora, evCargaTotal, (estado >> 6) & 1);
    if (mudouEstado & 0x80) registrarEvento(agora, evLdrAtaque, (estado >> 7) & 1);

    uint8_t mudouSensores = sensores ^ gravSensoresAnterior;
    if (mudouSensores & SENS_LINHA_E) registrarEvento(agora, evLinhaE, (sensores & SENS_LINHA_E) != 0);
    if (mudouSensores & SENS_LINHA_D) registrarEvento(agora, evLinhaD, (sensores & SENS_LINHA_D) != 0);
    if (mudouSensores & SENS_LDR)     registrarEvento(agora, evLdr, (sensores & SENS_LDR) != 0);

    gravEstadoAnterior = estado;
    gravSensoresAnterior = sensores;
}

// Diferenca minima entre preto e branco (ou claro e escuro) para valer sugerir um limiar
#define CONTRASTE_MINIMO       200

// Um limiar para as duas linhas: no meio entre o branco mais alto e o preto mais baixo dos dois
// sensores. Devolve -1 se nao houve contraste suficiente (ex.: um sensor nunca viu a linha)
int sugerirLimiarLinha(int brancoE, int pretoE, int brancoD, int pretoD) {
    int branco = max(brancoE, brancoD);
    int preto = min(pretoE, pretoD);
    if (preto - branco < CONTRASTE_MINIMO) return -1;
    return (branco + preto) / 2;
}

// LDR: no meio entre a sombra (escuro) e a luz ambiente (claro). -1 se nao houve sombra
int sugerirLimiarLdr(int escuro, int claro) {
    if (claro - escuro < CONTRASTE_MINIMO) return -1;
    return (escuro + claro) / 2;
}

void analogSensorTask(void *pvParameters) {
    DetectorLinha detectorE, detectorD;
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

        value_QRE_E = atualizarLinha(detectorE, linhaE);
        value_QRE_D = atualizarLinha(detectorD, linhaD);
        baseLinhaE = detectorE.baseQ8 >> 8;
        baseLinhaD = detectorD.baseQ8 >> 8;
        value_LDR = ldrFiltrado < LDR_TRESHOLD;

        if (rastrearExtremos) atualizarExtremos(linhaE, linhaD, ldrFiltrado);
        if (logAtivo) registrarLog(linhaE, linhaD, ldrFiltrado);
        if (gravAtivo) registrarGravador(linhaE, linhaD, ldrFiltrado);

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
        256 * 24,                             // Tamanho da pilha (guardarRound le os cabecalhos na pilha)
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