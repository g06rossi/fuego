"""
Coleta as lutas gravadas pelo Fuego e salva em CSV.

Depois do IR 3, o firmware guarda a luta na flash como um round numerado (ate 5, ver guardarRound()
em include/auto_mode.hpp). Ao receber 'd', exporta todos os rounds guardados, do mais antigo ao mais
novo. Este script pede a exportacao pela USB, separa cada round e grava:

  lutas/luta_AAAAMMDD_HHMMSS_rN_amostras.csv   serie temporal (uma linha a cada 10 ms)
  lutas/luta_AAAAMMDD_HHMMSS_rN_eventos.csv    eventos com o tempo exato
  lutas/luta_AAAAMMDD_HHMMSS_rN_bruto.txt      texto recebido, sem alteracao (backup)
  lutas/lutas.csv                              uma linha por luta: configuracao, resultado e resumo

N e o numero do round no robo (1 = a primeira luta desde a ultima limpeza). Depois de salvar todos,
o script envia 'l' e o robo apaga os rounds exportados (--manter nao apaga).

A porta e aberta sem acionar DTR/RTS: na placa do ESP32 eles reiniciam o chip. Os rounds ja estao
na flash, mas uma luta que nao conseguiu ser guardada so existe na RAM. As funcoes de leitura,
processamento e gravacao tambem sao usadas pela interface grafica (fst.py).

Uso:
  python tools/coletar_luta.py --porta COM5
  python tools/coletar_luta.py --porta COM5 --manter     (nao apaga os rounds do robo)
  python tools/coletar_luta.py --arquivo exportacao_bt.txt   (texto salvo do terminal BT)
  python tools/coletar_luta.py --listar                      (lista as portas seriais)

O pyserial vem junto, em tools/lib: nada para instalar alem do Python.
"""

import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

# Bibliotecas copiadas no repositorio (tools/lib) valem mais que as instaladas no PC
PASTA_LIB = Path(__file__).resolve().parent / "lib"
if str(PASTA_LIB) not in sys.path:
    sys.path.insert(0, str(PASTA_LIB))

PASTA_PADRAO = Path(__file__).resolve().parent.parent / "lutas"
TEMPO_INICIO_S = 5                            # Espera pelo #ROUNDS depois do 'd'
TEMPO_SILENCIO_S = 5                          # Sem dados por esse tempo no meio da exportacao = erro
TEMPO_LIMPEZA_S = 5                           # Espera pelo #LIMPOS depois do 'l'
LINHAS_POR_ROUND = 3100                       # Estimativa da barra de progresso (30 s de luta)

CHAVES_CONFIG = [
    "round", "adversario", "asa", "iniciacao", "direcao", "movimentacao", "furtivo", "finalizacao",
    "tempoFinalizacaoMs", "limiarLinha", "quedaLinha", "limiarLdr", "periodoSensoresUs", "janelaMs",
]

RESULTADOS = {"v": "vitoria", "d": "derrota", "e": "empate"}

# Quem sofreu o lance: "Fuego ..." = o Fuego, "adversario ..." = o adversario
TIPOS = [
    "Fuego saiu sozinho",
    "adversario saiu sozinho",
    "Fuego foi rampado",
    "adversario foi rampado",
    "Fuego acertado lateralmente",
    "adversario acertado lateralmente",
    "Fuego perdeu posicao de vantagem",
    "adversario perdeu posicao de vantagem",
    "decisao (tempo ou juizes)",
    "outro",
]

MODOS = ["ofensiva", "defensiva", "linha", "pulsada", "retorno", "desengate"]


#==========================================//Leitura//==========================================//

class ErroColeta(Exception):
    """Falha na coleta, com a mensagem para o usuario (usada pelo terminal e pela GUI)"""


def listar_portas():
    try:
        from serial.tools import list_ports
    except ImportError:
        raise ErroColeta(f"pyserial nao encontrado em {PASTA_LIB}")
    return [(p.device, p.description) for p in list_ports.comports()]


def abrir_serial(porta, baud):
    """Abre a porta sem acionar DTR/RTS (eles reiniciam o ESP) e descarta o que ja estava no buffer"""
    try:
        import serial
    except ImportError:
        raise ErroColeta(f"pyserial nao encontrado em {PASTA_LIB}")

    conexao = serial.Serial()
    conexao.port = porta
    conexao.baudrate = baud
    conexao.timeout = 0.5
    conexao.dtr = False                       # Antes do open: nao reinicia o ESP
    conexao.rts = False
    try:
        conexao.open()
    except serial.SerialException as erro:
        raise ErroColeta(f"Nao foi possivel abrir {porta} (outro programa esta usando?)\n{erro}")
    time.sleep(0.2)
    conexao.reset_input_buffer()
    return conexao


def ler_serial(porta, baud, progresso=None):
    """Pede os rounds ao robo e devolve uma lista com as linhas de cada luta, na ordem do robo (o
    round mais antigo primeiro). progresso(recebidas, total_estimado)"""
    conexao = abrir_serial(porta, baud)
    try:
        conexao.write(b"d\n")

        lutas = []
        linhas = None                         # Luta sendo recebida
        avisos = []
        esperados = None                      # Do #ROUNDS,N
        recebidas = 0
        total_luta = LINHAS_POR_ROUND         # Corrigido pelos cabecalhos #AMOSTRAS e #EVENTOS
        inicio = time.time()
        ultimo_dado = time.time()

        while True:
            bruta = conexao.readline()
            agora = time.time()
            if not bruta:
                comecou = esperados is not None or linhas is not None
                if not comecou and agora - inicio > TEMPO_INICIO_S:
                    msg = "\n  ".join(avisos) if avisos else "(nada)"
                    raise ErroColeta(f"O robo nao enviou nenhum round. Mensagens recebidas:\n  {msg}")
                if comecou and agora - ultimo_dado > TEMPO_SILENCIO_S:
                    raise ErroColeta(f"A transmissao parou no meio ({len(lutas)} round(s) completo(s), "
                                     f"{recebidas} linhas recebidas)")
                continue

            ultimo_dado = agora
            linha = bruta.decode("utf-8", errors="replace").strip()
            if linha.startswith("#ROUNDS,"):
                esperados = int(linha.split(",")[1])
                continue
            if linha.startswith("#ROUNDS_FIM"):
                return lutas
            if linha == "#LUTA_INICIO":
                linhas = []
                total_luta = LINHAS_POR_ROUND
            if linhas is None:
                if linha:
                    avisos.append(linha)
                continue

            linhas.append(linha)
            recebidas += 1
            if linha.startswith("#AMOSTRAS,"):
                total_luta = len(linhas) + int(linha.split(",")[1]) + 30
            elif linha.startswith("#EVENTOS,"):
                total_luta = len(linhas) + int(linha.split(",")[1]) + 2
            if progresso and (recebidas % 50 == 0 or linha == "#LUTA_FIM"):
                faltam = max(0, (esperados or 1) - len(lutas) - 1)
                total = recebidas - len(linhas) + max(total_luta, len(linhas)) + faltam * LINHAS_POR_ROUND
                progresso(recebidas, total)
            if linha == "#LUTA_FIM":
                lutas.append(linhas)
                linhas = None
                if esperados is None:         # Firmware antigo: uma luta so, sem #ROUNDS
                    return lutas
    finally:
        conexao.close()


def limpar_robo(porta, baud):
    """Pede ao robo para apagar os rounds da ultima exportacao. Devolve quantos foram apagados"""
    conexao = abrir_serial(porta, baud)
    try:
        conexao.write(b"l\n")
        avisos = []
        inicio = time.time()
        while time.time() - inicio < TEMPO_LIMPEZA_S:
            linha = conexao.readline().decode("utf-8", errors="replace").strip()
            if linha.startswith("#LIMPOS,"):
                return int(linha.split(",")[1])
            if linha:
                avisos.append(linha)
        msg = "\n  ".join(avisos) if avisos else "(nada)"
        raise ErroColeta(f"O robo nao confirmou a limpeza. Mensagens recebidas:\n  {msg}")
    finally:
        conexao.close()


def separar_lutas(linhas):
    """Blocos #LUTA_INICIO ... #LUTA_FIM completos, na ordem"""
    lutas, atual = [], None
    for linha in linhas:
        if linha == "#LUTA_INICIO":
            atual = []
        if atual is not None:
            atual.append(linha)
            if linha == "#LUTA_FIM":
                lutas.append(atual)
                atual = None
    return lutas


def ler_arquivo(caminho):
    """Lutas da ultima exportacao do arquivo (todos os rounds dela). Sem #ROUNDS, so a ultima luta"""
    texto = Path(caminho).read_text(encoding="utf-8", errors="replace")
    linhas = [l.strip() for l in texto.splitlines()]
    rounds = [i for i, l in enumerate(linhas) if l.startswith("#ROUNDS,")]
    if rounds:
        lutas = separar_lutas(linhas[rounds[-1]:])
        esperados = int(linhas[rounds[-1]].split(",")[1])
        if len(lutas) < esperados:
            raise ErroColeta(f"A exportacao do arquivo esta incompleta ({len(lutas)} de {esperados} rounds)")
        return lutas
    lutas = separar_lutas(linhas)
    if not lutas:
        raise ErroColeta("O arquivo nao tem um bloco #LUTA_INICIO ... #LUTA_FIM completo")
    return lutas[-1:]


#===========================================//Parse//===========================================//

def separar_blocos(linhas):
    config = {}
    amostras = []
    eventos = []
    bloco = None

    for linha in linhas:
        if not linha or linha in ("#LUTA_INICIO", "#LUTA_FIM"):
            continue
        if linha.startswith("#AMOSTRAS"):
            bloco = amostras
            continue
        if linha.startswith("#EVENTOS"):
            bloco = eventos
            continue
        if linha.startswith("#"):
            chave, _, valor = linha[1:].partition(",")
            config[chave] = valor
            continue
        if bloco is not None:
            bloco.append(linha.split(","))

    if not amostras or not eventos:
        raise ErroColeta("Exportacao incompleta: faltam as amostras ou os eventos")

    return config, amostras, eventos


def tabela(bloco):
    cabecalho, *dados = bloco
    return [dict(zip(cabecalho, valores)) for valores in dados if len(valores) == len(cabecalho)]


#==========================================//Resumo//===========================================//

def resumir(config, amostras, eventos):
    janela = int(config.get("janelaMs", 10))
    resumo = {"duracao_ms": int(amostras[-1]["t_ms"]) if amostras else 0}

    fins = [e["evento"] for e in eventos if e["evento"] in ("parada", "fim")]
    resumo["fim_gravacao"] = "ir3" if fins and fins[-1] == "parada" else ("30s" if fins else "?")

    resumo["retornos"] = sum(1 for e in eventos if e["evento"] == "modo" and e["valor"] == "retorno")
    resumo["linhas_vistas"] = sum(1 for e in eventos if e["evento"] in ("linhaE", "linhaD") and e["valor"] == "1")
    resumo["ataques_ldr"] = sum(1 for e in eventos if e["evento"] == "ldrAtaque" and e["valor"] == "1")

    def primeiro(condicao):
        for a in amostras:
            if condicao(a):
                return int(a["t_ms"])
        return ""

    resumo["primeira_deteccao_ms"] = primeiro(lambda a: "1" in (a["jsE"], a["irE"], a["irD"], a["jsD"]))
    resumo["primeira_frente_ms"] = primeiro(lambda a: a["irE"] == "1" and a["irD"] == "1")

    resumo["carga_total_ms"] = sum(janela for a in amostras if a["cargaTotal"] == "1")
    for modo in MODOS:
        resumo[f"ms_{modo}"] = sum(janela for a in amostras if a["modo"] == modo)
    return resumo


def processar(linhas):
    """Separa os blocos e calcula o resumo da luta"""
    config, amostras_cruas, eventos_crus = separar_blocos(linhas)
    amostras, eventos = tabela(amostras_cruas), tabela(eventos_crus)
    return {
        "linhas": linhas, "config": config,
        "amostras_cruas": amostras_cruas, "eventos_crus": eventos_crus,
        "amostras": amostras, "eventos": eventos,
        "resumo": resumir(config, amostras, eventos),
    }


def nome_round(luta):
    """Rotulo para o usuario: "round 3", ou a luta que so estava na RAM (nao foi guardada na flash)"""
    r = luta["config"].get("round", "")
    if r.isdigit():
        return f"round {r}"
    return "luta da RAM (nao guardada)" if r == "ram" else "luta"


def descrever_luta(luta):
    """Uma linha para reconhecer o round: estrategia e duracao"""
    config, resumo = luta["config"], luta["resumo"]
    return (f"{config.get('adversario', '?')}, {config.get('iniciacao', '?')} "
            f"({config.get('direcao', '?')}), {config.get('movimentacao', '?')}, "
            f"{resumo['duracao_ms'] / 1000:.1f} s (fim por {resumo['fim_gravacao']})")


#==========================================//Gravacao//=========================================//

def gravar_csv(caminho, linhas):
    with open(caminho, "w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerows(linhas)


def gravar_resumo(pasta, linha):
    caminho = pasta / "lutas.csv"
    colunas = list(linha.keys())
    if caminho.exists():
        with open(caminho, newline="", encoding="utf-8") as f:
            leitor = csv.DictReader(f)
            antigas = list(leitor.fieldnames or [])
            linhas = list(leitor)
        if antigas != colunas:
            # Coluna nova (ex.: round): reescreve com a uniao. As lutas antigas ficam vazias nela
            colunas += [c for c in antigas if c not in colunas]
            with open(caminho, "w", newline="", encoding="utf-8") as f:
                escritor = csv.DictWriter(f, fieldnames=colunas)
                escritor.writeheader()
                escritor.writerows(linhas)
    else:
        with open(caminho, "w", newline="", encoding="utf-8") as f:
            csv.DictWriter(f, fieldnames=colunas).writeheader()
    with open(caminho, "a", newline="", encoding="utf-8") as f:
        csv.DictWriter(f, fieldnames=colunas).writerow(linha)
    return caminho


def salvar_luta(pasta, luta, resultado, tipo, observacao, data=None):
    """Grava os arquivos da luta e acrescenta a linha em lutas.csv. Devolve o prefixo dos arquivos.
    data: a mesma para todos os rounds de uma coleta (eles se diferenciam pelo _rN)"""
    pasta = Path(pasta)
    pasta.mkdir(parents=True, exist_ok=True)
    data = data or datetime.now()
    r = luta["config"].get("round", "")
    base = f"luta_{data:%Y%m%d_%H%M%S}" + (f"_r{r}" if r else "")
    nome = base
    sufixo = 2
    while (pasta / f"{nome}_amostras.csv").exists():   # Mesmo nome de uma luta ja salva
        nome = f"{base}_{sufixo}"
        sufixo += 1

    (pasta / f"{nome}_bruto.txt").write_text("\n".join(luta["linhas"]) + "\n", encoding="utf-8")
    gravar_csv(pasta / f"{nome}_amostras.csv", luta["amostras_cruas"])
    gravar_csv(pasta / f"{nome}_eventos.csv", luta["eventos_crus"])

    linha = {"luta": nome, "data": f"{data:%Y-%m-%d %H:%M:%S}"}
    linha.update({chave: luta["config"].get(chave, "") for chave in CHAVES_CONFIG})
    linha.update({"resultado": resultado, "tipo": tipo, "observacao": observacao})
    linha.update(luta["resumo"])
    gravar_resumo(pasta, linha)
    return nome


#==========================================//Terminal//=========================================//

def perguntar_resultado():
    while True:
        r = input("Resultado [v]itoria, [d]errota, [e]mpate (ENTER = sem resultado): ").strip().lower()
        if r == "" or r in RESULTADOS:
            break
    resultado = RESULTADOS.get(r, "")

    tipo = ""
    if resultado:
        for i, nome in enumerate(TIPOS):
            print(f"  {i} - {nome}")
        while True:
            t = input("Tipo (ENTER = sem tipo): ").strip()
            if t == "":
                break
            if t.isdigit() and int(t) < len(TIPOS):
                tipo = TIPOS[int(t)]
                break

    observacao = input("Observacao (opcional): ").strip()
    return resultado, tipo, observacao


def main():
    parser = argparse.ArgumentParser(description="Coleta as lutas gravadas pelo Fuego e salva em CSV")
    parser.add_argument("--porta", help="Porta serial do robo (ex.: COM5)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--arquivo", help="Le a exportacao de um arquivo de texto em vez da serial")
    parser.add_argument("--pasta", type=Path, default=PASTA_PADRAO, help="Pasta de saida")
    parser.add_argument("--sem-resultado", action="store_true", help="Nao pergunta o resultado")
    parser.add_argument("--manter", action="store_true", help="Nao apaga os rounds do robo depois de salvar")
    parser.add_argument("--listar", action="store_true", help="Lista as portas seriais e sai")
    args = parser.parse_args()

    try:
        if args.listar:
            for device, descricao in listar_portas():
                print(f"{device}  {descricao}")
            return

        if args.arquivo:
            blocos = ler_arquivo(args.arquivo)
        elif args.porta:
            print(f"Pedido enviado em {args.porta}. Recebendo...")
            blocos = ler_serial(args.porta, args.baud,
                                lambda n, total: print(f"  {n}/{total} linhas", end="\r"))
            print()
        else:
            parser.error("informe --porta ou --arquivo")

        lutas = [processar(linhas) for linhas in blocos]
    except ErroColeta as erro:
        sys.exit(str(erro))

    # Na ordem do robo: o round mais antigo primeiro
    print(f"\n{len(lutas)} luta(s) recebida(s), na ordem em que foram lutadas:")
    for luta in lutas:
        print(f"  {nome_round(luta):10s} {descrever_luta(luta)}")

    data = datetime.now()
    nomes = []
    for i, luta in enumerate(lutas, 1):
        config, resumo = luta["config"], luta["resumo"]
        print(f"\n=== {i} de {len(lutas)}: {nome_round(luta).upper()} ===")
        print(f"  {len(luta['amostras'])} amostras, {len(luta['eventos'])} eventos")
        for chave in ("adversario", "iniciacao", "direcao", "movimentacao", "finalizacao"):
            print(f"  {chave:13s}: {config.get(chave, '?')}")
        print(f"  duracao      : {resumo['duracao_ms']} ms (fim por {resumo['fim_gravacao']})")
        print(f"  retornos     : {resumo['retornos']}")
        print(f"  1a deteccao  : {resumo['primeira_deteccao_ms'] or '-'} ms\n")

        resultado, tipo, observacao = ("", "", "") if args.sem_resultado else perguntar_resultado()
        nomes.append(salvar_luta(args.pasta, luta, resultado, tipo, observacao, data))

    print(f"\nSalvo em {args.pasta}:")
    for nome in nomes:
        print(f"  {nome}_amostras.csv, {nome}_eventos.csv, {nome}_bruto.txt")
    print("  resumo acrescentado em lutas.csv")

    # So depois de tudo salvo: uma falha antes deixa os rounds no robo
    if args.porta and not args.manter:
        try:
            print(f"Rounds apagados do robo: {limpar_robo(args.porta, args.baud)}")
        except ErroColeta as erro:
            print(f"Os arquivos foram salvos, mas a limpeza falhou: {erro}")


if __name__ == "__main__":
    main()
