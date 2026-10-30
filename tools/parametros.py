"""
Parametros de ajuste do Fuego para a aba Parametros do FST: le os valores direto do codigo e troca
so o numero da inicializacao ao aplicar.

Fontes:
  - o bloco PARAMETROS DE AJUSTE de include/fighting.hpp (velocidades e tempos das FSMs de luta),
    agrupado pelos comentarios de secao ("// Busca Linha", "// Retorno ...")
  - os itens configuraveis pela NVS (PIN_CONFIG e THRESHOLD_CONFIG de include/defines.hpp) que
    ficam em outros lugares, com a faixa aceita pelo menu de Configuracao

Um parametro novo no bloco aparece sozinho, desde que siga o formato "tipo nome = numero;".
"""

import re
from pathlib import Path

import config_nvs

RAIZ = Path(__file__).resolve().parent.parent
ARQUIVO_LUTA = RAIZ / "include" / "fighting.hpp"
CABECALHO_BLOCO = "PARAMETROS DE AJUSTE"
GRUPO_NVS = "Configuraveis pela NVS (outros arquivos)"

RE_PARAMETRO = re.compile(r"^\s*(const\s+)?(unsigned\s+long|unsigned\s+int|unsigned|long|int|uint8_t|uint16_t|"
                          r"int16_t|int32_t)\s+(\w+)\s*=\s*(-?\d+)\s*;\s*(?://\s*(.*))?$")
RE_COMENTARIO = re.compile(r"^\s*//\s*(.+)$")
RE_FIM_BLOCO = re.compile(r"^\s*(enum|struct|void|bool|//=+)\b")
RE_PINO = re.compile(r'\{\s*"([^"]*)"\s*,\s*&(\w+)\s*,\s*"(\w+)"\s*\}')
RE_LIMIAR = re.compile(r'\{\s*"([^"]*)"\s*,\s*&(\w+)\s*,\s*"(\w+)"\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}')

VEL_MAXIMA = 255                              # PWM dos motores
PINO_MAXIMO = 39                              # Maior GPIO do ESP32


def itens_nvs():
    """{variavel: {nome, chave, minimo, maximo}} das tabelas de configuracao do defines.hpp"""
    texto = (RAIZ / "include" / "defines.hpp").read_text(encoding="utf-8")
    itens = {}
    for nome, variavel, chave in RE_PINO.findall(texto):
        itens[variavel] = {"nome": nome, "chave": chave, "minimo": 0, "maximo": PINO_MAXIMO}
    for nome, variavel, chave, minimo, maximo in RE_LIMIAR.findall(texto):
        itens[variavel] = {"nome": nome, "chave": chave, "minimo": int(minimo), "maximo": int(maximo)}
    return itens


def faixa(parametro):
    """(minimo, maximo) aceitos. None = sem limite"""
    if parametro["nvs"]:
        return parametro["nvs"]["minimo"], parametro["nvs"]["maximo"]
    if parametro["variavel"].lower().startswith("vel"):
        return 0, VEL_MAXIMA
    if parametro["tipo"].startswith("unsigned") or parametro["tipo"].startswith("uint"):
        return 0, None
    return None, None


def validar(parametro, texto):
    """Devolve o valor inteiro ou levanta ValueError com a mensagem"""
    texto = texto.strip()
    if not re.fullmatch(r"-?\d+", texto):
        raise ValueError(f"{parametro['variavel']}: use um numero inteiro")
    valor = int(texto)
    minimo, maximo = faixa(parametro)
    if minimo is not None and valor < minimo:
        raise ValueError(f"{parametro['variavel']}: minimo {minimo}")
    if maximo is not None and valor > maximo:
        raise ValueError(f"{parametro['variavel']}: maximo {maximo}")
    return valor


def ler_parametros():
    """Lista de parametros: {grupo, variavel, tipo, constante, valor, comentario, arquivo, linha, nvs}"""
    nvs = itens_nvs()
    parametros = []

    linhas = ARQUIVO_LUTA.read_text(encoding="utf-8").splitlines()
    inicio = next((i for i, l in enumerate(linhas) if CABECALHO_BLOCO in l), None)
    if inicio is not None:
        grupo, anterior_vazia, comecou = "Geral", False, False
        for numero in range(inicio + 2, len(linhas)):      # Pula o cabecalho e a linha de "=" de baixo
            texto = linhas[numero]
            if comecou and RE_FIM_BLOCO.match(texto):
                break
            achado = RE_PARAMETRO.match(texto)
            if achado:
                comecou = True
                constante, tipo, variavel, valor, comentario = achado.groups()
                parametros.append({
                    "grupo": grupo, "variavel": variavel, "tipo": re.sub(r"\s+", " ", tipo),
                    "constante": bool(constante), "valor": int(valor), "comentario": (comentario or "").strip(),
                    "arquivo": ARQUIVO_LUTA, "linha": numero + 1, "nvs": nvs.get(variavel)})
            else:
                comentario = RE_COMENTARIO.match(texto)
                # Comentario sozinho depois de uma linha vazia = titulo da secao
                if comentario and anterior_vazia:
                    grupo = comentario.group(1).strip()
            anterior_vazia = not texto.strip()

    # Itens da NVS fora do bloco (pinos, servo, limiar do LDR, periodo dos sensores...)
    no_bloco = {p["variavel"] for p in parametros}
    for chave, local in config_nvs.mapear_codigo(RAIZ).items():
        variavel = local["variavel"]
        if variavel in no_bloco or not local["arquivo"]:
            continue
        linha = Path(local["arquivo"]).read_text(encoding="utf-8").splitlines()[local["linha"] - 1]
        achado = RE_PARAMETRO.match(linha)
        parametros.append({
            "grupo": GRUPO_NVS, "variavel": variavel,
            "tipo": re.sub(r"\s+", " ", achado.group(2)) if achado else "int", "constante": False,
            "valor": local["valor"], "comentario": nvs.get(variavel, {}).get("nome", ""),
            "arquivo": Path(local["arquivo"]), "linha": local["linha"], "nvs": nvs.get(variavel)})
    return parametros


def aplicar(alteracoes):
    """alteracoes: [(parametro, valor_novo)]. Troca so o numero no codigo e devolve as descricoes"""
    return config_nvs.aplicar_no_codigo([(p["variavel"], p["arquivo"], p["linha"], p["valor"], novo)
                                         for p, novo in alteracoes])
