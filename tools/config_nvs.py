"""
Configuracao do Fuego (pinos e parametros da NVS) para a aba NVS da interface.

- ler_config_serial(): pede a tabela ao robo pelo Serial ('c', ver imprimirConfig() em
  include/auto_mode.hpp). Cada item traz o valor em uso, o padrao com que o firmware gravado foi
  compilado e se o valor veio da NVS.
- mapear_codigo(): acha, no codigo do repositorio, a variavel de cada chave (tabelas PIN_CONFIG e
  THRESHOLD_CONFIG de include/defines.hpp) e a linha em que ela e inicializada.
- aplicar_no_codigo(): troca o numero da inicializacao pelo valor escolhido. So mexe no numero e
  mantem o alinhamento do comentario de fim de linha.
"""

import re
import time
from pathlib import Path

from coletar_luta import ErroColeta, abrir_serial

RAIZ = Path(__file__).resolve().parent.parent
TEMPO_CONFIG_S = 4                            # Espera pelo #CONFIG_FIM depois do 'c'

RE_ENTRADA = re.compile(r'\{\s*"[^"]*"\s*,\s*&(\w+)\s*,\s*"(\w+)"')
TIPOS_C = r"(?:int|uint8_t|uint16_t|int16_t|int32_t|long)"


def ler_config_serial(porta, baud):
    conexao = abrir_serial(porta, baud)
    try:
        conexao.write(b"c\n")
        linhas, dentro, inicio, avisos = [], False, time.time(), []
        while time.time() - inicio < TEMPO_CONFIG_S:
            bruta = conexao.readline()
            if not bruta:
                continue
            linha = bruta.decode("utf-8", errors="replace").strip()
            if linha == "#CONFIG_INICIO":
                dentro, linhas = True, []
                continue
            if linha == "#CONFIG_FIM" and dentro:
                return interpretar_config(linhas)
            if dentro:
                linhas.append(linha)
            elif linha:
                avisos.append(linha)
    finally:
        conexao.close()
    msg = "\n  ".join(avisos[-5:]) if avisos else "(nada)"
    raise ErroColeta(f"O robo nao enviou a configuracao (firmware antigo ou em luta?). Recebido:\n  {msg}")


def interpretar_config(linhas):
    if not linhas:
        raise ErroColeta("Configuracao vazia")
    cabecalho = linhas[0].split(",")
    itens = []
    for linha in linhas[1:]:
        valores = linha.split(",")
        if len(valores) != len(cabecalho):
            continue
        item = dict(zip(cabecalho, valores))
        for campo in ("id", "valor", "padrao", "nvs"):
            item[campo] = int(item[campo])
        for campo in ("minimo", "maximo"):
            item[campo] = int(item[campo]) if item[campo] != "" else None
        itens.append(item)
    return itens


def _arquivos_codigo(raiz):
    return sorted((raiz / "include").glob("*.hpp")) + sorted((raiz / "src").glob("*.cpp"))


def _regex_definicao(variavel):
    # Ex.: "int LINHA_QUEDA                = 300;   // comentario"
    return re.compile(rf"^(\s*(?:static\s+|volatile\s+)*{TIPOS_C}\s+{re.escape(variavel)}\s*=\s*)(-?\d+)(;)(\s*)(.*)$")


def mapear_codigo(raiz=RAIZ):
    """Devolve {chave: {variavel, arquivo, linha, valor}} a partir do codigo do repositorio"""
    defines = (raiz / "include" / "defines.hpp").read_text(encoding="utf-8")
    chaves = {chave: variavel for variavel, chave in RE_ENTRADA.findall(defines)}

    mapa = {}
    arquivos = {arquivo: arquivo.read_text(encoding="utf-8").splitlines() for arquivo in _arquivos_codigo(raiz)}
    for chave, variavel in chaves.items():
        regex = _regex_definicao(variavel)
        for arquivo, linhas in arquivos.items():
            for numero, texto in enumerate(linhas, 1):
                encontrado = regex.match(texto)
                if encontrado:
                    mapa[chave] = {"variavel": variavel, "arquivo": arquivo, "linha": numero,
                                   "valor": int(encontrado.group(2))}
                    break
            if chave in mapa:
                break
        if chave not in mapa:
            mapa[chave] = {"variavel": variavel, "arquivo": None, "linha": None, "valor": None}
    return mapa


def aplicar_no_codigo(alteracoes, raiz=RAIZ):
    """alteracoes: [(variavel, arquivo, linha, valor_antigo, valor_novo)]. Devolve as descricoes"""
    feitas = []
    por_arquivo = {}
    for alteracao in alteracoes:
        por_arquivo.setdefault(alteracao[1], []).append(alteracao)

    for arquivo, lista in por_arquivo.items():
        with open(arquivo, encoding="utf-8", newline="") as f:   # newline="": preserva o CRLF
            texto = f.read()
        final = "\r\n" if "\r\n" in texto else "\n"
        linhas = texto.split(final)
        for variavel, _, numero, antigo, novo in lista:
            encontrado = _regex_definicao(variavel).match(linhas[numero - 1])
            if not encontrado or int(encontrado.group(2)) != antigo:
                raise ErroColeta(f"{variavel} mudou em {Path(arquivo).name}:{numero}. Recarregue o codigo")
            inicio, _, ponto, espacos, resto = encontrado.groups()
            # Mantem a coluna do comentario: os espacos compensam a diferenca de digitos
            largura = len(encontrado.group(2)) + len(espacos)
            novo_texto = str(novo)
            espacos = " " * max(1, largura - len(novo_texto)) if resto else ""
            linhas[numero - 1] = f"{inicio}{novo_texto}{ponto}{espacos}{resto}"
            feitas.append(f"{variavel}: {antigo} -> {novo} ({Path(arquivo).name}:{numero})")
        Path(arquivo).write_text(final.join(linhas), encoding="utf-8", newline="")
    return feitas
