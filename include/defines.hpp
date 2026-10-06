//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef DEFINES_H
#define DEFINES_H

#include <Arduino.h>                          // DOIS | Funcoes basicas do framework Arduino
#include <BluetoothSerial.h>                  // DOIS | Comunicacao serial via Bluetooth para debug
#include <FastLED.h>                          // DOIS | Controle de fitas de LED enderecaveis
#include <IRremote.hpp>                       // DOIS | Decodifica sinais do modulo de largada IR
#include <Preferences.h>                      // DOIS | Salva o modo de boot na memoria flash NVS
#include <math.h>                             // DOIS | Funcoes matematicas padrao (sqrt, pow)

#include "soc/gpio_struct.h"                   // DOIS | Acesso direto e rapido aos pinos GPIO
#include "driver/ledc.h"                       // DOIS | Controle do periferico de PWM por hardware

#pragma endregion

//=====================================//Objetos e Handles//=====================================//

#pragma region OBJETOS E HANDLES

BluetoothSerial                SerialBT;      //  DOIS | Objeto para controlar o BT
CRGB                           leds[5];       //  DOIS | Objeto para os 5 LEDs enderecaveis
Preferences                    preferences;   //  DOIS | Objeto para salvar NVS

hw_timer_t *sensorTimer        = NULL;        //  AUTO | Handle do timer dos JSumos

TaskHandle_t openServoHandle   = NULL;        //  DOIS | Handle da task de abrir servo
TaskHandle_t closeServoHandle  = NULL;        //  DOIS | Handle da task de fechar servo

TaskHandle_t fightingLogicHandle = NULL;        //  AUTO | Handle da task do combate iterativo
TaskHandle_t openingsHandle   = NULL;        //  AUTO | Handle da task da estrategia inicial

TaskHandle_t stopRobotHandle   = NULL;        //  AUTO | Handle da task de desligar robo
TaskHandle_t IRCommandHandle   = NULL;        //  AUTO | Handle da task de interpretar IR
TaskHandle_t swSensorHandle    = NULL;        //  AUTO | Handle da task pra desligar sensor

QueueHandle_t btQueue;                        //  AUTO | Handle da fila do BT

volatile uint16_t ultimoComandoIR = 0xFFFF;   //  DOIS | Armazena o comando IR recebido

// Mux para a leitura de sensores e escrita dos motores
portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE motorMux = portMUX_INITIALIZER_UNLOCKED;

//=========================================//Bluetooth//=========================================//

// Verifica se o Bluetooth esta devidamente habilitado no menuconfig
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif                                        // Finaliza o condicional do erro

#pragma endregion

//========================================//Instrucoes//=========================================//

#pragma region INSTRUCOES

/*
!INFO | Instrucoes e GPIOs do ESP32
-----------------------------------
Definicao da placa
   -> No ArduinoIDE selecionar: ESP32 Dev Module

Definicao de pinos e configuracoes
   O ESP32-WROOM-32 possui 39 GPIOs que podem ser configuradas como entrada, saida, ADC, PWM etc
         
      -> GPIOs que NAO devem ser usadas em projetos:
         - GPIOs 6, 7, 8, 9, 10, 11: conectadas na memoria SPI Flash do ESP32
         - GPIOs 20, 24, 28, 29, 30, 31, 37, 38: usadas internamente, nao disponiveis pra uso

      -> GPIOs com uso limitado ou comportamento especial no boot ou flash:
         - GPIO 0: deve estar em LOW para entrar no modo de flash
         - GPIO 1: conectada ao TX Pin do ESP, usada para comunicacao com o modulo
         - GPIO 2: deve estar flutuando ou em LOW durante o boot. Usar para LED_BUILTIN 
         - GPIO 3: conectada ao RX Pin do ESP, usada para comunicacao com o modulo
         - GPIO 12: causa falha no boot se estiver em nivel logico HIGH (strapping pin)
         - GPIO 15: deve estar em HIGH durante o boot
         - GPIOs 34, 35, 36, 39: INPUT ONLY e nao tem resistores pull-up/down internos

      -> GPIOs recomendadas para uso geral (sem restricoes conhecidas):
         - GPIOs 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33.

      -> Mais informacoes em: https://tinyurl.com/3p99rpkk
*/

#pragma endregion

//===============================================================================================//
//=====================================//PINOS E VARIAVEIS//=====================================//
//===============================================================================================//

//==========================================//Motores//==========================================//

#pragma region PINOS MOTORES

/*
!INFO | Ultimos pinos conhecidos dos robos
-----------------------------------
              LP     LN     RP     RN
SMOKER        17     16     19     18
ARRUELA       17     16     19     18
BRIGA         19     18     16     17
FUEGO         19     18     16     17
FUEGUITO      19     18     16     17
RESSACA       19     18     17     16
SHENLONG      19     18     16     17
TSUNAMI       17     16     18     19
VAMPETA       18     19     17     16
*/

// Esquerdo invertido na última montagem (sem termo azul)
#define LEFT_POS_PIN           19             //  DOIS | Ponte H M1 A_IN_2 ESP32 IO18
#define LEFT_NEG_PIN           18             //  DOIS | Ponte H M1 A_IN_1 ESP32 IO19
#define RIGHT_POS_PIN          16             //  DOIS | Ponte H M2 B_IN_2 ESP32 IO16
#define RIGHT_NEG_PIN          17             //  DOIS | Ponte H M2 B_IN_1 ESP32 IO17

#define LEFT_POS_CHANNEL       LEDC_CHANNEL_6 //  DOIS | Canal PWM positivo esquerdo
#define LEFT_NEG_CHANNEL       LEDC_CHANNEL_7 //  DOIS | Canal PWM negativo esquerdo
#define RIGHT_POS_CHANNEL      LEDC_CHANNEL_4 //  DOIS | Canal PWM positivo direito
#define RIGHT_NEG_CHANNEL      LEDC_CHANNEL_5 //  DOIS | Canal PWM negativo direito

#define PWM_FREQ               500            //  DOIS | Frequencia para os motores (Hz)
#define PWM_FREIO              255            //  DOIS | PWM de freio ativo dos motores
#define PWM_RESOLUTION         8              //  DOIS | Resolucao de 8 bits
#define PWM_ZERO_DELAY         30             //  DOIS | Tempo para Ponte H limpar PWM (µs)

#pragma endregion

//========================================//Servomotor//========================================//

#pragma region PINOS SERVO

#define SERVOMOTOR_PIN         32              //  DOIS | Servomotor da haste ESP32 porta 23

#define SERVO_LEDC_CHANNEL     LEDC_CHANNEL_0 //  DOIS | Canal do PWM do servo
#define SERVO_TIMER            LEDC_TIMER_0   //  DOIS | Timer do PWM do servo

#define SERVO_FREQ_HZ          50             //  DOIS | Frequencia padrao do MG90S
#define SERVO_RESOLUTION       16             //  DOIS | Resolucao de 16 bits para o canal
#define SERVO_MIN_PULSE_US     544            //  DOIS | Largura de pulso para 0 graus (µs)
#define SERVO_MAX_PULSE_US     2400           //  DOIS | Largura de pulso para 180 graus (µs)

#pragma endregion

//========================================//Comunicacao//========================================//

#pragma region COMUNICACAO

#define LED_PIN                2              //  DOIS | LED padrao ESP32 IO2
#define IR_RECIEVE_PIN         13              //  DOIS | Sensor IR ESP32 IO13

#define LEDS_ENDERECAVEIS_PIN  33             //  DOIS | LEDs enderecaveis ESP32 IO33
#define NUM_LEDS               5              //  DOIS | Numero de LEDs enderecaveis

#define MAX_STEPS              20             //  AUTO | Numero maximo de passos da estrategia z
#define BT_BUFFER_SIZE         64             //  AUTO | Tamanho buffer de comandos personalizados
#define BT_QUEUE_LENGTH        128            //  AUTO | Tamanho dda fila de BT
#define BT_NUM_TIMEOUT_MS      600            //  AUTO | Tempo sem digitos para validar o indice recebido

#define NVS_NAMESPACE          "fuego_cfg"    //  DOIS | Namespace da NVS
#define NVS_KEY_BOOT           "boot"         //  DOIS | Chave do modo de boot (IDLE/RC/AUTO)
#define BOOT_SENHA_TAMANHO     4              //  DOIS | Digitos da senha IR (3 de prefixo + 1 de modo)
#define BOOT_SENHA_CONFIRMA_MS 2000           //  DOIS | Tempo mostrando o modo escolhido antes de reiniciar
#define FINAL_TEMPO_MS         4000           //  AUTO | Tempo da finalizacao por tempo (ms)

#pragma endregion

//=======================================//Sensoreamento//=======================================//

#pragma region SENSOREAMENTO

#define NMOS_PIN  25                          //  AUTO | NMOS que desconecta o GND dos sensores

#define JSUMO_DIR_PIN          14             //  AUTO | JSumo direito ESP32 IO14
#define JSUMO_ESQ_PIN          23             //  AUTO | JSumo esquerdo ESP32 IO23

#define IR_DIR_PIN             4              //  AUTO | Sensor IR direito ESP32 IO4
#define IR_ESQ_PIN             5              //  AUTO | Sensor IR esquerdo ESP32 IO5

#define LINHA_DIR_PIN          34
#define LINHA_ESQ_PIN          39
#define LINHA_TRESHOLD         3800

#define LDR_PIN                36
#define LDR_TRESHOLD           200

#pragma endregion

//===================================//Variaveis utilizadas//====================================//
//BRIEL ESTEVE AQUI
#pragma region VARIAVEIS

volatile bool value_JS_E      = false;       //  AUTO | Definicao do Jsumo Esquerdo
volatile bool value_JS_D      = false;       //  AUTO | Definicao do Jsumo Direito
volatile bool value_IR_E      = false;       //  AUTO | Definicao do IR Esquerdo
volatile bool value_IR_D      = false;       //  AUTO | Definicao do IR Direito

volatile int value_QRE_E      = 0;           //  AUTO | Definicao do Linha Esquerdo
volatile int value_QRE_D      = 0;           //  AUTO | Definicao do Linha Direito
volatile int value_LDR        = 0;           //  AUTO | Definicao do LDR

bool inicializado              = false;       //  DOIS | Flag para validar a inicializacao 
bool running                   = false;       //  AUTO | Indica se o robo esta lutando
bool ready                     = false;       //  AUTO | Usada para testar se o robo recebe IR
bool seeing                    = false;       //  AUTO | Usada para indicar se o robo ve o outro
bool modoFurtivo               = false;       //  AUTO | Usada para indicar se deve desligar Jsumo

unsigned long tempoFighting      = 0;           //  AUTO | Inicio do combate

volatile int novaVE            = 0;           //  DOIS | Velocidade pra atualizar o motor esquerdo
volatile int novaVD            = 0;           //  DOIS | Velocidade pra atualizar o motor direito

int customOpeningCount        = 0;           //  AUTO | Contagem de passos da estrategia personalizada
int bufferIndex                = 0;           //  AUTO | Indice para controlar a posicao no buffer

int macroIndex                 = 0;           //  AUTO | Iniciacao escolhida (0 = nenhuma, N = macro N-1)
char btBuffer[BT_BUFFER_SIZE];                //  AUTO | Buffer para os comandos personalizados

#pragma endregion

//======================================//Enums e Structs//======================================//

#pragma region ENUMS E STRUCTS

enum BootMode {                               //  DOIS | Modo de inicializacao do robo
   BOOT_IDLE,
   BOOT_RC,
   BOOT_AUTO
};

enum ConfigStage {                            //  AUTO | Etapas de selecao BT (na ordem do fluxo)
   STAGE_MODE,                                // Modo: luta, teste sensor, teste motor, personalizada
   STAGE_CUSTOM_OPENING,                        // Passos da estrategia personalizada (so modo 3)
   STAGE_FURTIVE,                             // Modo furtivo
   STAGE_DIRECTION,                           // Direcao da estrategia
   STAGE_INITIATION,                          // Macro de iniciacao
   STAGE_MOVEMENT,                            // Estrategia iterativa de movimentacao
   STAGE_FINALIZATION,                        // Finalizacao (LDR ou tempo) -> ultima etapa
   STAGE_DONE
};

enum ModoBT {                                 //  AUTO | Opcoes da etapa de modo
   MODO_LUTA,
   MODO_TESTE_SENSOR,
   MODO_TESTE_MOTOR,
   MODO_PERSONALIZADA
};

enum FinalizationMode {                       //  AUTO | Finalizacao da estrategia
   FIM_LDR,
   FIM_TEMPO
};

enum ModoLuta {                               //  AUTO | Define qual o modo de luta iterativo
   buscaOfensiva,
   buscaDefensiva,
   buscaLinha,
   buscaPulsada,
   retorno,
   desengate
};

enum Direction {                              //  AUTO | Direcao de movimentacao do robo
   esquerda,
   direita,
   reto
};

enum DirecaoAdversario {                      //  AUTO | Direcao de movimentacao do adversario
   vistoEsquerda,
   vistoDireita,
   nuncaVisto
};

enum AsaEstado {                              //  AUTO | Estado da asa do robo
   asaFechada,
   asaAberta
};

enum ViuLinha {
   linhaNADA,
   linhaESQ,
   linhaDIR,
   linhaAMBAS
};

struct OpeningStep {                         //  AUTO | Struct para os comandos personalizados
   int speedLeft;
   int speedRight;
   int delayMs;
};

BootMode currentBootMode       = BOOT_IDLE;
ConfigStage currentStage       = STAGE_MODE;
ModoBT modoBT                  = MODO_LUTA;
FinalizationMode finalization  = FIM_LDR;

ModoLuta modoLuta              = buscaOfensiva;
ModoLuta modoLutaOriginal      = buscaOfensiva;
Direction direction            = esquerda;
DirecaoAdversario ultimoLado   = nuncaVisto;
AsaEstado estadoAsa            = asaFechada;
ViuLinha viuLinha              = linhaNADA;
OpeningStep customOpening    [MAX_STEPS];

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif