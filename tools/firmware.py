"""
Firmware e versao do codigo do Fuego: PlatformIO (compilar, gravar, limpar a flash) e git (commit e
push), com a saida linha a linha para a interface mostrar no log.

Todas as funcoes recebem log(texto) e, quando ha progresso, progresso(porcentagem, texto). Os
comandos rodam na raiz do repositorio.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SEM_JANELA = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0

# Barra de progresso da gravacao: compilacao de 0 a GRAVACAO_COMPILA_PCT, gravacao dali ate 100
GRAVACAO_COMPILA_PCT = 70
ARQUIVOS_ESTIMADOS = 60                       # Para a curva da compilacao (o total nao e conhecido antes)


class ErroFirmware(Exception):
    """Falha de um comando, com a mensagem para o usuario"""


#==========================================//Processos//========================================//

class Execucao:
    """Um comando por vez, com cancelamento"""

    def __init__(self):
        self.processo = None
        self.cancelado = False

    def rodar(self, comando, log, linha=None, ambiente=None):
        """Roda o comando, manda cada linha para linha(texto) (ou log) e devolve o codigo de saida"""
        log("$ " + " ".join(str(c) for c in comando))
        env = dict(os.environ, PYTHONIOENCODING="utf-8", PYTHONUNBUFFERED="1", **(ambiente or {}))
        try:
            self.processo = subprocess.Popen(
                [str(c) for c in comando], cwd=RAIZ, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL, text=True, encoding="utf-8", errors="replace", env=env,
                creationflags=SEM_JANELA)
        except OSError as erro:
            raise ErroFirmware(f"Nao foi possivel executar {comando[0]}: {erro}")
        for texto in self.processo.stdout:   # O modo texto troca o \r do esptool por \n
            texto = texto.rstrip()
            if texto:
                (linha or log)(texto)
        codigo = self.processo.wait()
        self.processo = None
        if self.cancelado:
            raise ErroFirmware("Cancelado pelo usuario")
        return codigo

    def cancelar(self):
        self.cancelado = True
        if self.processo:
            self.processo.kill()


#=========================================//PlatformIO//========================================//

def achar_platformio():
    """O platformio do PlatformIO (penv) primeiro: o 'pio' do PATH pode ser outro programa Python"""
    penv = Path.home() / ".platformio" / "penv" / ("Scripts" if sys.platform == "win32" else "bin")
    for candidato in (penv / "platformio.exe", penv / "platformio", shutil.which("platformio")):
        if candidato and Path(candidato).exists():
            return str(candidato)
    raise ErroFirmware("PlatformIO nao encontrado. Instale a extensao PlatformIO no VS Code")


class ProgressoPio:
    """Traduz a saida do PlatformIO em porcentagem e texto da etapa. A compilacao ocupa a barra de 0 a
    compila_pct, e a gravacao dali ate 100"""

    def __init__(self, progresso, compila_pct=100):
        self.progresso, self.compila_pct = progresso, compila_pct
        self.compilados = 0

    def ler(self, linha):
        c = self.compila_pct
        if linha.startswith("Compiling "):
            self.compilados += 1
            # Curva que se aproxima do fim da compilacao sem passar dele
            self.progresso(c * 0.9 * (1 - 0.5 ** (3 * self.compilados / ARQUIVOS_ESTIMADOS)),
                           f"Compilando ({self.compilados} arquivos)")
        elif linha.startswith("Linking "):
            self.progresso(c * 0.92, "Ligando o firmware")
        elif linha.startswith("Building ") and linha.endswith(".bin"):
            self.progresso(c * 0.96, "Gerando o .bin")
        elif linha.startswith("Flash:"):
            self.progresso(c, "Compilado. " + linha)
        elif linha.startswith("Connecting"):
            self.progresso(c + 0.02 * (100 - c), "Conectando ao ESP32 (segure BOOT se nao conectar)")
        elif linha.startswith("Erasing flash"):
            self.progresso(30, "Apagando a flash (leva alguns segundos)")
        elif "Chip erase completed" in linha:
            self.progresso(100, "Flash apagada")
        elif linha.startswith("Hard resetting"):
            self.progresso(100, "Pronto. Reiniciando o robo")
        else:
            gravando = re.search(r"Writing at 0x[0-9a-fA-F]+\.*\s*\((\d+)\s*%\)", linha)
            if gravando:
                pct = int(gravando.group(1))
                # Bootloader, particoes e firmware sao gravados em sequencia: a barra segue o atual
                self.progresso(c + (100 - c) * (0.05 + 0.93 * pct / 100), f"Gravando no robo: {pct}%")


def platformio(execucao, argumentos, log, progresso, compila_pct=100):
    leitor = ProgressoPio(progresso, compila_pct)

    def linha(texto):
        log(texto)
        leitor.ler(texto)

    return execucao.rodar([achar_platformio()] + argumentos, log, linha)


def compilar(execucao, log, progresso):
    progresso(0, "Compilando")
    if platformio(execucao, ["run"], log, progresso) != 0:
        raise ErroFirmware("A compilacao falhou. Veja os erros no log")
    progresso(100, "Compilado")


def gravar(execucao, porta, log, progresso):
    progresso(0, "Compilando e gravando")
    if platformio(execucao, ["run", "-t", "upload", "--upload-port", porta], log, progresso, GRAVACAO_COMPILA_PCT) != 0:
        raise ErroFirmware("A gravacao falhou. Veja o log: se parou em 'Connecting', segure o botao BOOT "
                           "do ESP32 durante a conexao, e confira se nenhum monitor serial esta com a porta")
    progresso(100, "Firmware gravado")


def limpar_flash(execucao, porta, log, progresso):
    progresso(0, "Apagando a flash")
    if platformio(execucao, ["run", "-t", "erase", "--upload-port", porta], log, progresso) != 0:
        raise ErroFirmware("A limpeza da flash falhou. Veja o log")
    progresso(100, "Flash apagada")


#============================================//Git//============================================//

def git(execucao, argumentos, log=None):
    """Roda o git e devolve (codigo, saida). Sem log, nao mostra o comando"""
    saida = []
    codigo = execucao.rodar(["git"] + argumentos, log or (lambda _: None),
                            lambda texto: (saida.append(texto), log and log(texto)),
                            {"GIT_TERMINAL_PROMPT": "0"})
    return codigo, "\n".join(saida)


def estado_git(execucao):
    """Branch atual, branches locais e remotas, remoto e arquivos alterados"""
    _, atual = git(execucao, ["rev-parse", "--abbrev-ref", "HEAD"])
    _, locais = git(execucao, ["branch", "--format=%(refname:short)"])
    _, remotas = git(execucao, ["branch", "-r", "--format=%(refname:short)"])
    _, remoto = git(execucao, ["remote", "get-url", "origin"])
    _, status = git(execucao, ["status", "--short"])
    locais = [b for b in locais.splitlines() if b]
    remotas = [b.split("/", 1)[1] for b in remotas.splitlines() if "/" in b and not b.endswith("/HEAD")]
    return {
        "atual": atual.strip(),
        "locais": locais,
        "remotas": remotas,
        "branches": sorted(set(locais) | set(remotas)),
        "remoto": remoto.strip() if remoto and "fatal" not in remoto else "",
        "alterados": [l for l in status.splitlines() if l.strip()],
    }


def commitar(execucao, branch, mensagem, enviar, log, progresso):
    """Troca para a branch (cria se nao existir), faz o commit de tudo e envia para o GitHub"""
    branch = branch.strip()
    if not branch or re.search(r"[\s~^:?*\[\\]|\.\.", branch):
        raise ErroFirmware(f"Nome de branch invalido: '{branch}'")
    if not mensagem.strip():
        raise ErroFirmware("Escreva a mensagem do commit")

    estado = estado_git(execucao)
    passo = lambda pct, texto: (progresso(pct, texto), log(f">> {texto}"))

    if branch != estado["atual"]:
        if branch in estado["locais"]:
            passo(10, f"Trocando para a branch {branch} (as alteracoes vao junto)")
            argumentos = ["checkout", branch]
        elif branch in estado["remotas"]:
            passo(10, f"Criando a branch {branch} a partir da origin/{branch}")
            argumentos = ["checkout", "-b", branch, "--track", f"origin/{branch}"]
        else:
            passo(10, f"Criando a branch nova {branch} a partir de {estado['atual']}")
            argumentos = ["checkout", "-b", branch]
        if git(execucao, argumentos, log)[0] != 0:
            raise ErroFirmware(f"Nao foi possivel trocar para {branch}. Se o git avisou que arquivos seriam "
                               "sobrescritos, a branch tem versoes diferentes deles: resolva pelo VS Code")

    passo(30, "Adicionando os arquivos alterados")
    if git(execucao, ["add", "-A"], log)[0] != 0:
        raise ErroFirmware("O git add falhou")
    if git(execucao, ["diff", "--cached", "--quiet"])[0] == 0:
        raise ErroFirmware("Nada para commitar: o codigo nao mudou desde o ultimo commit")

    passo(50, "Criando o commit")
    if git(execucao, ["commit", "-m", mensagem.strip()], log)[0] != 0:
        raise ErroFirmware("O commit falhou (o nome e o e-mail do git estao configurados?)")
    _, hash_curto = git(execucao, ["rev-parse", "--short", "HEAD"])

    if enviar:
        passo(70, f"Enviando para o GitHub (origin/{branch})")
        if git(execucao, ["push", "-u", "origin", branch], log)[0] != 0:
            raise ErroFirmware(f"O commit {hash_curto.strip()} foi criado, mas o envio falhou (internet ou "
                               "login do GitHub). Tente enviar de novo pelo VS Code")
    progresso(100, f"Commit {hash_curto.strip()} na branch {branch}" + (" enviado" if enviar else ""))
    return hash_curto.strip()
