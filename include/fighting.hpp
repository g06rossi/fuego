#ifndef FIGHTING_H
#define FIGHTING_H

#include <defines.hpp>                        // Definicoes globais
#include <move.hpp>                           // Funcoes de movivmentacao de motores

// Declaracoes antecipadas (Forward declarations)
void modoBuscaOfensiva();
void modoBuscaDefensiva();
void modoBuscaLinha();
void modoBuscaPulsada();
void modoRetorno();
void modoDesengate();

// Parametros de velocidades (convertidos de 0-100 para 0-255)
#define velAtaqueMax           255
#define velAtaqueReduzida      127
#define velBuscaGiro           153

#define velDefensivaGiro       102
#define velDefensivaCruzeiro   51

#define velBuscaLinhaAvanco    255
#define velBuscaLinhaCruzeiro  140

#define pulsoPwm               255

// Variaveis de controle
unsigned long ultimaLinhaMs = 0;
unsigned long semLinhaMs = 0;
unsigned long avancoMs = 0;
bool avancando = false;
bool cargaTotal = false;
bool defensivaEscalou = false;
bool ldrAtacando = false;
int passoRetorno = 0;
int passoDesengate = 0;

void resetFightingState() {
    unsigned long agora = millis();
    ultimaLinhaMs = agora;
    semLinhaMs = agora;
    avancoMs = 0;
    avancando = false;
    cargaTotal = false;
    defensivaEscalou = false;
    ldrAtacando = false;
    passoRetorno = 0;
    passoDesengate = 0;
}

//===============================================================================================//
//====================================//MODO BUSCA OFENSIVA//====================================//
//===============================================================================================//

void modoBuscaOfensiva() {
    bool viuEsq = value_JS_E;
    bool viuDir = value_JS_D;

    if (viuEsq) ultimoLado = vistoEsquerda;
    else if (viuDir) ultimoLado = vistoDireita;

    if (viuEsq || viuDir) {
        if (viuEsq && viuDir) {
            moverMotores(velAtaqueMax, velAtaqueMax);
        } else if (viuEsq) {
            moverMotores(velAtaqueReduzida, velAtaqueMax);
        } else {
            moverMotores(velAtaqueMax, velAtaqueReduzida);
        }
    } else {
        if (ultimoLado == vistoDireita) {
            moverMotores(velBuscaGiro, -velBuscaGiro);
        } else {
            moverMotores(-velBuscaGiro, velBuscaGiro);
        }
    }
}

//===============================================================================================//
//===================================//MODO BUSCA DEFENSIVA//====================================//
//===============================================================================================//

void modoBuscaDefensiva() {
    unsigned long agora = millis();
    
    // Escala por estagnacao (10000ms sem tocar a linha)
    if (!defensivaEscalou && agora - semLinhaMs >= 10000) {
        defensivaEscalou = true;
        ultimaLinhaMs = agora;
        semLinhaMs = agora;
    }

    if (defensivaEscalou) {
        modoBuscaLinha();
        return;
    }

    bool viuEsq = value_JS_E;
    bool viuDir = value_JS_D;

    if (viuEsq) ultimoLado = vistoEsquerda;
    else if (viuDir) ultimoLado = vistoDireita;

    if (viuEsq && !viuDir) {
        moverMotores(-velDefensivaGiro, velDefensivaGiro);
    } else if (viuDir && !viuEsq) {
        moverMotores(velDefensivaGiro, -velDefensivaGiro);
    } else {
        moverMotores(velDefensivaCruzeiro, velDefensivaCruzeiro);
    }
}

//===============================================================================================//
//======================================//MODO BUSCA LINHA//=====================================//
//===============================================================================================//

void modoBuscaLinha() {
    unsigned long agora = millis();
    
    // Etapa 2: Carga Total
    if (!cargaTotal && (agora - semLinhaMs >= 7000)) {
        cargaTotal = true;
    }

    if (cargaTotal) {
        moverMotores(velAtaqueMax, velAtaqueMax);
        return;
    }

    // Etapa 1: Arrancada de desencalhe
    if (!avancando && (agora - ultimaLinhaMs >= 4000)) {
        avancando = true;
        avancoMs = agora;
    }

    if (avancando) {
        if (agora - avancoMs < 90) {
            moverMotores(velBuscaLinhaAvanco, velBuscaLinhaAvanco);
        } else {
            avancando = false;
            ultimaLinhaMs = agora;
            moverMotores(velBuscaLinhaCruzeiro, velBuscaLinhaCruzeiro);
        }
    } else {
        moverMotores(velBuscaLinhaCruzeiro, velBuscaLinhaCruzeiro);
    }
}

//===============================================================================================//
//=====================================//MODO BUSCA PULSADA//====================================//
//===============================================================================================//

void modoBuscaPulsada() {
    unsigned long decorrido = millis() - tempoFighting;
    
    // 4 pulsos de 1500ms -> finaliza em 4*1500 - 1500 + 60 = 4560ms
    if (decorrido >= 4560) {
        modoBuscaLinha();
        return;
    }

    bool pulsando = (decorrido % 1500) < 60;
    if (pulsando) {
        moverMotores(pulsoPwm, pulsoPwm);
    } else {
        moverMotores(0, 0);
    }
}

//===============================================================================================//
//========================================//MODO RETORNO//=======================================//
//===============================================================================================//

void modoRetorno() {
    static unsigned long tempoPassoRetorno = 0;
    
    // Fumacinha tempos:
    // Lateral: recuo 125ms, giro 120ms
    // Frontal: recuo 150ms, giro 175ms
    
    unsigned long tempoRecuo = (viuLinha == linhaAMBAS) ? 150 : 125;
    unsigned long tempoGiro  = (viuLinha == linhaAMBAS) ? 175 : 120;

    switch (passoRetorno) {
        case 0:
            moverMotores(-255, -255);
            tempoPassoRetorno = millis();
            passoRetorno = 1;
            break;
        
        case 1:
            if (millis() - tempoPassoRetorno >= tempoRecuo) passoRetorno = 2;
            break;

        case 2:
            // Fumacinha vira para dentro da arena (para o lado da linha, como o original RECUA do flanco)
            if (viuLinha == linhaESQ) moverMotores(-255, 255);
            else if (viuLinha == linhaDIR) moverMotores(255, -255);
            else {
                if (ultimoLado == vistoDireita) moverMotores(255, -255);
                else moverMotores(-255, 255);
            }
            tempoPassoRetorno = millis();
            passoRetorno = 3;
            break;
        
        case 3:
            if (millis() - tempoPassoRetorno >= tempoGiro) {
                passoRetorno = 0;
                ultimaLinhaMs = millis();
                semLinhaMs = millis();
                modoLuta = modoLutaOriginal;
            }
            break;
    }
}

//===============================================================================================//
//=======================================//MODO DESENGATE//======================================//
//===============================================================================================//

void modoDesengate() {
    static unsigned long tempoPassoDesengate = 0;
    
    // Oponente escapou (rampa livre). Varre um "S" para tentar reencostar.
    // Passo 1: fecha pra um lado (140, 255 por 50ms)
    // Passo 2: cruza de volta pro outro (255, 140 por 100ms)
    int velFrenteLinha = 140; // 55 de cruzeiro -> ~140/255

    switch (passoDesengate) {
        case 0:
            moverMotores(velFrenteLinha, 255);
            tempoPassoDesengate = millis();
            passoDesengate = 1;
            break;
            
        case 1:
            if (millis() - tempoPassoDesengate >= 50) {
                moverMotores(255, velFrenteLinha);
                tempoPassoDesengate = millis();
                passoDesengate = 2;
            }
            break;
            
        case 2:
            if (millis() - tempoPassoDesengate >= 100) {
                passoDesengate = 0;
                modoLuta = modoLutaOriginal;
            }
            break;
    }
}

//===============================================================================================//
//=====================================//FINALIZA O ARQUIVO//====================================//
//===============================================================================================//

#endif
