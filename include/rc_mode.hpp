//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef RC_MODE_H
#define RC_MODE_H

#include <PS4Controller.h>                    // Biblioteca do controle do PS4

#include <defines.hpp>                        // Definicoes globais
#include <functions.hpp>                      // Funcoes auxiliares
#include <move.hpp>                           // Funcoes de movivmentacao de motores
#include <boot_mode.hpp>                      // NVS do modo de boot e senha IR

#define coefAtenuacao          1.8            // Atenuacao da velocidade em curvas (deve ser FLOAT)
#define coefReverse            0.9            // Coeficiente para balancear a re (deve ser FLOAT)
#define limiteCurva            154.0          // Velocidade limite em curvas puras (deve ser FLOAT)

volatile bool switchCxV        = false;       // Switch definido para controlar MACRO usado
volatile bool velLimitada      = false;       // Switch definido pra limitar a velocidade do motor
int limiteVelocidade           = 255;         // Limite de velocidade do motor
int zonaMorta                  = 10;          // Zona desconsiderada do controle de PS4
volatile int r2                = 0;           // Valor do gatilho direito do PS4 (R2)
volatile int l2                = 0;           // Valor do gatilho esquerdo do PS4 (L2)
volatile int direcao           = 0;           // Valor do direcional esquerdo do PS4 em X (LStickX)

bool primeiraAceleracao        = true;        // Indica que o robo nunca andou para frente

/*
!INFO | Ultimos MAC Addresses conhecidos dos robos
-----------------------------------
*/

const char* SMOKER             = "b0:cb:d8:04:6e:4c";
const char* FUMACINHA          = "d4:e9:f4:e3:64:90";
const char* ARRUELA            = "98:83:89:e8:11:75";
const char* BRIGA              = "5c:01:3b:74:15:20";
const char* FUEGO              = "d4:e9:f4:e3:64:90";
const char* FUEGUITO           = "8c:4f:00:3d:27:00";
const char* RESSACA            = "5c:96:66:c5:63:55";
const char* SHENLONG           = "78:9b:3c:f6:29:fc";
const char* TSUNAMI            = "f4:2d:c9:59:df:f1";
const char* CAIPORA            = "a4:f0:0f:69:bc:c0";

#pragma endregion

//===============================================================================================//
//===========================================//SETUP//===========================================//
//===============================================================================================//

#pragma region SETUP

void modoRC() {
    // Tenta conectar ao controle PS4 e debuga conexao pelo Serial Monitor
    if (PS4.begin(FUEGO)) {
        Serial.println("Bluetooth inicializado, aguardando controle...");
        xTaskNotifyGive(swSensorHandle);      // Entra no modo furtivo

        while (!PS4.isConnected()) {
            directWriteHigh(LED_PIN);
            bootSenhaProcessar();             // Janela da senha de boot por IR
            vTaskDelay(pdMS_TO_TICKS(10));    // Delay para o FreeRTOS
        }
    }

    bootSenhaCancelar();                      // Controle conectado: fecha a janela da senha
    Serial.println("Controle conectado!");
    PS4.setLed(0, 200, 0);                    // Define a cor do LED para verde
    setLeds(0, 200, 0,                        // LED 1
            0, 200, 0,                        // LED 2
            0, 200, 0,                        // LED 3
            0, 200, 0,                        // LED 4
            0, 200, 0);                       // LED 5

    directWriteLow(LED_PIN);

    while (PS4.isConnected()) {
        
        int velocidadeEsquerda = 0;           // Velocidade do motor esquerdo
        int velocidadeDireita = 0;            // Velocidade do motor direito

#pragma endregion

//===============================================================================================//
//==========================================//BOTOES//===========================================//
//===============================================================================================//

//=========================//Turbo para frente: IMPEDE OUTRAS INTERACOES//=======================//

#pragma region TURBO

        if (PS4.Cross()) {
            moverMotores(255, 255);
            continue;

#pragma endregion

//=================================//Switch Estretegia Inicial//=================================//

#pragma region SWITCH ESTRATEGIA

        // Permite movimentacao em V
        } else if (PS4.Circle() && !switchCxV) {
            vTaskDelay(pdMS_TO_TICKS(50));
            switchCxV = true;
            vTaskDelay(pdMS_TO_TICKS(200));   // Evita multiplas leituras
        
        // Permite movimentacao em C
        } else if (PS4.Circle()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            switchCxV = false;
            vTaskDelay(pdMS_TO_TICKS(200));   // Evita multiplas leituras

#pragma endregion

//==============================//Limitador da potencia dos motores//============================//

#pragma region LIMITADOR
        
        // Limita a velocidade a 180
        } else if (PS4.Triangle() && !velLimitada) {
            velLimitada = true;
            limiteVelocidade = 180;           // Define o limite reduzido
            vTaskDelay(pdMS_TO_TICKS(200));   // Evita multiplas leituras
        
        // Permite a velocidade de 255
        } else if (PS4.Triangle()) {
            velLimitada = false;
            limiteVelocidade = 255;           // Restaura o limite maximo
            vTaskDelay(pdMS_TO_TICKS(200));   // Evita multiplas leituras

#pragma endregion

//===============================================================================================//
//==========================================//MACROS//===========================================//
//===============================================================================================//

//========================================//Macros em V//========================================//

#pragma region MACRO EM V

        // Movimentacao em V para a esquerda
        } else if (PS4.Left() > 0 && switchCxV) {
            vTaskDelay(pdMS_TO_TICKS(10));    // Delay para garantir o fim da logica anterior
            xTaskNotifyGive(openServoHandle); // Abre o servomotor
            moverMotores(-255, 255);          // Gira para a esquerda
            vTaskDelay(pdMS_TO_TICKS(40));
            moverMotores(255, 255);           // Anda para frente
            vTaskDelay(pdMS_TO_TICKS(150));
            moverMotores(255, -255);          // Gira para a direita
            vTaskDelay(pdMS_TO_TICKS(130));
            moverMotores(255, 255);           // Anda para frente  
            vTaskDelay(pdMS_TO_TICKS(100));
            moverMotores(0, 0);               // Para os motores

        // Movimentacao em V para a direita
        } else if (PS4.Right() > 0 && switchCxV) {
            vTaskDelay(pdMS_TO_TICKS(10));    // Delay para garantir o fim da logica anterior
            xTaskNotifyGive(openServoHandle); // Abre o servomotor
            moverMotores(255, -255);          // Gira para a direita
            vTaskDelay(pdMS_TO_TICKS(40));
            moverMotores(255, 255);           // Anda para frente
            vTaskDelay(pdMS_TO_TICKS(150));
            moverMotores(-255, 255);          // Gira para a esquerda
            vTaskDelay(pdMS_TO_TICKS(130));
            moverMotores(255, 255);           // Anda para frente  
            vTaskDelay(pdMS_TO_TICKS(100));
            moverMotores(0, 0);               // Para os motores

#pragma endregion

//======================================//Macros Desviada//======================================//

#pragma region MACRO DESVIADA

        // Movimentacao em C para a esquerda
        } else if (PS4.Left() > 0 && !switchCxV) {
            vTaskDelay(pdMS_TO_TICKS(10));    // Delay para garantir o fim da logica anterior
            moverMotores(-255, 255);          // Gira para a direita
            vTaskDelay(pdMS_TO_TICKS(75));
            moverMotores(255, 100);         // Para tras
            vTaskDelay(pdMS_TO_TICKS(144));
            moverMotores(255, -255);          // Gira para a direita
            vTaskDelay(pdMS_TO_TICKS(156));
            moverMotores(0, 0);               // Para os motores

        // Movimentacao em V para a direita
        } else if (PS4.Right() > 0 && !switchCxV) {
            vTaskDelay(pdMS_TO_TICKS(10));    // Delay para garantir o fim da logica anterior
            moverMotores(255, -255);          // Gira para a direita
            vTaskDelay(pdMS_TO_TICKS(75));
            moverMotores(127, 255);         // Para tras
            vTaskDelay(pdMS_TO_TICKS(144));
            moverMotores(-255, 255);          // Gira para a direita
            vTaskDelay(pdMS_TO_TICKS(156));
            moverMotores(0, 0);               // Para os motores

#pragma endregion

//===============================================================================================//
//=====================================//MOVIMENTACAO LIVRE//====================================//
//===============================================================================================//

//========================================//Curvas Puras//=======================================//

#pragma region CURVAS PURAS

        } else {
            r2 = PS4.R2Value();               // Armazena o valor de R2
            l2 = PS4.L2Value();               // Armazena o valor de R2
            direcao = PS4.LStickX();          // Armazena o valor em X do Direcional Esquerdo

            // Se nenhum gatilho estiver pressionado (considerando a zona morta)
            if (r2 <= zonaMorta && l2 <= zonaMorta) {
                velocidadeEsquerda = 0;
                velocidadeDireita = 0;

                // Se os gatilhos estao parados, verifica se o analogico quer girar
                    if (direcao < -zonaMorta) { // Curva pra esquerda parado
					
                        int velCurva = map(abs(direcao), zonaMorta, 128, 0, limiteCurva);
                        velocidadeEsquerda = -velCurva;
                        velocidadeDireita = velCurva;
                    }
                    else if (direcao > zonaMorta) { // Curva pra direita parado
					
                        int velCurva = map(direcao, zonaMorta, 127, 0, limiteCurva);
                        velocidadeEsquerda = velCurva;
                        velocidadeDireita = -velCurva;
                    }

                // Se nem gatilho nem analogico estiverem ativos, as velocidades continuam 0.

#pragma endregion

//=======================================//Gatilhos Puros//======================================//

#pragma region GATILHOS PUROS

            } else {                          // Pelo menos um gatilho pressionado

                // Define a velocidade base (frente ou re)
                if (r2 > zonaMorta) {                // Gatilho direito (frente)
                    int velocidade = map(r2, zonaMorta, 255, 0, limiteVelocidade);
                    velocidadeEsquerda = velocidade;
                    velocidadeDireita = velocidade;
                } else {                      // Gatilho esquerdo (re)
                    int velocidade = map(l2, zonaMorta, 255, 0, limiteVelocidade);
                    velocidadeEsquerda = -velocidade;
                    velocidadeDireita = - int(coefReverse * velocidade);
                }

#pragma endregion

//=====================================//Gatilhos e Curvas//=====================================//

#pragma region GATILHOS E CURVAS

                // Aplica a curva sobre a velocidade existente
                if (direcao < -zonaMorta) {          // Curva pra esquerda em movimento
                    // Reduz a velocidade da roda interna (esquerda para frente, direita para re)
                    if (r2 > zonaMorta) 
                        velocidadeEsquerda *= (1.0 - (abs(direcao) * coefAtenuacao) / 255.0);
                    else 
                        velocidadeDireita *= (1.0 - (abs(direcao) * coefAtenuacao) / 255.0);

                } else if (direcao > zonaMorta) {    // Curva pra direita em movimento
                    // Reduz a velocidade da roda interna (direita para frente, esquerda para re)
                    if (r2 > zonaMorta) 
                        velocidadeDireita *= (1.0 - (abs(direcao) * coefAtenuacao) / 255.0);
                    else 
                        velocidadeEsquerda *= (1.0 - (abs(direcao) * coefAtenuacao) / 255.0);
                }
            }
            moverMotores(velocidadeEsquerda, velocidadeDireita);
        }
    }
    brakeMotors();                            // Se desconectar, freia o motor
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif