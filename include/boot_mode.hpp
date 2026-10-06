//===============================================================================================//
//=====================================//INCLUDES E DEFINES//====================================//
//===============================================================================================//

#pragma region INCLUDES

#ifndef BOOT_MODE_H
#define BOOT_MODE_H

#include <defines.hpp>                        // Definicoes globais
#include <functions.hpp>                      // Funcoes auxiliares

// Estado da senha de boot (4 5 6 + 7/8/9). O 4o digito escolhe o modo: 7 = IDLE, 8 = AUTO, 9 = RC
int bootSenhaProgresso         = 0;           // Digitos corretos ja recebidos
bool bootSenhaFechada          = false;       // Senha completa: aguardando reinicio
BootMode bootModoPendente      = BOOT_IDLE;   // Modo escolhido pela senha
unsigned long bootSenhaFechadaMs = 0;         // Instante em que a senha fechou

const int BOOT_SENHA_PREFIXO[BOOT_SENHA_TAMANHO - 1] = {4, 5, 6};

#pragma endregion

//===============================================================================================//
//=======================================//NVS DO BOOT//=========================================//
//===============================================================================================//

#pragma region NVS

const char* nomeModoBoot(BootMode modo) {
    switch (modo) {
        case BOOT_RC:   return "RC";
        case BOOT_AUTO: return "AUTO";
        default:        return "IDLE";
    }
}

// Le o modo gravado na NVS. Sem valor (ou invalido) -> IDLE
BootMode carregarBootMode() {
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        Serial.println("[BOOT] NVS indisponivel. Iniciando em IDLE.");
        return BOOT_IDLE;
    }
    uint8_t bruto = preferences.getUChar(NVS_KEY_BOOT, 0xFF);
    preferences.end();

    if (bruto > BOOT_AUTO) return BOOT_IDLE;
    Serial.printf("[BOOT] Modo gravado na NVS: %s\n", nomeModoBoot((BootMode)bruto));
    return (BootMode)bruto;
}

void salvarBootMode(BootMode modo) {
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("[BOOT] ERRO: NVS indisponivel, modo NAO gravado.");
        return;
    }
    preferences.putUChar(NVS_KEY_BOOT, (uint8_t)modo);
    preferences.end();
}

#pragma endregion

//===============================================================================================//
//====================================//SENHA DE BOOT POR IR//===================================//
//===============================================================================================//

#pragma region SENHA BOOT

/*
!INFO | Senha de boot por IR
-----------------------------------
Nas janelas em que o robo esta parado esperando (IDLE, pareamento do PS4 no RC e selecao BT no AUTO),
digitar 4 5 6 e depois 7 (IDLE), 8 (AUTO) ou 9 (RC) grava o modo na NVS e reinicia o ESP. Os botoes 8 e
9 continuam engatando AUTO e RC no IDLE sem gravar nada, pois so sao consumidos com a senha em curso
*/

// Alimenta a senha com um digito do controle. Retorna true se o digito foi consumido
bool bootSenhaFeed(int digito) {
    if (digito <= 0 || digito > 9) return false;
    if (bootSenhaFechada) return true;        // Engole tudo ate o reinicio

    // Ultimo digito: escolhe o modo
    if (bootSenhaProgresso == BOOT_SENHA_TAMANHO - 1) {
        if (digito == 7) bootModoPendente = BOOT_IDLE;
        else if (digito == 8) bootModoPendente = BOOT_AUTO;
        else if (digito == 9) bootModoPendente = BOOT_RC;
        else { bootSenhaProgresso = 0; return true; }

        bootSenhaFechada = true;
        bootSenhaFechadaMs = millis();
        Serial.printf("[BOOT] Senha completa: proximo boot em %s\n", nomeModoBoot(bootModoPendente));
        return true;
    }

    if (digito != BOOT_SENHA_PREFIXO[bootSenhaProgresso]) {
        if (bootSenhaProgresso == 0) return false;   // Nao era da senha: tratamento normal
        bootSenhaProgresso = 0;
        Serial.println("[BOOT] Digito fora de sequencia. Senha cancelada.");
        return true;
    }

    bootSenhaProgresso++;
    return true;
}

// Consome o comando IR pendente (se houver) para a senha. Chamar a cada iteracao da janela
void bootSenhaProcessar() {
    if (ultimoComandoIR != 0xFFFF) {
        // Botao N do controle chega como comando N-1 (ex.: botao 8 = 0x7)
        if (bootSenhaFeed((int)ultimoComandoIR + 1)) ultimoComandoIR = 0xFFFF;
    }

    static int ultimoProgressoLed = -1;
    if (bootSenhaProgresso == 0 && !bootSenhaFechada) {
        if (ultimoProgressoLed != -1) {       // Senha cancelada: apaga o roxo
            ultimoProgressoLed = -1;
            clearLeds();
        }
        return;
    }

    if (ultimoProgressoLed != bootSenhaProgresso + (bootSenhaFechada ? 10 : 0)) {
        ultimoProgressoLed = bootSenhaProgresso + (bootSenhaFechada ? 10 : 0);
        ledsSenhaBoot(bootSenhaProgresso, bootSenhaFechada, bootModoPendente);
    }

    if (bootSenhaFechada && millis() - bootSenhaFechadaMs >= BOOT_SENHA_CONFIRMA_MS) {
        salvarBootMode(bootModoPendente);
        Serial.printf("[BOOT] Modo gravado (%s). Reiniciando...\n", nomeModoBoot(bootModoPendente));
        Serial.flush();
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP.restart();
    }
}

// Fecha a janela da senha (luta iniciada ou controle conectado)
void bootSenhaCancelar() {
    if (bootSenhaFechada) return;             // Reinicio ja contratado
    bootSenhaProgresso = 0;
}

#pragma endregion

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif
