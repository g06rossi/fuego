//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <defines.hpp>                        // Definicoes globais

#pragma endregion

//===============================================================================================//
//===========================================//DELAY//===========================================//
//===============================================================================================//

#pragma region DELAYS

// Funcao para o robo realizar um delay em µs sem bloquear o processamento do ESP32
void delayUs(uint32_t us) {
    uint64_t start = esp_timer_get_time();    // Obtem o tempo atual em µs
    while ((esp_timer_get_time() - start) < us) {
        taskYIELD();                          // Libera tempo de CPU para outras tarefas FreeRTOS
    }
}

#pragma endregion

//===============================================================================================//
//=======================================//SINAL DIGITAL//=======================================//
//===============================================================================================//

#pragma region SINAL DIGITAL

// Leitura direta de GPIO para uso em ISR (suporta pinos 0-39)
static inline bool IRAM_ATTR lerGPIO(uint8_t pino) {
    if (pino < 32) return (GPIO.in >> pino) & 0x1;
    return (GPIO.in1.val >> (pino - 32)) & 0x1;
}

// Funcao para definir um pino como HIGH
void directWriteHigh(int pin) {
    if (pin < 32)
        GPIO.out_w1ts = ((uint32_t)1 << pin);
    else if (pin < 64)
        GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
}

// Funcao para definir um pino como LOW
void directWriteLow(int pin) {
    if (pin < 32)
        GPIO.out_w1tc = ((uint32_t)1 << pin);
    else if (pin < 64)
        GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
}

bool gndLigado = true;                        // Estado do NMOS que conecta o GND dos JSumos

// Define o estado dos JSumos de forma absoluta. A notificacao do switchSensor so alterna e
// perde notificacoes seguidas (ulTaskNotifyTake com pdTRUE zera a contagem)
void definirJSumos(bool ligado) {
    if (ligado) directWriteHigh(NMOS_PIN);
    else directWriteLow(NMOS_PIN);
    gndLigado = ligado;
}

#pragma endregion

//===============================================================================================//
//===========================================//LEDs//============================================//
//===============================================================================================//

#pragma region LEDS

// Pisca o LED embutido um certo numero de vezes com um delay
void blinkLED(int times, int delayTime) {
    for (int i = 0; i < times; i++) {
        directWriteLow(LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(delayTime));
        directWriteHigh(LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
    directWriteLow(LED_PIN);
}

// Configura os LEDs enderecaveis com o tipo de chip (WS2812), pino e ordem de cores
void setupLeds() {
    FastLED.addLeds<WS2812,LEDS_ENDERECAVEIS_PIN, GRB>(leds, NUM_LEDS);
    FastLED.clear();
    FastLED.show();
}

// Define todos os 5 LEDs com seus respectivos valores RGB
void setLeds(
    uint8_t r0, uint8_t g0, uint8_t b0,
    uint8_t r1, uint8_t g1, uint8_t b1,
    uint8_t r2, uint8_t g2, uint8_t b2,
    uint8_t r3, uint8_t g3, uint8_t b3,
    uint8_t r4, uint8_t g4, uint8_t b4
) {
    leds[0] = CRGB(r0, g0, b0);
    leds[1] = CRGB(r1, g1, b1);
    leds[2] = CRGB(r2, g2, b2);
    leds[3] = CRGB(r3, g3, b3);
    leds[4] = CRGB(r4, g4, b4);
    FastLED.show();
}

// Modo para quando enxerga o robo adversario
void AnnihilationModeLeds() {
    setLeds(200, 0, 0,                // LED 1
            200, 0, 0,                // LED 2
            200, 0, 0,                // LED 3
            200, 0, 0,                // LED 4
            200, 0, 0);               // LED 5
}

// Apaga todos os LEDs
void clearLeds() {
    FastLED.clear();
    FastLED.show();
}

// Funcao para indicar o status do setup com LEDs verdes (feito) ou vermelhos (pendente)
void validaSetup(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t s3, uint8_t s4) {
    // Verde se 1, vermelho se 0
    leds[0] = s0 ? CRGB(0, 200, 0) : CRGB(200, 0, 0);
    leds[1] = s1 ? CRGB(0, 200, 0) : CRGB(200, 0, 0);
    leds[2] = s2 ? CRGB(0, 200, 0) : CRGB(200, 0, 0);
    leds[3] = s3 ? CRGB(0, 200, 0) : CRGB(200, 0, 0);
    leds[4] = s4 ? CRGB(0, 200, 0) : CRGB(200, 0, 0);

    FastLED.show();
}

// Funcao para indicar o status dos sensores JSumo (esquerda, frente, direita) usando LEDs
void indicarSensores(bool esquerda, bool frente, bool direita) {
    leds[0] = CRGB::Black;                    // LED 1 apagado
    leds[4] = CRGB::Black;                    // LED 5 apagado

    // Decide entre roxo (128, 0, 128) se 1 ou laranja (200, 128, 0) se 0
    leds[1] = esquerda ? CRGB(128, 0, 128) : CRGB(200, 128, 0);  
    leds[2] = frente   ? CRGB(128, 0, 128) : CRGB(200, 128, 0);  
    leds[3] = direita  ? CRGB(128, 0, 128) : CRGB(200, 128, 0); 

    FastLED.show();
}

// Painel dos testes de sensor: os 5 LEDs mostram JS_E, IR_E, LDR, IR_D e JS_D (roxo = enxerga)
void indicarSensoresTeste(bool jsE, bool irE, bool ldr, bool irD, bool jsD) {
    const CRGB enxerga = CRGB(128, 0, 128);
    const CRGB nada    = CRGB(200, 128, 0);

    leds[0] = jsE ? enxerga : nada;
    leds[1] = irE ? enxerga : nada;
    leds[2] = ldr ? enxerga : nada;
    leds[3] = irD ? enxerga : nada;
    leds[4] = jsD ? enxerga : nada;

    FastLED.show();
}

// Cor que representa cada modo de boot: RC verde, AUTO vermelho, IDLE laranja
CRGB corModoBoot(BootMode modo) {
    switch (modo) {
        case BOOT_RC:   return CRGB(0, 200, 0);
        case BOOT_AUTO: return CRGB(200, 0, 0);
        default:        return CRGB(200, 128, 0);
    }
}

// Modo IDLE: respiracao verde (~50fps). Deve ser chamada a cada iteracao da espera
void ledsHeartbeat() {
    static unsigned long ultimoHeartbeat = 0;
    if (millis() - ultimoHeartbeat < 20) return;
    ultimoHeartbeat = millis();

    uint8_t brilho = beatsin8(40, 10, 200);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB(0, brilho, 0);
    FastLED.show();
}

// Modos RC e AUTO ja engatados: todos os LEDs na cor do modo
void ledsModo(BootMode modo) {
    CRGB cor = corModoBoot(modo);
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = cor;
    FastLED.show();
}

// Progresso da senha de boot: um LED roxo por digito correto (a partir do LED 2) e, com a senha
// fechada, o LED 1 assume a cor do modo escolhido
void ledsSenhaBoot(int progresso, bool fechada, BootMode modoEscolhido) {
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
    for (int i = 0; i < progresso && i < BOOT_SENHA_TAMANHO - 1; i++) leds[1 + i] = CRGB::Purple;
    if (fechada) leds[0] = corModoBoot(modoEscolhido);
    FastLED.show();
}

#pragma endregion

//===============================================================================================//
//=======================================//CALLBACK BT//=========================================//
//===============================================================================================//

#pragma region CALLBACK BT

// Funcao de Callback: chamada pelo driver Bluetooth quando chegam dados
void bt_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_DATA_IND_EVT) {        // Nos interessa apenas o evento de dados recebidos
    while(SerialBT.available()){              // Enquanto houver dados disponiveis no buffer
      char receivedChar = SerialBT.read();    // Le um caractere
      // Envia o caractere para a fila para ser processado pela tarefa modoAutonomo
      // O '0' no final significa que ele espera se a fila estiver cheia
      xQueueSend(btQueue, &receivedChar, (TickType_t)0);
    }
  }
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif