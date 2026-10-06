//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef MOVE_H
#define MOVE_H

#include <defines.hpp>                        // Definicoes globais
#include <functions.hpp>                      // Funcoes auxiliares

#pragma endregion

//===============================================================================================//
//========================================//MOVIMENTACAO//=======================================//
//===============================================================================================//

//=======================================//Mover Motores//=======================================//

#pragma region ENVIAR PWM

// Define o duty cycle (velocidade) para os motores esquerdo e direito
void setMotors(int velocidadeEsquerdaMotor, int velocidadeDireitaMotor) {

    portENTER_CRITICAL(&motorMux);            // Indica estrutura critica: prioridade de execucao

    // Logica para o Motor Esquerdo
    if (velocidadeEsquerdaMotor >= 0) {
        // Gira para frente: PWM no pino POS, pino NEG em 0
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL, velocidadeEsquerdaMotor);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL, 0);
    } else {                                  // Inverte o valor para ser positivo
        // Gira para tras: pino POS em 0, PWM no pino NEG
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL, -velocidadeEsquerdaMotor);
    }

    // Logica para o Motor Direito
    if (velocidadeDireitaMotor >= 0) {
        // Gira para frente: PWM no pino POS, pino NEG em 0
        ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL, velocidadeDireitaMotor);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL, 0);
    } else {                                  // Inverte o valor para ser positivo
        // Gira para tras: pino POS em 0, PWM no pino NEG
        ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL, 0);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL, -velocidadeDireitaMotor);
    }

    // Aplica as alteracoes de duty cycle em todos os canais
    // Isso garante que as mudancas ocorram de forma sincrona
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL);

    portEXIT_CRITICAL(&motorMux);             // Fim da estrutura critica

    // Envia informacoes da movimentacao para debug via Bluetooth e Serial
    SerialBT.printf("MOTOR -> D: %d E: %d\n", novaVD, novaVE);
    Serial.printf("MOTOR -> D: %d E: %d\n", novaVD, novaVE);
}

#pragma endregion

//=======================================//Parar Motores//=======================================//

#pragma region PARAR PWM

// Para os motores zerando o PWM e depois os pinos
void stopMotors() {
    portENTER_CRITICAL(&motorMux);            // Indica estrutura critica: prioridade de execucao

    ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL, 0);

    ledc_update_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL);

    portEXIT_CRITICAL(&motorMux);             // Fim da estrutura critica

    delayUs(2 * PWM_ZERO_DELAY);              // Pequeno delay para garantir que os PWMs zerem
}

void brakeMotors() {
    portENTER_CRITICAL(&motorMux);            // Indica estrutura critica: prioridade de execucao

    ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL, PWM_FREIO);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL, PWM_FREIO);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL, PWM_FREIO);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL, PWM_FREIO);

    ledc_update_duty(LEDC_LOW_SPEED_MODE, RIGHT_POS_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, RIGHT_NEG_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEFT_POS_CHANNEL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEFT_NEG_CHANNEL);

    portEXIT_CRITICAL(&motorMux);             // Fim da estrutura critica

    vTaskDelay(pdMS_TO_TICKS(1));             // Delay para garantir que o robo para de fato
}

#pragma endregion

//==============================//Processar Movimento dos Motores//==============================//

#pragma region PROCESSAR PWM

void processarMovimento(int velocidadeEsquerdaMotor, int velocidadeDireitaMotor) {
    static int ultimaVE = 0;
    static int ultimaVD = 0;

    // Caso (0, 0): aplica freio ativo
    if (velocidadeEsquerdaMotor == 0 && velocidadeDireitaMotor == 0) {
        brakeMotors();
        SerialBT.println("FREIO");
        Serial.println("FREIO");

    // Caso de troca de polaridade: zera PWM antes de inverter
    } else if (
        (ultimaVE > 0 && velocidadeEsquerdaMotor < 0) || 
        (ultimaVE < 0 && velocidadeEsquerdaMotor > 0) ||
        (ultimaVD > 0 && velocidadeDireitaMotor < 0) || 
        (ultimaVD < 0 && velocidadeDireitaMotor > 0)
    ) {
        stopMotors();
        setMotors(velocidadeEsquerdaMotor, velocidadeDireitaMotor);

    // Caso padrao: mantem fluidez
    } else {
        setMotors(velocidadeEsquerdaMotor, velocidadeDireitaMotor);
    }

    // Atualiza os valores das ultimas velocidades
    ultimaVE = velocidadeEsquerdaMotor;
    ultimaVD = velocidadeDireitaMotor;
}

#pragma endregion

//=======================================//Funcao Chamada//======================================//

#pragma region FUNCAO GLOBAL

void moverMotores(int velocidadeEsquerdaMotor, int velocidadeDireitaMotor) {
    static int ultimaVE = 0;
    static int ultimaVD = 0;

    // So notifica a processarMovimento se a velocidade realmente mudou
    if (velocidadeEsquerdaMotor != ultimaVE || velocidadeDireitaMotor != ultimaVD) {
        novaVE = velocidadeEsquerdaMotor;
        novaVD = velocidadeDireitaMotor;
        processarMovimento(novaVE, novaVD);
    }

    // Atualiza os valores das ultimas velocidades
    ultimaVE = velocidadeEsquerdaMotor;
    ultimaVD = velocidadeDireitaMotor;
}

#pragma endregion

//=========================================//Servomotor//========================================//

#pragma region SERVOMOTOR

// Converte um angulo (0-180) para o valor de duty cycle correspondente
uint32_t angleToDuty(int angle) {
    long pulse_width_us = map(angle, 0, 180, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    long period_us = 1000000 / SERVO_FREQ_HZ;

    // Calcula a largura do pulso enviada para o servomotor
    uint32_t duty = (uint32_t)(((double)pulse_width_us / 
        (double)period_us) * (double)(1 << SERVO_RESOLUTION));

    return duty;
}

/* 
!INFO | Abertura do servomotor
-----------------------------------
A forma mais rapida de abrir o servomotor aqui seria definir o angulo como 180°, pois gera o maior
"sinal de erro" interno possivel entre a posicao atual e a de destino. Em resposta, o servo aplica 
potencia maxima ao seu motor para corrigir esse erro, resultando na maior velocidade de rotacao 
durante todo o percurso.

A limitacao do movimento se da pelos limitadores fisicos e a limitacao do tempo de acionamento se
da pelo delay entre a abertura e o relaxamento do servo
*/
void openServo(void *pvParameters) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        estadoAsa = asaAberta;
        
        ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL, angleToDuty(180));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(150));       // Tempo para garantir mov 90 graus. Datasheet -> 150ms

        ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL);
    }
}

// Fecha o servomotor
void closeServo(void *pvParameters) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        estadoAsa = asaFechada;

        // Fecha o servo
        ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL, angleToDuty(90));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(150));       // Tempo para garantir mov 90 graus. Datasheet -> 150ms

        ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_LEDC_CHANNEL);
    }
}

#pragma endregion

//===============================================================================================//
//===========================================//SETUP//===========================================//
//===============================================================================================//

//=====================================//Setup dos motores//=====================================//

#pragma region SETUP MOTORES

void setupLEDC() {
    // Configurar o Timer do LEDC (um timer pode servir para varios canais)
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum)
        .duty_resolution = (ledc_timer_bit_t)PWM_RESOLUTION, // Resolucao do duty cycle (8 bits)
        .timer_num       = LEDC_TIMER_3,      // Usa o Timer 3 do LEDC
        .freq_hz         = PWM_FREQ,          // Frequencia do PWM em Hz
        .clk_cfg         = LEDC_AUTO_CLK      // Configuracao automatica do clock
    };
    ledc_timer_config(&ledc_timer);           // Aplica a configuracao do timer

    // Configurar cada pino/canal individualmente
    // Array com as configuracoes para simplificar
    ledc_channel_config_t ledc_channels[4] = {
        { .gpio_num      = RIGHT_POS_PIN,     // Pino para motor direito, sentido positivo
          .speed_mode    = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum)
          .channel       = RIGHT_POS_CHANNEL, // Canal para motor direito, sentido positivo
          .timer_sel     = LEDC_TIMER_3,      // Usa o Timer 3 do LEDC
          .duty          = 0,                 // Valor inicial do duty cycle
          .hpoint        = 0                  // Deslocamento de fase
        },

        { .gpio_num      = RIGHT_NEG_PIN,     // Pino para motor direito, sentido negativo
          .speed_mode    = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum)
          .channel       = RIGHT_NEG_CHANNEL, // Canal para motor direito, sentido negativo
          .timer_sel     = LEDC_TIMER_3,      // Usa o Timer 3 do LEDC
          .duty          = 0,                 // Valor inicial do duty cycle
          .hpoint        = 0                  // Deslocamento de fase
        },

        { .gpio_num      = LEFT_POS_PIN,      // Pino para motor esquerdo, sentido positivo
          .speed_mode    = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum)
          .channel       = LEFT_POS_CHANNEL,  // Canal para motor esquerdo, sentido positivo
          .timer_sel     = LEDC_TIMER_3,      // Usa o Timer 3 do LEDC
          .duty          = 0,                 // Valor inicial do duty cycle
          .hpoint        = 0                  // Deslocamento de fase
        },
          
        { .gpio_num      = LEFT_NEG_PIN,      // Pino para motor esquerdo, sentido negativo
          .speed_mode    = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum) 
          .channel       = LEFT_NEG_CHANNEL,  // Canal para motor esquerdo, sentido negativo
          .timer_sel     = LEDC_TIMER_3,      // Usa o Timer 3 do LEDC
          .duty          = 0,                 // Valor inicial do duty cycle
          .hpoint        = 0                  // Deslocamento de fase
        }
    };
    
    // Aplica a configuracao de cada canal
    for (int i = 0; i < 4; i++) {
        ledc_channel_config(&ledc_channels[i]);
    }
}

#pragma endregion

//===================================//Setup dos servomotores//==================================//

#pragma region SETUP SERVOMOTOR

void setupLEDC_Servo() {
    // Configurar o Timer do LEDC (um timer pode servir para varios canais)
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum)
        .duty_resolution = (ledc_timer_bit_t)SERVO_RESOLUTION, // Resolucao do duty cycle (16 bits)
        .timer_num       = SERVO_TIMER,       // Usa o Timer 0 do LEDC
        .freq_hz         = SERVO_FREQ_HZ,     // Frequencia do PWM em Hz
        .clk_cfg         = LEDC_AUTO_CLK      // Configuracao automatica do clock
    };
    ledc_timer_config(&ledc_timer);           // Aplica a configuracao do timer

    ledc_channel_config_t canal_servo = {
        .gpio_num        = SERVOMOTOR_PIN,    // Pino para o servomotor
        .speed_mode      = LEDC_LOW_SPEED_MODE, // Modo de velocidade baixa (para PWM comum)
        .channel         = SERVO_LEDC_CHANNEL, // Canal para o servomotor
        .timer_sel       = SERVO_TIMER,       // Usa o Timer 0 do LEDC
        .duty            = 0,                 // Valor inicial do duty cycle
        .hpoint          = 0                  // Deslocamento de fase
    };
    ledc_channel_config(&canal_servo);        // Aplica a configuracao
}

#pragma endregion

//======================================//Task de alocacao//=====================================//

#pragma region TASK ALOCACAO

void setupMoveTask() {

    setupLEDC();                              // Configuracao do LEDC dos motores
    setupLEDC_Servo();                        // Configuracao do LEDC do servomotor

    xTaskCreatePinnedToCore(                  // Abre o servomotor
        openServo,                            // Funcao da tarefa
        "OpenServo",                          // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        15,                                   // Prioridade 15
        &openServoHandle,                     // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );

    xTaskCreatePinnedToCore(                  // Fecha o servomotor
        closeServo,                           // Funcao da tarefa
        "CloseServo",                         // Nome da tarefa
        256 * 16,                             // Tamanho da pilha
        nullptr,                              // Parametros
        15,                                   // Prioridade 15
        &closeServoHandle,                    // Handle
        PRO_CPU_NUM                           // Nucleo 0 onde a tarefa sera executada
    );
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif
