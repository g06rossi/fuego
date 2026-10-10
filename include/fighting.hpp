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

//===============================================================================================//
//====================================//PARAMETROS DE AJUSTE//===================================//
//===============================================================================================//

// Velocidades em PWM (0-255) e tempos em ms

// Sensor de linha
int LINHA_TRESHOLD                        = 3800;  // ADC abaixo disso = linha branca (configuravel via NVS)

// Leitura do adversario (giros iguais em todas as buscas)
const int velGiroRoda                     = 255;   // Roda externa no giro em torno da roda (interna parada)
const int velGiroEixo                     = 192;   // Giro no proprio eixo
const unsigned long flancoTimeoutMs       = 7000;  // Duracao maxima da manobra de flanco da Asa Com IR

// Busca Ofensiva
const int velOfensivaFrente               = 255;   // Adversario a frente

// Busca Defensiva
const int velDefensivaFrente              = 70;    // Adversario a frente
const unsigned long defensivaEscalaMs     = 10000; // Sem tocar a linha -> vira Busca Linha de vez

// Busca Linha
int velBuscaLinhaCruzeiro                 = 60;   // Avanco normal (configuravel via NVS)
const int velBuscaLinhaArrancada          = 255;   // Arrancada de desencalhe
const unsigned long arrancadaIntervaloMs  = 4000;  // Sem linha -> dispara arrancada
const unsigned long arrancadaDuracaoMs    = 90;
const int velCargaTotal                   = 255;
const unsigned long cargaTotalMs          = 7000;  // Sem tocar a linha -> carga total ate o fim

// Busca Pulsada
const int velPulso                        = 255;
const unsigned long pulsoDuracaoMs        = 80;
const unsigned long pulsoEsperaMs         = 800;  // Parado entre os pulsos
const unsigned long pulsoQuantidade       = 6;

// Retorno (linha detectada)
const int velRetornoRecuo                 = 255;
const int velRetornoGiro                  = 255;
int retornoRecuoLateralMs                 = 200;   // Linha vista por 1 sensor (configuravel via NVS)
int retornoGiroLateralMs                  = 160;   // (configuravel via NVS)
int retornoRecuoFrontalMs                 = 250;   // Linha vista pelos 2 sensores (configuravel via NVS)
int retornoGiroFrontalMs                  = 225;   // (configuravel via NVS)

// Desengate (LDR apagou apos ataque): varredura em S
const int velDesengateInterna             = 140;   // Roda interna da curva
const int velDesengateExterna             = 255;   // Roda externa da curva
const unsigned long desengateIdaMs        = 80;    // Curva para a esquerda
const unsigned long desengateVoltaMs      = 140;   // Curva para a direita

enum EstadoFlanco {                           // FSM de leitura da Asa Com IR
    flancoInativo,
    flancoGiroEixo,                           // Gira no eixo ate um JSumo enxergar
    flancoPivoFora,                           // Gira em torno da roda para longe do sensor que enxerga
    flancoPivoVolta,                          // Gira em torno da outra roda de volta ao adversario
    flancoBuscaLinha                          // Estourou o tempo: Busca Linha ate o proximo retorno
};

enum Leitura {                                // Resultado da leitura dos sensores de adversario
    leituraNada,                              // Nada visto: a busca delega para a Busca Linha
    leituraFrente,                            // Adversario a frente: cada busca anda do seu jeito
    leituraGiro                               // Giro ja executado pela leitura
};

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
unsigned long inicioPulsada = 0;              // Inicio da contagem dos pulsos (0 = ainda nao iniciada)
EstadoFlanco estadoFlanco = flancoInativo;
bool flancoEsq = false;                       // Lado do sensor que disparou o flanco
bool giroFlancoEsq = false;                   // Sentido do giro no eixo antes do JSumo enxergar
unsigned long inicioFlanco = 0;

void resetFightingState() {
    unsigned long agora = millis();
    inicioPulsada = 0;
    estadoFlanco = flancoInativo;
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
//==================================//LEITURA DO ADVERSARIO//====================================//
//===============================================================================================//

/*
!INFO | Geometria dos sensores
-----------------------------------
IRs na frente, angulados 15 graus para as laterais (so recebem). JSumos nas laterais,
perpendiculares a frente (emitem e recebem). No furtivo da movimentacao os JSumos ficam sem GND
e sao ignorados

As leituras executam os giros (iguais em todas as buscas) e devolvem leituraFrente para que cada
busca ande para frente do seu jeito (ofensiva rapida, defensiva devagar, pulsada em pulsos)
*/

void girarEixo(bool esq)   { esq ? moverMotores(-velGiroEixo, velGiroEixo) : moverMotores(velGiroEixo, -velGiroEixo); }
// Gira para o lado pedido em torno da roda desse lado (ela fica parada)
void girarRoda(bool esq)   { esq ? moverMotores(0, velGiroRoda) : moverMotores(velGiroRoda, 0); }

// Rampa simples (com ou sem IR) e Asa sem emissor
Leitura leituraRampa(bool furtivo, bool jsE, bool jsD, bool irE, bool irD) {
    if (irE && irD)      return leituraFrente;
    if (irE || irD)      furtivo ? girarEixo(irE) : girarRoda(irE);
    else if (jsE || jsD) girarEixo(jsE);
    else return leituraNada;
    return leituraGiro;
}

// Asa com emissor: FSM de flanco. Fora do furtivo dispara com os 2 IRs e usa o JSumo como
// sensor do flanco; no furtivo dispara com 1 IR e usa esse IR
Leitura leituraAsaComEmissor(bool furtivo, bool jsE, bool jsD, bool irE, bool irD) {
    unsigned long agora = millis();

    if (furtivo && irE && irD) {
        estadoFlanco = flancoInativo;
        return leituraFrente;
    }

    if (estadoFlanco != flancoInativo && estadoFlanco != flancoBuscaLinha &&
        agora - inicioFlanco >= flancoTimeoutMs) {
        estadoFlanco = flancoBuscaLinha;
    }

    bool sensorFlanco = furtivo ? (flancoEsq ? irE : irD) : (flancoEsq ? jsE : jsD);

    switch (estadoFlanco) {
        case flancoInativo:
            // Fora do furtivo: gira no eixo ate um JSumo enxergar, sem parar no ponto cego entre
            // o IR e o JSumo. 2 IRs: lado oposto ao da macro; 1 IR: lado oposto ao do IR
            if (!furtivo && (irE || irD)) {
                giroFlancoEsq = (irE && irD) ? (direction == direita) : !irE;
                estadoFlanco = flancoGiroEixo;
                inicioFlanco = agora;
                girarEixo(giroFlancoEsq);
                return leituraGiro;
            }
            if (furtivo && (irE || irD)) {
                flancoEsq = irE;
                estadoFlanco = flancoPivoFora;
                inicioFlanco = agora;
                girarRoda(!flancoEsq);
                return leituraGiro;
            }
            if (jsE || jsD) girarEixo(jsE);
            else return leituraNada;
            return leituraGiro;

        case flancoGiroEixo:
            if (jsE || jsD) {
                flancoEsq = jsE;
                estadoFlanco = flancoPivoFora;
                girarRoda(!flancoEsq);
            } else {
                girarEixo(giroFlancoEsq);
            }
            return leituraGiro;

        case flancoPivoFora:
            if (sensorFlanco) {
                girarRoda(!flancoEsq);
            } else {
                estadoFlanco = flancoPivoVolta;
                girarRoda(flancoEsq);
            }
            return leituraGiro;

        case flancoPivoVolta:
            if (sensorFlanco) {
                estadoFlanco = flancoPivoFora;
                girarRoda(!flancoEsq);
            } else {
                girarRoda(flancoEsq);
            }
            return leituraGiro;

        default:                              // flancoBuscaLinha
            return leituraNada;
    }
}

// Le os sensores conforme o tipo de adversario escolhido no BT
Leitura lerAdversario() {
    bool furtivo = furtivoMovimentacao;
    bool jsE = !furtivo && value_JS_E, jsD = !furtivo && value_JS_D;
    bool irE = value_IR_E, irD = value_IR_D;

    if (tipoAdversario == advAsaComEmissor) return leituraAsaComEmissor(furtivo, jsE, jsD, irE, irD);
    return leituraRampa(furtivo, jsE, jsD, irE, irD);
}

//===============================================================================================//
//====================================//MODO BUSCA OFENSIVA//====================================//
//===============================================================================================//

void modoBuscaOfensiva() {
    switch (lerAdversario()) {
        case leituraFrente: moverMotores(velOfensivaFrente, velOfensivaFrente); break;
        case leituraNada:   modoBuscaLinha();                                   break;
        default:                                                                break;
    }
}

//===============================================================================================//
//===================================//MODO BUSCA DEFENSIVA//====================================//
//===============================================================================================//

void modoBuscaDefensiva() {
    unsigned long agora = millis();
    
    // Escala por estagnacao (tempo sem tocar a linha)
    if (!defensivaEscalou && agora - semLinhaMs >= defensivaEscalaMs) {
        defensivaEscalou = true;
        ultimaLinhaMs = agora;
        semLinhaMs = agora;
    }

    if (defensivaEscalou) {
        modoBuscaLinha();
        return;
    }

    switch (lerAdversario()) {
        case leituraFrente: moverMotores(velDefensivaFrente, velDefensivaFrente); break;
        case leituraNada:   modoBuscaLinha();                                     break;
        default:                                                                  break;
    }
}

//===============================================================================================//
//======================================//MODO BUSCA LINHA//=====================================//
//===============================================================================================//

void modoBuscaLinha() {
    unsigned long agora = millis();
    
    // Etapa 2: Carga Total
    if (!cargaTotal && (agora - semLinhaMs >= cargaTotalMs)) {
        cargaTotal = true;
    }

    if (cargaTotal) {
        moverMotores(velCargaTotal, velCargaTotal);
        return;
    }

    // Etapa 1: Arrancada de desencalhe
    if (!avancando && (agora - ultimaLinhaMs >= arrancadaIntervaloMs)) {
        avancando = true;
        avancoMs = agora;
    }

    if (avancando) {
        if (agora - avancoMs < arrancadaDuracaoMs) {
            moverMotores(velBuscaLinhaArrancada, velBuscaLinhaArrancada);
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

// Pulsa para frente; qualquer giro da leitura reinicia a contagem dos pulsos
void modoBuscaPulsada() {
    unsigned long agora = millis();
    if (inicioPulsada == 0) inicioPulsada = agora;

    Leitura leitura = lerAdversario();
    if (leitura == leituraGiro) {
        inicioPulsada = agora;
        return;
    }

    const unsigned long periodo = pulsoDuracaoMs + pulsoEsperaMs;
    unsigned long decorrido = agora - inicioPulsada;

    // Fim da contagem: o ultimo pulso acaba em (N-1) periodos + duracao do pulso
    if (leitura == leituraNada || decorrido >= (pulsoQuantidade - 1) * periodo + pulsoDuracaoMs) {
        modoBuscaLinha();
        return;
    }

    int vel = (decorrido % periodo < pulsoDuracaoMs) ? velPulso : 0;
    moverMotores(vel, vel);
}

//===============================================================================================//
//========================================//MODO RETORNO//=======================================//
//===============================================================================================//

void modoRetorno() {
    static unsigned long tempoPassoRetorno = 0;
    
    bool frontal = (viuLinha == linhaAMBAS);
    unsigned long tempoRecuo = frontal ? retornoRecuoFrontalMs : retornoRecuoLateralMs;
    unsigned long tempoGiro  = frontal ? retornoGiroFrontalMs  : retornoGiroLateralMs;

    switch (passoRetorno) {
        case 0:
            moverMotores(-velRetornoRecuo, -velRetornoRecuo);
            tempoPassoRetorno = millis();
            passoRetorno = 1;
            break;
        
        case 1:
            if (millis() - tempoPassoRetorno >= tempoRecuo) passoRetorno = 2;
            break;

        case 2: {
            // Vira para o lado oposto ao da linha, de volta para dentro da arena
            bool girarParaDireita;
            if (viuLinha == linhaESQ)      girarParaDireita = true;
            else if (viuLinha == linhaDIR) girarParaDireita = false;
            else                           girarParaDireita = (ultimoLado == vistoDireita);

            if (girarParaDireita) moverMotores(velRetornoGiro, -velRetornoGiro);
            else                  moverMotores(-velRetornoGiro, velRetornoGiro);
            tempoPassoRetorno = millis();
            passoRetorno = 3;
            break;
        }
        
        case 3:
            if (millis() - tempoPassoRetorno >= tempoGiro) {
                passoRetorno = 0;
                estadoFlanco = flancoInativo;   // Recomeca a leitura da Asa Com IR
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
    
    // Oponente escapou (rampa livre). Varre um "S" para tentar reencostar
    switch (passoDesengate) {
        case 0:
            moverMotores(velDesengateInterna, velDesengateExterna);
            tempoPassoDesengate = millis();
            passoDesengate = 1;
            break;

        case 1:
            if (millis() - tempoPassoDesengate >= desengateIdaMs) {
                moverMotores(velDesengateExterna, velDesengateInterna);
                tempoPassoDesengate = millis();
                passoDesengate = 2;
            }
            break;

        case 2:
            if (millis() - tempoPassoDesengate >= desengateVoltaMs) {
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
