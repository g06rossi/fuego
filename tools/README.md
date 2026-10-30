# Fuego Sumo Tool (FST)

O FST reúne as ferramentas do Fuego no PC: a **coleta das lutas** para análise e a **passagem do código para o robô** (controle PS4, compilação, gravação, limpeza da flash e commit no GitHub). Ele abre pelo `tools/fst.bat`.

## Coleta de lutas

`coletar_luta.py` copia para o PC os rounds que o Fuego gravou. Ele salva cada luta em CSV, junto com o resultado e o tipo de vitória ou derrota, para a análise das estratégias.

## O que o robô grava

- **Quando:** a gravação começa no **fim da iniciação**, quando a luta iterativa começa. A iniciação em si não é gravada.
- **Quanto tempo:** até **30 s**, ou até o **IR 3**, o que vier primeiro.
- **Frequência:** uma amostra a cada **10 ms** (100 Hz). Cada amostra guarda:
  - **Linhas e LDR:** o **menor** valor do ADC nos 10 ms, para nenhuma passada pela linha se perder.
  - **IRs, JSumos e detecções de linha e LDR:** `1` se o sensor **viu pelo menos uma vez** nos 10 ms.
  - **Motores e estado:** o último PWM de cada motor, o modo de luta, o estado do flanco, a Carga Total e o ataque por LDR.
- **Eventos:** as trocas de modo e de flanco, a linha, o LDR, a Carga Total, o ataque por LDR e a parada também ficam numa lista à parte, com o tempo exato.
- **Modos que gravam:** só as lutas, ou seja, o modo 0 e as rápidas 6 e 7. Testes, personalizada e o log de sensores não gravam.

## Rounds guardados no robô

Durante a luta, a gravação fica na RAM. Logo depois do **IR 3**, com os motores já travados, o robô guarda a luta na flash como um **round numerado**. Assim dá para lutar até **5 rounds**, desligando ou reiniciando o robô entre eles, e coletar tudo de uma vez no fim.

- **Numeração:** o round 1 é a primeira luta desde a última limpeza, o 2 a segunda, e assim por diante. A exportação sai sempre nessa ordem, do mais antigo ao mais novo.
- **Para saber quem é quem:** ao conectar o BT, o robô avisa "Esta luta será o ROUND N". Anote o adversário com esse número. Depois do IR 3, ele confirma: "Luta guardada como ROUND N".
- **Slots cheios:** com 5 rounds guardados, a luta nova **sobrescreve o round mais antigo**, e o BT avisa antes ("sobrescreve o round 1, exporte antes").
- **Limpeza:** depois de salvar os arquivos, a ferramenta envia `l` e o robô apaga **só os rounds que acabou de exportar**. Na próxima luta, a numeração volta ao 1.
- **Sem IR 3, sem round:** a luta só vai para a flash com o IR 3. Se o robô for desligado antes dele, a luta se perde.
- **Gravar o firmware de novo** (`pio run -t upload`) não apaga os rounds. `pio run --target erase` apaga.

## Instalação (uma vez)

1. Python 3 instalado.
2. Mais nada: o `pyserial` e o `libusb0.dll` já estão em `tools/lib` (veja `tools/lib/README.md`).

Fora da pasta `tools`, o FST usa só o que não cabe no repositório: o **Python** (com o tkinter, que vem no instalador padrão), o **PlatformIO** da extensão do VS Code (5 GB de compiladores, só para compilar e gravar) e o **git** (só para o commit).

## Jeito mais fácil: a interface

Dê dois cliques em **`tools/fst.bat`**. Na primeira vez, ele confere se o Python e o tkinter estão instalados. Depois abre o FST.

A ferramenta abre com uma introdução de 7 s na própria janela:
1. A arte do Fuego (a mesma do cabeçalho do `src/main.cpp`, centralizada) surge sobre o fundo escuro.
2. Ela fica na tela e depois some.
3. A tela fica vazia por 0,5 s.
4. As abas aparecem.

Um clique ou ESC pula a introdução. Ela pode ser desligada na aba **Configurações**.

A janela tem oito abas e uma barra de progresso:

| Parte | Função |
|---|---|
| **Fuego** | Imagem do robô, escolhida em Configurações: a ASCII art do `tools/imagens/fuego.txt` (cada caractere vira um bloco de cor e a imagem se ajusta à janela), a foto ou a foto tratada (fundo removido e tons de fogo) |
| **Guia** | Mostra este documento |
| **Coleta** | Coleta os rounds e grava o resultado. Veja o passo a passo abaixo |
| **Firmware** | Controle PS4, compilar, gravar e limpar a flash do robô e commit no GitHub. Veja a seção "Aba Firmware" |
| **Parâmetros** | Edita no código os parâmetros das FSMs de luta e os itens da NVS. Veja a seção "Aba Parâmetros" |
| **Dados** | Mostra em tabela os CSVs da pasta de lutas. Veja os detalhes abaixo |
| **NVS** | Mostra a configuração do robô (pinos e parâmetros) ao lado dos valores do código e copia os valores do robô para o código |
| **Configurações** | **Modo noturno** (padrão), **Animação de abertura** (ligada por padrão) e **Abertura em degradê**, do amarelo ao vermelho (padrão; desligada, a arte fica só vermelha). **Imagem da aba Fuego** (ASCII do robô, foto ou foto tratada) e **Imagem da abertura** (ASCII do Fuego do `main.cpp`, ASCII do robô, foto ou foto tratada). Quando a abertura mostra o robô, o nome "Fuego" da ASCII art do `main.cpp` aparece embaixo dele. **Ver a abertura agora** mostra a introdução de novo. As opções ficam em `tools/preferencias.json`, fora do git |
| **Barra de progresso** (embaixo, em todas as abas) | Avança enquanto os rounds chegam pela USB. Ao lado, o status da última ação |

**Coletar na aba Coleta:**
1. Conecte a USB e escolha a porta. Use "Atualizar portas" se ela não aparecer. O robô pode estar em qualquer modo, menos pronto para lutar (depois do IR 1/IR 2 e antes do IR 3).
2. Clique em **Coletar pela USB**. Se os rounds foram exportados pelo BT e salvos num `.txt`, use **Abrir exportação do BT**.
3. O quadro "Rounds recebidos" tem uma lista com os rounds na ordem das lutas (`1 de 3: round 1 - adversário, iniciação, movimentação, duração`). Escolha um para ver a configuração e o resumo dele.
4. Preencha o resultado, o tipo e a observação **de cada round**: troque o round na lista e preencha. A ferramenta guarda o que foi preenchido em cada um.
5. Clique em **Salvar todos os rounds**. Se algum ficou sem resultado, ela pergunta antes.
6. Com **Limpar os rounds do robô depois de salvar** marcado (padrão), ela envia `l` depois de salvar, e o robô apaga os rounds exportados. Só funciona na coleta pela USB. **Descartar** joga os rounds recebidos fora sem salvar, e eles continuam no robô.

**Ver os dados na aba Dados:**
- **Arquivo:** escolhe entre o `lutas.csv` e as séries de cada luta.
- **Ordenar:** clique no cabeçalho de uma coluna. Clicar de novo inverte a ordem.
- **Filtro:** mostra só as linhas que contêm o texto digitado, em qualquer coluna.
- **Da luta para as séries:** no `lutas.csv`, selecione uma luta e use **Ver amostras** ou **Ver eventos** para abrir a série dela. Dar dois cliques na luta abre as amostras.
- **Abrir pasta:** abre a pasta das lutas no Explorer.

**Configuração na aba NVS:**
O robô pode estar em qualquer modo (IDLE, menus, RC ou parado pelo IR 3), menos no meio da luta.
1. Conecte a USB e escolha a porta na aba Coleta.
2. Clique em **Ler do robô (USB)**. A tabela mostra, para cada item:

   | Coluna | Significado |
   |---|---|
   | Robô | Valor em uso no robô |
   | Padrão gravado | Padrão com que o firmware do robô foi compilado |
   | Código | Valor de inicialização da variável no código do repositório |
   | Da NVS | Se o valor veio da NVS |
   | Faixa | Mínimo e máximo aceitos pelo menu de Configuração |
   | Local | Arquivo e linha da variável no código |

3. As cores indicam a situação:
   - **Amarelo:** o robô usa um valor diferente do código, salvo pelo menu de Configuração.
   - **Vermelho:** o firmware gravado foi compilado com outro padrão. O código mudou desde a última gravação.
4. Selecione as linhas (Ctrl/Shift para várias), ou use **Selecionar diferentes**, e clique em **Aplicar valores do robô ao código**.
5. Confira a prévia e confirme. A ferramenta troca **só o número** da inicialização de cada variável e mantém o resto da linha.
6. Compile e grave o firmware (`pio run -t upload`). Só a partir daí os valores novos passam a ser o padrão.

Mesmo sem o robô conectado, a aba mostra os valores do código e onde cada variável está. **Recarregar código** relê os arquivos depois de uma edição manual. As iniciações e o histórico da personalizada não entram nesta aba.

O passo a passo abaixo faz o mesmo pelo terminal.

## Aba Firmware

Reúne o que é preciso para passar o código para o robô. Cada ação escreve o que está fazendo no **Log** da aba (os comandos executados e a saída deles) e avança a barra de progresso. **Cancelar** interrompe a compilação, a gravação ou o git em andamento.

### 1. Controle PS4 (como o SixaxisPairTool)
O controle só se conecta ao aparelho cujo MAC está gravado nele, o **mestre**. No Fuego, esse MAC é o `FUEGO` de `include/rc_mode.hpp`, que o `PS4.begin()` usa como MAC do Bluetooth do robô.
1. Conecte o controle **pelo cabo USB** (um cabo de dados, não só de carga). Feche o DS4Windows ou a Steam, se estiverem abertos.
2. **Ler controle:** mostra o modelo, o MAC do próprio controle e o mestre atual, e diz se ele é igual ao do código.
3. **Mestre novo** já vem com o MAC do código. Mude só se quiser outro.
4. **Atualizar mestre:** grava o mestre novo no controle e lê de novo para conferir.
5. Desconecte o cabo e ligue o robô no RC: o controle deve conectar sozinho.

Funciona com o DualShock 4 (1ª e 2ª geração), só no Windows, sem instalar nada. Se o SixaxisPairTool já foi usado no controle, ele trocou o driver do controle pelo libusb-win32. Nesse caso, a FST fala com o controle por esse driver, como o SixaxisPairTool, e o modelo aparece com "(driver libusb)". O mesmo pelo terminal: `python tools/ps4_mac.py` (lê) ou `python tools/ps4_mac.py aa:bb:cc:dd:ee:ff` (grava).

### 2. Código no robô
Escolha a porta do robô (a lista é a mesma da aba Coleta).
- **Compilar:** só compila (`platformio run`) e mostra os erros no log. Não precisa do robô.
- **Compilar e gravar:** compila e grava no robô (`platformio run -t upload`). Se parar em "Connecting", segure o botão **BOOT** do ESP32 até a gravação começar.
- **Limpar a flash antes de gravar:** apaga a flash inteira e depois grava.
- **Limpar flash:** apaga a flash inteira (`platformio run -t erase`). O robô fica sem programa até gravar de novo.

> ⚠️ **Limpar a flash apaga tudo:** a NVS (pinos, parâmetros, iniciações salvas e histórico da personalizada) e os **rounds guardados**. Colete os rounds antes. Gravar o firmware sem limpar mantém tudo isso.

A ferramenta usa o PlatformIO da extensão do VS Code (`~/.platformio/penv`). Nenhum monitor serial pode estar com a porta aberta.

### 3. Versão no GitHub
1. **Atualizar** lê a branch atual, as branches existentes e os arquivos alterados (que aparecem no log).
2. **Branch:** escolha uma da lista ou digite um nome novo.
   - Branch atual: o commit vai nela.
   - Outra branch existente: a ferramenta troca para ela levando as alterações. Se o git avisar que arquivos seriam sobrescritos, a troca é cancelada e nada muda.
   - Nome novo: cria a branch a partir da atual.
3. Escreva o **comentário** do commit.
4. **Commitar:** mostra os arquivos e pede confirmação. Depois adiciona **todos** os arquivos alterados (`git add -A`, respeitando o `.gitignore`), faz o commit e, com **Enviar para o GitHub** marcado, envia (`git push -u origin <branch>`).

Na primeira vez, o Windows pode abrir a janela de login do GitHub.

## Aba Parâmetros

Edita no código os valores de ajuste sem abrir os arquivos. A tabela mostra:
- **Parâmetros das FSMs de luta:** todo o bloco "PARÂMETROS DE AJUSTE" do `include/fighting.hpp` (velocidades, tempos e limiares de cada busca, do Retorno e do Desengate), agrupado pelas seções do próprio arquivo. Um parâmetro novo no bloco aparece sozinho, desde que siga o formato `tipo nome = numero;`.
- **Configuráveis pela NVS (outros arquivos):** os pinos e os parâmetros do menu de Configuração que ficam fora do `fighting.hpp` (servo, LDR, asa, período dos sensores, ré do RC).

Como usar:
1. Dê **dois cliques** (ou ENTER) numa linha e digite o valor novo. ENTER confirma e ESC cancela.
2. O valor é conferido na hora: velocidades de 0 a 255, tempos sem valor negativo e itens da NVS na faixa do menu de Configuração. As linhas alteradas ficam **amarelas**.
3. **Aplicar no código** mostra a lista do que muda e troca **só o número** de cada linha, mantendo o comentário alinhado. **Descartar alterações** desfaz o que ainda não foi aplicado.
4. Compile e grave na aba **Firmware** para os valores valerem no robô.

> ⚠️ Nos itens da NVS (coluna NVS preenchida), um valor salvo no robô vale mais que o do código. Para o valor novo do código valer, ajuste o item pelo menu de Configuração ou limpe a flash.


## Passo a passo (terminal)

### 1. Lute normalmente
1. Selecione a luta pelo BT ("Fuego Wu"): modo 0, 6 ou 7.
2. **IR 1** (pronto) e **IR 2** (largada).
3. **IR 3** ao fim do round.

O BT avisa "Luta guardada como ROUND N". Agora o robô pode ser desligado. Repita para os próximos rounds (até 5).

### 2. Conecte a USB
- Ligue o cabo USB no robô.
- **Feche qualquer monitor serial aberto**, como o do PlatformIO ou o do VS Code. O script precisa da porta só para ele.
- Se for usar outro monitor, deixe DTR e RTS desligados (`pio device monitor --rts 0 --dtr 0`). Senão, o ESP reinicia ao abrir a porta. Os rounds guardados não se perdem, mas uma luta que não chegou à flash sim. O script já abre a porta assim.

### 3. Descubra a porta
```
python tools/coletar_luta.py --listar
```
Procure a porta do conversor USB do ESP32, por exemplo `COM5  Silicon Labs CP210x`.

### 4. Colete a luta
```
python tools/coletar_luta.py --porta COM5
```
1. O script envia o pedido (`d`) e recebe todos os rounds (~11 s por round de 30 s). Ele lista os rounds na ordem das lutas e depois mostra o resumo de cada um, começando pelo round 1.
2. Para cada round, responda o **resultado**: `v` vitória, `d` derrota ou `e` empate. ENTER deixa em branco.
3. Escolha o **tipo**, que diz o que aconteceu e com quem:

   | # | Tipo |
   |---|---|
   | 0 | Fuego saiu sozinho |
   | 1 | adversário saiu sozinho |
   | 2 | Fuego foi rampado |
   | 3 | adversário foi rampado |
   | 4 | Fuego acertado lateralmente |
   | 5 | adversário acertado lateralmente |
   | 6 | Fuego perdeu posição de vantagem (estava propício para ganhar, mas alguma movimentação mudou isso) |
   | 7 | adversário perdeu posição de vantagem |
   | 8 | decisão (tempo ou juízes) |
   | 9 | outro |

4. Escreva uma **observação**, se quiser. Por exemplo: "adversário novo, asa larga".

5. Depois de salvar todos, o script envia `l` e o robô apaga os rounds exportados. Com `--manter`, eles ficam no robô e podem ser coletados de novo.

Cada round vira os arquivos `luta_AAAAMMDD_HHMMSS_rN_*`, todos com a hora da coleta. O `N` é o número do round no robô.

### Alternativa: exportar pelo Bluetooth
Logo depois do IR 3 (ainda na tela da luta), envie `d` pelo terminal BT, e todos os rounds saem no próprio terminal. `l` apaga os exportados. Depois de reiniciar, a exportação só sai pelo Serial.
1. Salve o texto recebido num arquivo, por exemplo `bt.txt`. Ele precisa ter as linhas `#ROUNDS`, `#LUTA_INICIO`, `#LUTA_FIM` e `#ROUNDS_FIM`.
2. Processe o arquivo:
   ```
   python tools/coletar_luta.py --arquivo bt.txt
   ```
Se o arquivo tiver mais de uma exportação, o script usa a última. Ele não limpa o robô: envie `l` pelo BT depois de conferir os arquivos.

## Imagens do robô

A foto original fica em `tools/imagens/fuego_original.png`. As versões usadas pela interface são geradas a partir dela:

```
python tools/gerar_imagens.py
```

O script gera a foto e a foto tratada em dois tamanhos, um para a aba Fuego e outro para a abertura. No tratamento, o fundo cinza do render fica transparente, e o robô ganha tons de fogo (as partes que já têm cor misturam as duas). Rode de novo sempre que trocar a foto original. Os ajustes (faixa do fundo e cores do degradê) são constantes no topo do script.

## Exemplos para teste

A pasta `tools/exemplos/` tem duas exportações simuladas, no mesmo formato que o robô envia. Elas servem para testar a ferramenta sem o robô: na aba Coleta, use **Abrir exportação do BT**, ou rode `--arquivo` no terminal.

| Arquivo | Luta |
|---|---|
| `luta_simulada_rampa.txt` | Rampa Simples com IR, Busca Linha, finalização por LDR. Tem Retorno lateral, giro pelo IR, ataque e desengate por LDR, arrancada, detecção pelo JSumo, empurrão e Retorno frontal. IR 3 aos 12,5 s |
| `luta_simulada_asa.txt` | Asa Com Emissor, Busca Defensiva, finalização por tempo (4 s). Tem a manobra de flanco, a troca para a Busca Ofensiva e dois Retornos. Vai até o fim dos 30 s |

Os valores são inventados. Não use esses arquivos para análise.

## Opções

| Opção | Função |
|---|---|
| `--porta COM5` | Coleta pela USB |
| `--arquivo bt.txt` | Lê uma exportação salva em texto |
| `--listar` | Lista as portas seriais |
| `--pasta caminho` | Pasta de saída (padrão: `lutas/` na raiz do repositório) |
| `--baud 115200` | Velocidade da serial (tem que bater com o firmware) |
| `--sem-resultado` | Não pergunta resultado, tipo nem observação |

## Arquivos gerados

Ficam em `lutas/`, pasta ignorada pelo git. `<data>` é a data e a hora do PC na coleta, porque o robô não tem relógio.

| Arquivo | Conteúdo |
|---|---|
| `luta_<data>_rN_amostras.csv` | Série temporal da luta, uma linha a cada 10 ms |
| `luta_<data>_rN_eventos.csv` | Eventos com o tempo exato |
| `luta_<data>_rN_bruto.txt` | Texto recebido, sem alteração (backup) |
| `lutas.csv` | **Uma linha por luta**, acrescentada a cada coleta. É a tabela principal da análise |

### `luta_<data>_rN_amostras.csv`
| Coluna | Significado |
|---|---|
| `t_ms` | Tempo desde o fim da iniciação |
| `linhaE`, `linhaD`, `ldr` | Menor ADC na janela (0–4095; menor = mais branco/mais escuro) |
| `jsE`, `irE`, `irD`, `jsD` | 1 = o sensor viu o adversário na janela |
| `viuLinhaE`, `viuLinhaD`, `viuLdr` | 1 = o detector marcou linha ou sombra na janela |
| `pwmE`, `pwmD` | Último comando dos motores (-255 a 255; 0/0 = freio) |
| `modo` | `ofensiva`, `defensiva`, `linha`, `pulsada`, `retorno`, `desengate` |
| `flanco` | Estado da FSM da Asa Com Emissor (`inativo`, `giroEixo`, `pivoFora`, `pivoVolta`, `buscaLinha`) |
| `cargaTotal`, `ldrAtacando` | 1 = ativo |

### `luta_<data>_rN_eventos.csv`
Colunas `t_ms,evento,valor`. Os eventos possíveis são:

| Evento | Valor |
|---|---|
| `modo` | Novo modo de luta |
| `flanco` | Novo estado do flanco |
| `linhaE`, `linhaD`, `ldr` | 1 quando começou a ver, 0 quando parou |
| `cargaTotal`, `ldrAtaque` | 1 quando ligou, 0 quando desligou |
| `parada` | IR 3 |
| `fim` | Os 30 s acabaram |

### `lutas.csv`
| Grupo | Colunas |
|---|---|
| Identificação | `luta` (prefixo dos arquivos da luta), `data` (da coleta), `round` (número do round no robô; vazio nas lutas coletadas antes dos rounds) |
| Estratégia | `adversario`, `asa`, `iniciacao`, `direcao`, `movimentacao`, `furtivo`, `finalizacao`, `tempoFinalizacaoMs` |
| Calibração | `limiarLinha`, `quedaLinha`, `limiarLdr`, `periodoSensoresUs`, `janelaMs` |
| Resultado | `resultado`, `tipo`, `observacao` |
| Resumo | `duracao_ms`, `fim_gravacao` (`ir3` ou `30s`), `retornos`, `linhas_vistas`, `ataques_ldr`, `primeira_deteccao_ms`, `primeira_frente_ms` (os 2 IRs), `carga_total_ms`, `ms_<modo>` (tempo em cada modo) |

## Problemas comuns

| Mensagem ou sintoma | Causa e solução |
|---|---|
| "Indisponível com o robô pronto para lutar" | O robô está esperando a largada ou lutando. Mande o IR 3, ou reinicie se ainda não lutou |
| "Guardando a luta, envie de novo em instantes" | O IR 3 acabou de chegar e a luta está indo para a flash (~1 s). Repita |
| "Nenhum round guardado" | Não há rounds na flash: já foram limpos, ou o IR 3 chegou durante a iniciação, ou o modo escolhido não é de luta |
| "Falha ao guardar a luta na flash" (BT) | A luta ficou só na RAM. Exporte antes de desligar: ela sai como `luta da RAM` |
| "O robô não enviou nenhum round" e nada recebido | Porta errada, robô desligado ou firmware antigo |
| A porta não abre ("Access denied") | Outro programa está com a porta, provavelmente um monitor serial. Feche-o |
| "Os arquivos foram salvos, mas a limpeza falhou" | Os rounds continuam no robô. Envie `l` pelo Serial ou colete de novo (vai gerar arquivos repetidos) |
| "Nada para limpar" | O `l` só apaga os rounds exportados desde que o robô ligou. Exporte (`d`) antes |
| "A transmissão parou no meio" | Mau contato no cabo. Repita: os rounds continuam na flash |

## Dicas para a análise

- Colete **todas** as lutas, inclusive as de treino. Com poucas dezenas de lutas, só dá para ver tendências grosseiras.
- Preencha o tipo e a observação **na hora**, enquanto você ainda lembra da luta.
- Ao mudar o firmware ou a calibração, anote na observação. As colunas de calibração ajudam a separar as lutas antes e depois da mudança.
- Se o formato da exportação mudar no firmware (`exportarLuta()` em `include/auto_mode.hpp`), o script também precisa ser atualizado.
