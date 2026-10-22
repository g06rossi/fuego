# Fuego — Guia para agentes de IA

Você vai trabalhar no firmware do **Fuego**, um robô de **Mini Sumô** da equipe TamanduTech, mantido por Giovanni Rossi. O firmware roda num **ESP32** (Arduino + FreeRTOS, compilado com PlatformIO). Este documento reúne tudo o que é preciso saber antes de mudar qualquer coisa. Leia inteiro antes da primeira edição.

---

## 1. Como trabalhar com o mantenedor

- **Idioma:** converse em **português**. Os comentários no código também são em português, **sem acentos** (padrão do repositório). Arquivos Markdown podem ter acentos.
- **Compile depois de toda mudança:** `pio run`. Se o comando `pio` não estiver no PATH, use `~/.platformio/penv/Scripts/platformio.exe run` (Windows). Não diga que algo funciona só porque compilou. O teste real é no robô, e quem faz é o mantenedor. Deixe claro o que ficou sem teste.
- **Interpretações:** quando um pedido é ambíguo (por exemplo, "girar" sem dizer se é em torno da roda ou do eixo, ou um caso que não foi especificado), implemente a interpretação mais coerente e **liste as interpretações no fim da resposta** para o mantenedor confirmar.
- **Sem commits nem push** se o mantenedor não pedir.
- **Escopo:** faça só o que foi pedido. Se notar outro problema no caminho, aponte, mas não corrija sem perguntar.
- **Parâmetros de ajuste:** todo número que alguém pode querer calibrar (velocidade, tempo, limiar) vira constante nomeada no topo do arquivo. Os índices dos passos das FSMs não entram nessa regra.

## 2. Regras que não podem ser quebradas

1. **Não existe multi-round.** As regras do campeonato proíbem. Depois que o robô recebe o **IR 3**, ele tem que ficar parado para sempre, e passa por um stress-test de IR para confirmar isso. Nunca sugira nem implemente "reset para o próximo round", "voltar a lutar" ou algo parecido.
2. **Não bloqueie a lógica de combate.** Os modos de luta são FSMs cooperativas baseadas em `millis()`. Elas nunca usam `delay`/`vTaskDelay` dentro de `fighting.hpp`, porque a `fightingLogicTask` precisa trocar de modo na hora (linha, LDR). Macros com delay só existem nas **iniciações** (`openings.hpp`).
3. **Não use espera ocupada longa.** `delayUs()` não devolve a CPU ao FreeRTOS de verdade e pode disparar o watchdog. Em loops de espera, use `vTaskDelay(pdMS_TO_TICKS(1))` ou mais.
4. **Geometria dos sensores** (seção 4): os **IRs são a frente** e os **JSumos são as laterais**. Nunca trate "os 2 JSumos veem" como "adversário à frente".

## 3. Ambiente e build

| Item | Valor |
|---|---|
| Placa | `esp32dev` (ESP32-WROOM-32) |
| Plataforma | `espressif32@~6.7.0`, framework Arduino |
| Bibliotecas | `z3t0/IRremote@4.4.1`, `fastled/FastLED@^3.5.0`, PS4Controller (local em `lib/PS4-esp32-master`), BluetoothSerial e Preferences (core) |
| Monitor serial | 115200 baud |
| Flash | ~90% ocupada. Cuidado com bibliotecas novas ou strings grandes. |

Comandos (veja também `WORKFLOW.md`):
```
pio run                        # compila
pio run -t upload -t monitor   # grava e abre o monitor
pio run --target erase         # apaga a flash (apaga a NVS também!)
```

## 4. Hardware

### Sensores (importante para toda a lógica de luta)
- **JSumo E/D** (`JS_E`, `JS_D`): ficam nas **laterais**, perpendiculares à frente. Emitem e recebem. O GND deles passa por um transistor NMOS (`NMOS_PIN`). Desligar o NMOS deixa os JSumos "cegos". Esse é o **modo furtivo**.
- **IR E/D** (`IR_E`, `IR_D`): ficam na **frente**, cada um angulado ~15° para fora. **Só recebem**, ou seja, detectam a emissão IR do adversário. Um adversário sem emissor pode passar despercebido por eles. São **ativos em LOW** (0 no pino = viu): a ISR inverte a leitura (`value_IR_* = !lerGPIO(...)`), então no resto do código `value_IR_* == 1` significa "viu". Os JSumos são ativos em HIGH e não são invertidos.
- **Ponto cego:** entre o cone dos IRs (±15°) e os JSumos (90°) há uma faixa que nenhum sensor cobre. Um giro que começa num sensor precisa continuar até chegar no outro.
- **Linha E/D** (QRE, `value_QRE_E/D`): detectam a borda branca do dohyo. São **analógicos**: ADC abaixo de `LINHA_TRESHOLD` (3800, definido nos parâmetros do `fighting.hpp` e configurável por NVS) = linha branca.
- **LDR** (`value_LDR`): detecta a sombra do adversário sobre a rampa. É usado na finalização por LDR. É **analógico**, com média móvel de `LDR_JANELA_FILTRO` (8) amostras: abaixo de `LDR_TRESHOLD` (200) = adversário na rampa.
- JSumos e IRs são lidos como **digitais** pela ISR `readSensors()` a cada `PERIODO_SENSORES_US` (timer de hardware; padrão **300 µs**, configurável por NVS de 100 a 10000 µs), com `lerGPIO()`. Linha e LDR são lidos pela `analogSensorTask`, que a própria ISR acorda por notificação a cada ciclo, porque `analogRead` não pode ser chamado dentro de ISR. Não troque isso por `vTaskDelay`: o menor intervalo dele é 1 ms, um passo do FreeRTOS. A task lê com `adc1_get_raw` (ADC configurado uma vez, `adc_power_acquire`), **nunca com `analogRead`** (veja a seção 11). O Teste de sensor mostra a maior duração de um ciclo (`adcCicloMaxUs`). Se um ciclo passar de `ADC_CICLO_LIMITE_US` (2/3 do período), a task cede 1 ms ao núcleo 0 para proteger o watchdog. Os valores crus ficam em `adc_QRE_E/D` e `adc_LDR`, e o Teste de sensor mostra esses valores para calibrar os limiares.
- O sentido das comparações (abaixo do limiar = detectou) segue o legado sumo-sdk (`profiles/fuego.hpp`). Confirme no Teste de sensor depois de qualquer troca de hardware.

### Atuadores e outros
- **Motores:** ponte H com 4 canais LEDC (8 bits, 0–255, 500 Hz). `moverMotores(esq, dir)` é o **único caminho** para os motores (luta, iniciações, personalizada, RC e parada). Ele aceita valores de -255 a 255, e `(0, 0)` aciona o **freio ativo**. Quando um motor inverte o sentido, o PWM é zerado antes (proteção de cruzamento por zero). Comandos repetidos são ignorados por um cache, que é compartilhado entre tasks dos dois núcleos e por isso é lido e escrito sob o `motorMux`. Com `roboParado` (IR 3), qualquer comando vira freio. Não chame `brakeMotors`/`setMotors` direto de fora do `move.hpp`. Os prints de cada comando só existem com `DEBUG_MOTORES 1` (`defines.hpp`). Deixe em 0 para lutar, porque o `SerialBT` pode bloquear quando o buffer enche.
- **Servo da asa:** LEDC de 16 bits a 50 Hz. Ângulos `SERVO_ANGULO_ABERTO` (padrão 180) e `SERVO_ANGULO_FECHADO` (padrão 90) são configuráveis por NVS. O valor de 180 é proposital: o servo aplica força total até bater no limitador físico. Ele é acionado pelas notificações `openServoHandle`/`closeServoHandle`, e as duas tasks chamam `moverServo()`. Ela pulsa o ângulo e depois **relaxa** (duty 0, sem torque), num tempo proporcional ao curso: `|aberto - fechado| × SERVO_MS_POR_GRAU` (3 ms/°, mínimo `SERVO_TEMPO_MIN_MS`). O relaxamento só acontece se nenhum comando novo chegou nesse meio-tempo (`servoComandoId`), para um comando não cortar o outro. No AUTO, a asa abre no início da iniciação se `abrirAsa` for verdadeiro (etapa ASA do BT). No boot, a asa é fechada. Nenhum código fecha a asa depois de aberta. O RC não aciona a asa.
- **LEDs:** 5 LEDs WS2812 no GPIO 33 (FastLED) e o LED embutido no GPIO 2. O FastLED não é seguro entre tasks, então toda animação segue o padrão `if (!pegarLeds()) return;` → escreve em `leds[]` → `mostrarLeds()`, sob o `ledsMutex`. Depois do IR 3, `pegarLeds()` recusa, e só a `ledsParada()` muda os LEDs. Assim, nenhuma animação apaga o vermelho da parada. **Nunca chame `FastLED.show()` direto.** Toda animação usa os **5 LEDs**:

  | Momento | LEDs |
  |---|---|
  | Setup | 5 vermelhos que viram verdes um a um (`validaSetup`) |
  | IDLE | Verde "respirando" (`ledsHeartbeat`) |
  | Modo engatado / espera do BT ou do PS4 | Cor do modo: AUTO vermelho, RC verde (`ledsModo`) |
  | Seleção BT e Teste de sensor | Painel `indicarSensores`: JS_E, IR_E, LDR, IR_D, JS_D (roxo = viu, laranja = nada) |
  | Senha de boot | Roxo forte por dígito, roxo fraco nos demais. Ao fechar, os 5 na cor do modo escolhido. Ao cancelar, voltam à cor do modo atual |
  | IR 1 (pronto) | 5 vermelhos (`AnnihilationModeLeds`) |
  | IR 2 (largada) e luta | Apagados |
  | IR 3 (parada), em qualquer modo | 5 vermelhos e LED embutido aceso (depois do freio) até reiniciar; nenhuma outra animação sobrescreve |
  | RC com controle conectado | 5 verdes |
- **Receptor IR** (largada) no GPIO 13.

### Pinos
Os pinos de motores, servo, NMOS e sensores são **variáveis `uint8_t`** em `defines.hpp`. Linha e LDR só aceitam pinos do **ADC1 (GPIO 32–39)**, e o menu de pinos recusa outros. Eles podem ser reconfigurados em tempo de execução pelo BT e ficam salvos na NVS. LED (2), receptor IR (13) e fita de LEDs (33) continuam como `#define`, porque o FastLED exige o pino em tempo de compilação. **Nunca troque uma variável de pino por número fixo** e nunca escreva direto num registrador de GPIO fixo: use `directWriteHigh/Low()` e `lerGPIO()`, que tratam pinos acima e abaixo de 32.

## 5. Arquitetura do código

É um **projeto header-only**: `src/main.cpp` inclui todos os `.hpp`, e tudo vira uma única unidade de compilação. Cada header tem include guard. Variáveis globais são definidas direto nos headers (por isso não pode haver um segundo `.cpp`).

| Arquivo | Conteúdo |
|---|---|
| `src/main.cpp` | `setup()` (carrega a NVS, configura pinos, LEDs, IR, tasks), `__init__()` (escolhe o modo no IDLE) e `loop()` (chama `modoAUTO()` ou `modoRC()`, depois se suspende) |
| `include/defines.hpp` | Bibliotecas, objetos globais, handles, pinos, variáveis de sensor, **todos os enums/structs** e as tabelas `PIN_CONFIG`/`THRESHOLD_CONFIG` |
| `include/functions.hpp` | `delayUs`, `lerGPIO`, `directWrite*`, `definirJSumos`, todas as funções de LED e o callback do BT |
| `include/move.hpp` | Motores (`moverMotores`, `brakeMotors`, `stopMotors`), servo e configuração do LEDC |
| `include/sensor_task.hpp` | ISR dos sensores, leitura analógica, monitor de IR, parada (IR 3), `handleIRCommand` (IR 1/2) e a **`fightingLogicTask`** (despacho do combate) |
| `include/fighting.hpp` | **Parâmetros de ajuste**, leituras de adversário e todos os modos de luta (FSMs) |
| `include/openings.hpp` | Macros de iniciação (tabelas `OpeningStep`), Curva de Borda, estratégia personalizada, testes de sensor/motor e `openingsTask` |
| `include/auto_mode.hpp` | Menu de configuração por Bluetooth (etapas), menu de pinos/NVS e `modoAUTO()` |
| `include/boot_mode.hpp` | NVS (modo de boot, pinos, limiares) e **senha de boot por IR** |
| `include/rc_mode.hpp` | Controle por PS4 (`modoRC()`) |

Ordem dos includes: `auto_mode.hpp` inclui `boot_mode`, `sensor_task` e `openings`. `sensor_task` inclui `fighting`. **`logBT()` é definida em `auto_mode.hpp` depois do include de `openings.hpp`**, então `openings.hpp` não pode usar `logBT` (lá se usa `Serial`/`SerialBT` direto). Se uma função precisar ser usada antes de ser definida, crie uma declaração antecipada.

A pasta `scratch/` é rascunho. Ao criar ou renomear arquivos, atualize a lista do `README`.

### Tasks do FreeRTOS

| Task | Núcleo | Prio. | Função |
|---|---|---|---|
| `stopRobot` | 0 | 17 | No IR 3: `travarMotores()`, depois LEDs vermelhos e "PAROU"; reforça o freio a cada 1 s para sempre |
| `analogSensorTask` | 0 | 16 | Liga o timer dos sensores ao iniciar (a ISR fica no núcleo 0). Acordada pela ISR a cada `PERIODO_SENSORES_US`: lê linha e LDR (ADC), aplica limiares e filtro e, durante a luta, acorda a `fightingLogicTask`. Acima do IR porque a linha é a leitura mais crítica |
| `irMonitorTask` | 0 | 15 | Inicia o receptor IR (`IrReceiver.begin`, para a ISR de 50 µs do IRremote ficar no núcleo 0). Decodifica o IR a cada `IR_LEITURA_MS` (20 ms). IR 3 → notifica a `stopRobot` na hora. Os outros comandos vão para a fila `irQueue`, e, com `largadaLiberada`, ela notifica a `handleIRCommand` |
| `openServo` / `closeServo` | 0 | 15 | Movem o servo |
| `handleIRCommand` | 1 | 14 | Acordada pela `irMonitorTask` (só com `largadaLiberada`). IR 1 = pronto, IR 2 = inicia a iniciação e desliga `largadaLiberada`. Nunca é apagada: fica bloqueada, porque a `irMonitorTask` poderia notificar um handle já liberado |
| `fightingLogicTask` | 1 | 14 | Acordada pela `analogSensorTask` ao fim de cada ciclo, **só com `running`**, e despacha o modo de luta com as leituras do ciclo atual |
| `openingsTask` | 1 | 14 | Executa a iniciação e depois liga `running` |

Interrupções: as duas ficam no **núcleo 0**, porque uma ISR roda no núcleo de quem a liga. O timer dos sensores (`readSensors`, timer 0) é ligado pela `analogSensorTask`, e o do IRremote (timer 1, 50 µs) pela `irMonitorTask`. O núcleo 1 fica só com o combate. A cadeia de cada ciclo é: ISR → `analogSensorTask` → `fightingLogicTask`. O `brakeMotors` não tem espera, para o freio nunca bloquear a luta.

Comandos IR: os comandos (menos o IR 3) entram na fila `irQueue` e são lidos com `receberComandoIR()`. `descartarComandosIR()` limpa os que chegaram numa fase em que ninguém os esperava. Cada fase tem um único leitor da fila:
- `bootSenhaProcessar()` nas janelas da senha (ela devolve o comando que não era da senha, que o IDLE usa para 8/9);
- `handleIRCommand` na largada;
- `recebeuDoisIR()` na personalizada.

`running`, `ready`, `roboParado` e `largadaLiberada` são `volatile`, porque são escritas e lidas por tasks de núcleos diferentes. O `loopTask` (menus e RC) roda no núcleo 1 com prioridade 1, e o laço do RC cede 1 ms a cada volta.

Regiões críticas: `sensorMux` (ISR) e `motorMux` (escrita no LEDC e cache do `moverMotores`).

**Atenção:** `ulTaskNotifyTake(pdTRUE, …)` zera o contador, então duas notificações seguidas viram uma. Não use notificação para alternar estado. Para ligar ou desligar os JSumos, use **`definirJSumos(bool)`** (o RC usa `definirJSumos(false)` para entrar em modo furtivo).

## 6. Fluxo de boot e modos

1. **`setup()`:** `carregarPinConfig()` e `carregarMacros()` (NVS) **antes** de qualquer `pinMode`. Depois os LEDs mostram o progresso com `validaSetup` (5 vermelhos que viram verdes um a um), o LEDC dos motores e do servo é configurado (`setupMoveTask`) e o servo é fechado. Só **depois** vêm as tasks de sensor (`setupSensorTask`: cria a `irQueue`; a `irMonitorTask` inicia o receptor IR e a `analogSensorTask` liga o timer dos sensores). Essa ordem garante que nada comanda os motores antes de o LEDC existir, nem mesmo a parada do IR 3.
2. **Modo de boot** gravado na NVS (`carregarBootMode`): IDLE (padrão), AUTO ou RC.
3. **IDLE:** os LEDs ficam "respirando" em verde. No controle IR, **8 → AUTO** e **9 → RC**, sem gravar nada.
4. **Senha de boot por IR:** `4 5 6` seguido de `7` (IDLE), `8` (AUTO) ou `9` (RC). Grava o modo na NVS e reinicia o ESP. Funciona nas janelas de espera: IDLE, pareamento do PS4 e espera ou seleção do BT no AUTO. Os LEDs roxos mostram o progresso.
5. Códigos IR: o botão **N** chega como comando **N-1** (por exemplo, botão 8 = `0x7`).
6. **O controle reenvia o comando enquanto o botão está pressionado**, e cada repetição chega como um comando novo. A senha ignora a repetição do último dígito aceito. Qualquer lógica nova que conte toques precisa tratar isso.

## 7. Modo AUTO

### 7.1 Configuração por Bluetooth (nome "Fuego Wu")
Você envia um índice por mensagem e confirma com ENTER ou esperando 600 ms sem digitar. `?` imprime de novo e `R` reinicia. As etapas são, nesta ordem (enum `ConfigStage`, cuja ordem **tem que** seguir o fluxo, porque o log usa `currentStage > STAGE_X`):

1. **MODO:** 0 Luta, 1 Teste de sensor, 2 Teste de motor, 3 Estratégia personalizada, 4 Configuração (pinos e parâmetros), 5 Macros, 6 Rápida JSumo Ligado, 7 Rápida JSumo Desligado. Testes, Personalizada, Configuração e Macros terminam a seleção aqui e abrem um menu próprio. As **rápidas** aplicam um preset (`aplicarPresetRapido()`: Rampa Simples com IR, asa aberta, Busca Defensiva, finalização por LDR, furtivo da movimentação desativado no 6 e ativado no 7) e só perguntam INICIAÇÃO e DIREÇÃO. Depois da DIREÇÃO, a seleção termina e a luta segue igual à do modo 0.
2. **ADVERSÁRIO:** 0 Rampa Simples com IR, 1 Rampa Simples sem IR, 2 Asa Sem Emissor, 3 Asa Com Emissor (`tipoAdversario`).
3. **ASA:** 0 Fechada, 1 Aberta (`abrirAsa`). Se Aberta, abre no início da iniciação.
4. **INICIAÇÃO:** 0 nenhuma (iterativo puro) ou 1–N, uma das iniciações de `macros[]`: as 10 padrão e as novas salvas pelo histórico da personalizada.
5. **DIREÇÃO:** 0 Esquerda, 1 Direita.
6. **MOVIMENTAÇÃO:** 0 Ofensiva, 1 Defensiva, 2 Linha, 3 Pulsada (`modoLuta` e `modoLutaOriginal`).
7. **FURTIVO (movimentação):** `furtivoMovimentacao`, JSumos desligados durante a luta. Nesse caso as leituras os ignoram. Não há escolha de furtivo para a iniciação: durante a macro, os JSumos ficam **sempre** desligados.
8. **FINALIZAÇÃO:** 0 LDR, 1 Tempo.
9. **TEMPO DA FINALIZAÇÃO:** só aparece com Tempo. São os ms até a troca para a Busca Ofensiva (sugerido `FINAL_TEMPO_MS` = 4000).

**Configuração (modo 4):** a tabela mostra ID, componente e valor. Envie o ID e depois o novo valor. `x` salva na NVS e reinicia. A tabela tem os 13 pinos (IDs 0–12), os 2 limiares (13–14), os 2 ângulos do servo (15–16), a antecedência da asa (17), a velocidade de cruzeiro da Busca Linha (18), os tempos do Retorno (19–22: recuo lateral, giro lateral, recuo frontal, giro frontal) os coeficientes de ré do RC em % (23–24) e o período do timer dos sensores em µs (25). Para adicionar um item configurável, basta incluir uma linha em `PIN_CONFIG` ou `THRESHOLD_CONFIG` (`defines.hpp`), com nome, ponteiro para a variável, chave da NVS e, nos valores, o mínimo e o máximo aceitos pelo menu. A tabela **não guarda o padrão**: o padrão é o valor com que a variável é inicializada no código, e ele é lido no boot. O menu e a NVS funcionam sozinhos com a linha nova. Se a variável for definida em outro header, declare-a com `extern` antes da tabela (como `LINHA_TRESHOLD`, `velBuscaLinhaCruzeiro` e `coefReversoEsq/Dir`). A tabela só guarda inteiros, então coeficientes vão em porcentagem.

**Macros (modo 5):** só visualiza, não altera nada. Lista as iniciações de `macros[]` com os mesmos índices da etapa INICIAÇÃO. O índice **0 imprime todas**. Ao receber um índice, imprime os passos atuais (`Vel E | Vel D | Tempo`) das versões esquerda e direita (uma tabela só quando as duas são iguais). Para a Curva de Borda, que não é matriz, imprime os passos com as constantes `curvaBorda*`. `?` reimprime a lista e `R` reinicia.

**Estratégia personalizada (modo 3):** menu próprio (`menuPersonalizada()`) para testar movimentos. **Nunca entra em luta** (`running` fica falso) e, ao fim de cada movimento, volta ao menu. Só sai reiniciando (`R`). Os passos são `vel_esq,vel_dir,tempo_ms` (velocidades de -255 a 255, tempo de 1 a `PASSO_MAX_MS`, até `MAX_STEPS - 1` passos).
- **0 - Movimento completo:** recebe a matriz inteira e só anda ao receber **2**, pelo BT (linha "2") ou pelo botão 2 do controle IR. `x` cancela.
- **1 - Por passo:** cada linha recebida é executada na hora (anda o tempo do passo e freia). `x` encerra e imprime a matriz.
- **2 - Histórico:** as últimas `HISTORICO_TAMANHO` (5) matrizes enviadas pelos modos 0 e 1, gravadas na NVS (1 = a mais nova). Cada uma pode ser executada de novo (espera o 2, e não entra de novo no histórico), salva numa iniciação existente (lado esquerdo, direito ou os dois com o direito espelhado; nome novo ou `.` para manter) ou salva numa **iniciação nova** (nome obrigatório; o outro lado recebe a matriz espelhada). A Curva de Borda não pode ser substituída.
- As linhas são lidas por `pollLinhaBT()`/`esperarLinhaBT()`, que fecham a linha no ENTER ou depois de `BT_NUM_TIMEOUT_MS` sem novos caracteres.

**Teste de sensor:** a cada 200 ms imprime uma tabela `Pino | Sensor | Leitura` no BT e no Serial (linha e LDR também mostram o ADC e o limiar), e espelha os sensores nos LEDs.

### 7.2 Largada
Ao fim da seleção, o `modoAUTO` liga `largadaLiberada`, e a `irMonitorTask` passa a repassar os comandos para a `handleIRCommand`. **IR 1** → `ready` e LEDs vermelhos. **IR 2** → apaga os LEDs e notifica a `openingsTask`. Ela chama `resetFightingState()`, executa a iniciação (uma de `macros[]`, a Curva de Borda ou nada), marca `tempoFighting` e liga `running = true`.

**IR 3, a qualquer momento → parada definitiva:** a `irMonitorTask` notifica a `stopRobot`, que chama `travarMotores()`. Essa função liga a trava `roboParado` (nunca volta a falso), zera `running`, freia e, depois de `PARADA_REFORCO_MS`, freia de novo direto no LEDC. O segundo freio cobre uma task que já tinha passado pela checagem quando a trava ligou. Com a trava ligada:
- qualquer `moverMotores` vira freio;
- a `openingsTask` não liga `running` se o IR 3 chegou durante a iniciação;
- a Curva de Borda sai da espera pela linha;
- a `handleIRCommand` ignora um IR 2 posterior.

Só depois do freio a `stopRobot` acende os LEDs (`ledsParada()`) e imprime, para nada que possa bloquear vir antes. A trava também vale no IDLE, no RC e na personalizada. Os LEDs ficam vermelhos, e o AUTO avisa no BT ("ROBO TRAVADO PELO IR 3") ao conectar, ao fim da seleção e antes de executar uma personalizada.

### 7.3 Iniciações (`openings.hpp`)
Cada macro é um vetor de `OpeningStep {velEsq, velDir, delayMs}` terminado em `{0,0,0}`, com versões esquerda e direita. As matrizes do código são só os **padrões** (`MACROS_PADRAO`: Frentão, Frentinho, Curva, Curvão, Em V, Vzinho, Vzão, Giro, Desempate e Curva de Borda). No boot, `carregarMacros()` copia esses padrões para `macros[]` (RAM, até `MAX_MACROS`) e aplica por cima o que estiver salvo na NVS: posições sobrescritas e posições novas. **Tudo que executa, lista ou imprime iniciações usa `macros[]` e `numMacros`.** O índice do BT é a posição + 1. Para mudar o padrão de uma macro, edite a matriz no código, mas uma versão salva na NVS continua valendo mais. A **Curva de Borda** (`macros[i].curvaBorda`) é uma função especial, com os valores nas constantes `curvaBorda*` de `openings.hpp`: gira, avança até achar a linha, lendo `value_QRE_*` direto (não usa `viuLinha`, que só é atualizado com `running == true`), recua e gira de volta. Toda iniciação com movimento começa por `prepararIniciacao()`: desliga os JSumos e, se `abrirAsa`, manda a asa abrir e espera `ASA_ANTECEDENCIA_MS` (50 ms, configurável por NVS) antes de mover os motores. O servo roda na própria task (núcleo 0), então termina o curso em paralelo com a macro: 0 deixa tudo simultâneo, e um valor igual ao curso completo (~270 ms) faz a asa abrir toda antes de o robô andar. No fim, os JSumos seguem `furtivoMovimentacao`. No iterativo puro, sem macro, a asa abre com a mesma antecedência antes de a luta começar.

### 7.4 Despacho do combate (`fightingLogicTask`), por prioridade
1. `running == false` → não faz nada.
2. **FIM_TEMPO:** depois de `tempoFinalizacaoMs` desde o fim da iniciação, Defensiva, Linha ou Pulsada viram **Busca Ofensiva** de vez.
3. **Linha detectada** → guarda `viuLinha` (ESQ/DIR/AMBAS) → **Retorno**. Vem antes do LDR de propósito: a linha sempre vence.
4. **FIM_LDR:** com o LDR ativo e fora do Retorno, vai para frente a 255 e pula o resto do ciclo. Quando o LDR apaga depois de ter atacado → **Desengate**.
5. Se nada disso aconteceu → executa `modoLuta`.

### 7.5 Leitura do adversário (`fighting.hpp`)
As três buscas usam `lerAdversario()`. Ela executa os **giros** (iguais em todas as buscas) e devolve `leituraFrente`, `leituraGiro` ou `leituraNada`. Cada busca só decide o que fazer com "frente" e com "nada" (nada → **Busca Linha**). Movimentos: **girar em torno da roda** = a roda do lado da curva fica parada e a outra vai a `velGiroRoda`. **Girar no eixo** = rodas em sentidos opostos a `velGiroEixo`.

**Rampa (com e sem IR) e Asa Sem Emissor**, função `leituraRampa`:
| Leitura | Normal | Furtivo |
|---|---|---|
| 2 IRs | Frente | Frente |
| 1 IR (com ou sem JSumo) | Gira em torno da roda para o lado do IR | Gira no eixo para o lado do IR |
| Só JSumo | Gira no eixo para o lado do JSumo | — (JSumos ignorados) |

**Asa Com Emissor**, função `leituraAsaComEmissor`, FSM `EstadoFlanco`:
- **Normal:** 1 IR ou 2 IRs → `flancoGiroEixo`. Gira no eixo (com 2 IRs, para o lado **oposto ao da Direção da macro**. Com 1 IR, para o lado **oposto ao do IR**) até um JSumo ver, sem parar no ponto cego. Então `flancoPivoFora`: gira em torno da roda para longe do JSumo enquanto ele vê. Quando ele para de ver, `flancoPivoVolta`: gira em torno da outra roda de volta. Isso se repete. Só o JSumo vendo, com a FSM inativa → gira no eixo para o lado dele.
- **Furtivo:** 2 IRs → frente (e cancela a manobra). 1 IR → zigue-zague `PivoFora`/`PivoVolta` usando esse IR.
- Depois de `flancoTimeoutMs` (7 s) de manobra → `flancoBuscaLinha` até o próximo Retorno, que reinicia a FSM.

### 7.6 Modos de luta
- **Busca Ofensiva:** frente a `velOfensivaFrente` (255).
- **Busca Defensiva:** frente a `velDefensivaFrente` (51). Depois de `defensivaEscalaMs` sem tocar a linha, vira Busca Linha de vez.
- **Busca Pulsada:** com frente, pulsa `velPulso` por `pulsoDuracaoMs`, para por `pulsoEsperaMs`, até `pulsoQuantidade` pulsos. Qualquer giro da leitura **reinicia a contagem** (`inicioPulsada`). Quando os pulsos acabam ou não há leitura → Busca Linha.
- **Busca Linha:** cruzeiro a `velBuscaLinhaCruzeiro` (140, configurável por NVS). A cada 4 s sem linha dá uma arrancada de 90 ms a 255. Depois de 7 s sem tocar a linha entra em **Carga Total** (255) até o fim. Os relógios contam desde o último Retorno.
- **Retorno:** ré por `retornoRecuoLateralMs` (linha vista por 1 sensor) ou `retornoRecuoFrontalMs` (pelos 2). Depois gira **para o lado oposto ao da linha** (com os 2 sensores, para o lado do último adversário visto) por `retornoGiroLateralMs` ou `retornoGiroFrontalMs`. Os quatro tempos são configuráveis por NVS. Zera os relógios de linha e a FSM de flanco e volta a `modoLutaOriginal`.
- **Desengate:** "S" com curva 140/255 por 50 ms e 255/140 por 100 ms, depois volta ao modo original.

Todos os valores ficam no bloco **PARÂMETROS DE AJUSTE** no topo de `fighting.hpp`. Estado novo de luta precisa ser zerado em `resetFightingState()`.

## 8. Modo RC

`modoRC()` conecta ao controle PS4 pelo MAC `FUEGO`, entra em modo furtivo e espera o pareamento (a senha de boot por IR funciona aqui). Quando o controle conecta: LED do controle e fita verdes. Comandos:
- **R2/L2:** frente e ré. Na ré, cada motor é multiplicado pelo seu coeficiente, `coefReversoEsq`/`coefReversoDir` (em %, padrão 100 = sem atenuação, configuráveis por NVS).
- **Analógico esquerdo:** curva, com `coefAtenuacao`.
- **X:** turbo 255.
- **Triângulo:** limita a velocidade a 180.
- **Seta esquerda / direita:** gira no eixo para o lado da seta (`velMacroGiro` = 255 por `tempoMacroGiroMs` = 180 ms) e freia. São as únicas macros do RC.

Se o controle desconectar, os motores são freados.

## 9. NVS

**Configuração** (namespace `fuego_cfg`):

| Chave | Tipo | Conteúdo |
|---|---|---|
| `boot` | uchar | Modo de boot (0 IDLE, 1 RC, 2 AUTO) |
| `p_lp`, `p_ln`, `p_rp`, `p_rn`, `p_sv`, `p_nm`, `p_jd`, `p_je`, `p_id`, `p_ie`, `p_ld`, `p_le`, `p_lr` | uchar | Pinos (ausente = padrão) |
| `t_li`, `t_lr`, `t_sa`, `t_sf` | int | Limiar da linha, limiar do LDR, servo aberto, servo fechado |
| `t_aa` | int | Antecedência da asa sobre a macro (ms) |
| `t_vl` | int | Cruzeiro da Busca Linha |
| `t_rrl`, `t_rgl`, `t_rrf`, `t_rgf` | int | Retorno: recuo e giro com linha lateral, recuo e giro com linha frontal (ms) |
| `t_ce`, `t_cd` | int | Coeficiente de ré esquerdo e direito do RC (%) |
| `t_ps` | int | Período do timer de leitura dos sensores (µs) |

O padrão de cada item é o valor da variável no código. `carregarPinConfig()` guarda esse valor (`padraoPinos`/`padraoValores`) antes de aplicar a NVS. `salvarPinConfig()` grava só o que é diferente do padrão e **remove** a chave quando o valor volta ao padrão. Um valor salvo na NVS vale mais que o do código. Para um padrão novo do código valer, o item não pode estar salvo: ajuste-o pelo menu para o valor do código (isso apaga a chave) ou apague a flash. As seleções de estratégia do BT **não** são salvas, e precisam ser feitas de novo a cada boot.

**Iniciações e histórico** (namespace `fuego_mac`, `NVS_NAMESPACE_MACROS`), em blobs compactos (`PassoNVS`: int16/int16/uint16):

| Chave | Tipo | Conteúdo |
|---|---|---|
| `n` | uchar | Quantidade de iniciações (padrão + novas) |
| `m0`…`m19` | blob `MacroNVS` | Iniciação da posição (base 0): nome e passos dos dois lados. Só existe se foi sobrescrita ou criada |
| `hq`, `hp` | uchar | Quantidade de itens do histórico e próxima posição do buffer circular |
| `h0`…`h4` | blob `HistoricoNVS` | Matrizes do histórico da personalizada |

Ainda não existe opção para voltar uma iniciação ao padrão ou apagar uma nova: isso só se faz apagando a flash.

## 10. Convenções de código

- Visual: blocos com cabeçalhos `//====//TITULO//====//`, `#pragma region`/`endregion` e comentários de fim de linha alinhados na coluna ~47. Siga o estilo do arquivo que estiver editando.
- Nomes em português, em camelCase para variáveis e funções. Constantes de hardware em MAIÚSCULAS.
- Velocidades sempre em PWM de 0 a 255. O código legado (sumo-sdk/Fumacinha) usava 0–100, então converta multiplicando por 2,55.
- Saídas de debug vão para `Serial` e `SerialBT` (no AUTO, use `logBT` quando estiver disponível).
- Comentários curtos, explicando o **porquê** e não o quê.

## 11. Armadilhas e pendências conhecidas

- **`viuLinha` só é atualizado com `running == true`.** Fora da luta (por exemplo, nas iniciações), leia `value_QRE_E/D` direto.
- **O watchdog de tarefas reinicia o ESP** (`CONFIG_ESP_TASK_WDT_PANIC=y`, 5 s, vigiando a IDLE do núcleo 0). Qualquer task que ocupe 100% do núcleo 0 causa reset. Isso já aconteceu: com `analogRead` (que no core Arduino 2.x refaz `pinMode` e atenuação a cada chamada), 3 leituras a cada 300 µs travavam o núcleo 0, e o robô reiniciava ~5 s depois do boot. Tasks acordadas pela ISR precisam ser bem mais curtas que o período dos sensores, e baixar `PERIODO_SENSORES_US` aumenta a carga dos dois núcleos.
- **Carga Total** não desliga mais até o fim da luta. Isso é **intencional** por decisão do mantenedor. Não "corrija".
- **Novos prints no caminho do combate:** evite. Se forem necessários para depurar, coloque atrás de uma flag como `DEBUG_MOTORES`.
- O legado de referência é o repositório sumo-sdk (perfil Fumacinha: `FumacinhaAuto.cpp/.hpp`). A lógica de luta do Fuego foi portada de lá e depois adaptada à geometria dos sensores e aos tipos de adversário do Fuego. Não "corrija" o Fuego para ficar igual ao legado sem confirmar com o mantenedor.
