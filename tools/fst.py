"""
Fuego Sumo Tool (FST): interface das ferramentas do Fuego (aberta pelo fst.bat).

Abas:
  Fuego        imagem do robo (imagens/fuego.txt ou a foto)
  Guia         mostra o tools/README.md
  Coleta       coleta os rounds pela USB ou por um texto salvo do BT, mostra o resumo de cada um,
               salva com o resultado e limpa os rounds do robo
  Firmware     MAC mestre do controle PS4, compilacao, gravacao e limpeza da flash do robo e commit
               no GitHub, com log
  Parametros   edita no codigo os parametros das FSMs de luta (fighting.hpp) e os itens da NVS
  Dados        tabelas dos CSVs da pasta de lutas (lutas.csv e as series de cada luta)
  NVS          configuracao do robo (pinos e parametros) e aplicacao dos valores no codigo
  Configuracoes modo noturno, abertura em degrade e animacao de abertura

A coleta usa o coletar_luta.py, o controle o ps4_mac.py e o firmware o firmware.py. So usa a
biblioteca padrao (tkinter) e as copias de tools/lib (pyserial e libusb0.dll). Compilar e gravar
usam o PlatformIO, e o commit o git.
"""

import csv
import json
import os
import queue
import re
import threading
import time
import tkinter as tk
import tkinter.font as tkfont
from datetime import datetime
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import coletar_luta as coleta
import firmware
import parametros
import ps4_mac
import config_nvs

PASTA_TOOLS = Path(__file__).resolve().parent
README = PASTA_TOOLS / "README.md"
BAUD_PADRAO = 115200
MAIN_CPP = PASTA_TOOLS.parent / "src" / "main.cpp"
JANELA_LARGURA = 1150
JANELA_ALTURA = 720
JANELA_SUBIR = 20                             # Compensa a barra de titulo e a de tarefas

# Abertura: a arte do Fuego surge, fica na tela e some antes da janela principal
ABERTURA_DURACAO_MS = 2000                    # Tempo total: surgir, exibicao, sumir e tela vazia
ABERTURA_VAZIO_MS = 500                       # Tela vazia entre a arte sumir e a ferramenta aparecer
ABERTURA_ESPACO = 24                          # Espaco entre o robo e o nome "Fuego" na abertura
ABERTURA_PASSO = 0.08                         # Variacao da opacidade por passo
ABERTURA_INTERVALO_MS = 15                    # Intervalo entre passos (~200 ms para surgir ou sumir)
ABERTURA_FONTE = 8
ABERTURA_FUNDO = "#0d0d0d"
ABERTURA_COR = "#e01b1b"                      # Vermelho da arte
ABERTURA_DEGRADE = ((255, 210, 60), (220, 40, 20))   # Amarelo no topo ate vermelho embaixo
FONTES_ARTE = ("Cascadia Mono", "Cascadia Code", "Consolas", "Courier New")

# Aba Fuego: ASCII art do robo desenhada como imagem, no maior tamanho que cabe na aba
ARTE_ROBO = PASTA_TOOLS / "imagens" / "fuego.txt"
ARTE_ROBO_MARGEM = 16
# Cor de cada caractere: '@' sao as placas (claras), '.' os contornos (vermelhos), '+' e '*' o fundo
ARTE_ROBO_CORES = {
    "noturno": {"@": "#f2f2f2", "%": "#cfcfcf", "#": "#a8a8a8", ".": "#e01b1b", ":": "#8a8a8a",
                "-": "#707070", "=": "#5c5c5c", "*": "#3f3f3f", "+": "#333333"},
    "claro":   {"@": "#1e1e1e", "%": "#3c3c3c", "#": "#5e5e5e", ".": "#d01515", ":": "#8a8a8a",
                "-": "#a0a0a0", "=": "#b4b4b4", "*": "#d2d2d2", "+": "#dedede"},
}

# Preferencias da ferramenta (aba Configuracoes), guardadas fora do git
PREFERENCIAS = PASTA_TOOLS / "preferencias.json"
PREFERENCIAS_PADRAO = {"modo_noturno": True, "abertura_degrade": True, "abertura_animada": True,
                       "imagem_aba": "ascii_robo", "abertura_estilo": "ascii_fuego"}

# Representacoes do robo. As fotos sao geradas por gerar_imagens.py a partir de imagens/fuego_original.png
PASTA_IMAGENS = PASTA_TOOLS / "imagens"
REPRESENTACOES = {
    "ascii_fuego": "ASCII art do Fuego (main.cpp)",
    "ascii_robo": "ASCII art do robo (fuego.txt)",
    "foto": "Foto do robo",
    "tratada": "Foto tratada (fundo removido, tons de fogo)",
}
OPCOES_LISTA = {
    "imagem_aba": ["ascii_robo", "foto", "tratada"],
    "abertura_estilo": ["ascii_fuego", "ascii_robo", "foto", "tratada"],
}

NOME_FERRAMENTA = "Fuego Sumo Tool (FST)"
LOG_FIRMWARE_LINHAS = 5000                    # Acima disso, as linhas mais antigas do log saem
AVISO_LIMPAR_FLASH = ("Limpar a flash apaga tudo no ESP32: o programa, a NVS (pinos, parametros, "
                      "iniciacoes salvas e historico) e os rounds guardados.\n\nColetou os rounds? Continuar?")

TEMAS = {
    "noturno": {"fundo": "#1e1e1e", "campo": "#2a2a2a", "texto": "#e6e6e6", "sutil": "#9a9a9a",
                "borda": "#3c3c3c", "selecao": "#264f78", "realce": "#333333", "codigo": "#2d2d2d",
                "citacao": "#3a3220", "aviso": "#ffb454", "diferente": "#4a4120", "alerta": "#5a2626",
                "erro": "#ff6b6b", "ok": "#6fcf73"},
    "claro":   {"fundo": "#f3f3f3", "campo": "#ffffff", "texto": "#1e1e1e", "sutil": "#666666",
                "borda": "#c8c8c8", "selecao": "#cce4f7", "realce": "#e5e5e5", "codigo": "#eeeeee",
                "citacao": "#fff4d6", "aviso": "#a05000", "diferente": "#fff1b8", "alerta": "#ffd6d6",
                "erro": "#c01818", "ok": "#1d7a2b"},
}


def carregar_preferencias():
    preferencias = dict(PREFERENCIAS_PADRAO)
    try:
        salvas = json.loads(PREFERENCIAS.read_text(encoding="utf-8"))
        for chave, valor in salvas.items():
            padrao = PREFERENCIAS_PADRAO.get(chave)
            if isinstance(padrao, bool):
                preferencias[chave] = bool(valor)
            elif chave in OPCOES_LISTA and valor in OPCOES_LISTA[chave]:
                preferencias[chave] = valor
    except (OSError, ValueError, AttributeError):
        pass
    return preferencias


def salvar_preferencias(preferencias):
    try:
        PREFERENCIAS.write_text(json.dumps(preferencias, indent=2), encoding="utf-8")
    except OSError:
        pass


def aplicar_tema_ttk(raiz, nome):
    """Cores dos widgets ttk. O tema clam aceita todas as cores, nos dois modos"""
    c = TEMAS[nome]
    estilo = ttk.Style(raiz)
    estilo.theme_use("clam")
    estilo.configure(".", background=c["fundo"], foreground=c["texto"], fieldbackground=c["campo"],
                     bordercolor=c["borda"], lightcolor=c["fundo"], darkcolor=c["fundo"],
                     troughcolor=c["campo"], selectbackground=c["selecao"], selectforeground=c["texto"],
                     insertcolor=c["texto"], arrowcolor=c["texto"])
    estilo.map(".", background=[("disabled", c["fundo"]), ("active", c["realce"])],
               foreground=[("disabled", c["sutil"])])
    estilo.configure("TNotebook", background=c["fundo"], bordercolor=c["borda"])
    estilo.configure("TNotebook.Tab", background=c["fundo"], padding=(12, 4))
    estilo.map("TNotebook.Tab", background=[("selected", c["campo"]), ("active", c["realce"])])
    estilo.configure("TButton", background=c["realce"], padding=(10, 3))
    estilo.map("TButton", background=[("disabled", c["fundo"]), ("pressed", c["selecao"]), ("active", c["borda"])])
    estilo.map("TCombobox", fieldbackground=[("readonly", c["campo"])], foreground=[("readonly", c["texto"])],
               selectbackground=[("readonly", c["campo"])], selectforeground=[("readonly", c["texto"])])
    estilo.configure("Treeview", background=c["campo"], fieldbackground=c["campo"], foreground=c["texto"])
    estilo.map("Treeview", background=[("selected", c["selecao"])], foreground=[("selected", c["texto"])])
    estilo.configure("Treeview.Heading", background=c["realce"], foreground=c["texto"])
    estilo.map("Treeview.Heading", background=[("active", c["borda"])])
    estilo.configure("TLabelframe.Label", foreground=c["texto"])
    estilo.configure("Horizontal.TProgressbar", background="#e01b1b", troughcolor=c["campo"])
    estilo.configure("Aviso.TLabel", foreground=c["aviso"])
    estilo.configure("Dica.TLabel", foreground=c["sutil"])
    # Lista aberta pelos Combobox (widget tk, nao ttk)
    raiz.option_add("*TCombobox*Listbox.background", c["campo"])
    raiz.option_add("*TCombobox*Listbox.foreground", c["texto"])
    raiz.option_add("*TCombobox*Listbox.selectBackground", c["selecao"])
    raiz.option_add("*TCombobox*Listbox.selectForeground", c["texto"])
    raiz.configure(background=c["fundo"])


class App:
    def __init__(self, raiz, preferencias):
        self.raiz = raiz
        self.preferencias = preferencias
        self.raiz.title(NOME_FERRAMENTA)
        # Centralizada na tela (sem descontar a barra de tarefas, que fica embaixo)
        x = max(0, (raiz.winfo_screenwidth() - JANELA_LARGURA) // 2)
        y = max(0, (raiz.winfo_screenheight() - JANELA_ALTURA) // 2 - JANELA_SUBIR)
        self.raiz.geometry(f"{JANELA_LARGURA}x{JANELA_ALTURA}+{x}+{y}")
        self.raiz.minsize(800, 500)

        self.fila = queue.Queue()             # Mensagens da thread de coleta para a interface
        self.lutas = []                       # Rounds recebidos, ainda nao salvos
        self.origem = None                    # (porta, baud) da coleta pela USB
        self.indice = None                    # Round mostrado no formulario
        self.ocupado = False
        self.pasta = tk.StringVar(value=str(coleta.PASTA_PADRAO))

        abas = ttk.Notebook(raiz)
        abas.pack(fill="both", expand=True, padx=6, pady=(6, 0))
        self.abas = abas

        self.aba_fuego = ttk.Frame(abas)
        self.aba_guia = ttk.Frame(abas)
        self.aba_ferramentas = ttk.Frame(abas)
        self.aba_firmware = ttk.Frame(abas)
        self.aba_parametros = ttk.Frame(abas)
        self.aba_dados = ttk.Frame(abas)
        self.aba_nvs = ttk.Frame(abas)
        self.aba_config = ttk.Frame(abas)
        abas.add(self.aba_fuego, text="Fuego")
        abas.add(self.aba_guia, text="Guia")
        abas.add(self.aba_ferramentas, text="Coleta")
        abas.add(self.aba_firmware, text="Firmware")
        abas.add(self.aba_parametros, text="Parametros")
        abas.add(self.aba_dados, text="Dados")
        abas.add(self.aba_nvs, text="NVS")
        abas.add(self.aba_config, text="Configuracoes")

        self.montar_fuego()
        self.montar_guia()
        self.montar_ferramentas()
        self.montar_firmware()
        self.montar_parametros()
        self.montar_dados()
        self.montar_nvs()
        self.montar_config()
        self.montar_progresso()
        self.aplicar_tema()

        abas.select(self.aba_ferramentas)
        self.atualizar_portas()
        self.atualizar_arquivos()
        self.recarregar_codigo()
        self.raiz.after(100, self.processar_fila)
        self.raiz.after(300, self.atualizar_git)
        self.raiz.protocol("WM_DELETE_WINDOW", self.fechar)

    def fechar(self):
        if self.execucao and not messagebox.askyesno(NOME_FERRAMENTA, "Ha uma operacao em andamento. Cancelar e sair?"):
            return
        if self.pendentes and not messagebox.askyesno(
                NOME_FERRAMENTA, f"{len(self.pendentes)} parametro(s) alterado(s) e nao aplicado(s) ao codigo. Sair assim mesmo?"):
            return
        if self.execucao:
            self.execucao.cancelar()
        self.raiz.destroy()

#=======================================//Configuracoes//=======================================//

    def montar_config(self):
        aba = ttk.Frame(self.aba_config, padding=16)
        aba.pack(fill="both", expand=True)
        self.opcoes = {}
        textos = [
            ("modo_noturno", "Modo noturno", "Cores escuras na janela inteira (padrao)."),
            ("abertura_animada", "Animacao de abertura", "Mostra a arte do Fuego ao abrir a ferramenta."),
            ("abertura_degrade", "Abertura em degrade", "Arte em degrade do amarelo ao vermelho (padrao). Desligado: so vermelho."),
        ]
        for linha, (chave, titulo, descricao) in enumerate(textos):
            variavel = tk.BooleanVar(value=self.preferencias[chave])
            self.opcoes[chave] = variavel
            ttk.Checkbutton(aba, text=titulo, variable=variavel,
                            command=lambda c=chave: self.mudar_opcao(c)).grid(row=linha, column=0, sticky="w", pady=4)
            ttk.Label(aba, text=descricao, style="Dica.TLabel").grid(row=linha, column=1, sticky="w", padx=16)

        listas = [
            ("imagem_aba", "Imagem da aba Fuego", "Representacao do robo mostrada na aba Fuego."),
            ("abertura_estilo", "Imagem da abertura",
             "Com o robo (ASCII ou foto), o nome \"Fuego\" da ASCII art do main.cpp aparece embaixo."),
        ]
        self.listas = {}
        for deslocamento, (chave, titulo, descricao) in enumerate(listas):
            linha = len(textos) + deslocamento
            ttk.Label(aba, text=titulo).grid(row=linha, column=0, sticky="w", pady=(12 if deslocamento == 0 else 4, 4))
            nomes = [REPRESENTACOES[opcao] for opcao in OPCOES_LISTA[chave]]
            lista = ttk.Combobox(aba, values=nomes, state="readonly", width=44)
            lista.current(OPCOES_LISTA[chave].index(self.preferencias[chave]))
            lista.bind("<<ComboboxSelected>>", lambda _, c=chave: self.mudar_lista(c))
            lista.grid(row=linha, column=1, sticky="w", padx=16, pady=(12 if deslocamento == 0 else 4, 4))
            ttk.Label(aba, text=descricao, style="Dica.TLabel").grid(row=linha, column=2, sticky="w")
            self.listas[chave] = lista

        base = len(textos) + len(listas)
        ttk.Button(aba, text="Ver a abertura agora", command=self.ver_abertura).grid(
            row=base, column=0, sticky="w", pady=(16, 0))
        ttk.Label(aba, text=f"As opcoes sao salvas em {PREFERENCIAS.name}, na pasta tools.",
                  style="Dica.TLabel").grid(row=base + 1, column=0, columnspan=3, sticky="w", pady=(16, 0))

    def mudar_opcao(self, chave):
        self.preferencias[chave] = self.opcoes[chave].get()
        salvar_preferencias(self.preferencias)
        if chave == "modo_noturno":
            self.aplicar_tema()

    def mudar_lista(self, chave):
        self.preferencias[chave] = OPCOES_LISTA[chave][self.listas[chave].current()]
        salvar_preferencias(self.preferencias)
        if chave == "imagem_aba":
            self.desenhar_fuego()

    def ver_abertura(self):
        Abertura(self.raiz, self.preferencias["abertura_degrade"], self.preferencias["abertura_estilo"])

    def aplicar_tema(self):
        nome = "noturno" if self.preferencias["modo_noturno"] else "claro"
        aplicar_tema_ttk(self.raiz, nome)
        c = TEMAS[nome]
        for texto in (self.guia, self.resumo, self.observacao, self.log, self.log_fw, self.mensagem_commit):
            texto.config(background=c["campo"], foreground=c["texto"], insertbackground=c["texto"],
                         selectbackground=c["selecao"], selectforeground=c["texto"])
        self.guia.tag_config("codigo", background=c["codigo"])
        self.guia.tag_config("bloco", background=c["codigo"])
        self.guia.tag_config("citacao", background=c["citacao"])
        self.tabela_nvs.tag_configure("diferente", background=c["diferente"])
        self.tabela_nvs.tag_configure("aviso", background=c["alerta"])
        self.tabela_param.tag_configure("diferente", background=c["diferente"])
        self.tabela_param.tag_configure("grupo", foreground=c["aviso"])
        self.log_fw.tag_config("saida", foreground=c["sutil"])
        self.log_fw.tag_config("etapa", foreground=c["texto"])
        self.log_fw.tag_config("erro", foreground=c["erro"])
        self.log_fw.tag_config("ok", foreground=c["ok"])
        self.arte_robo.config(background=c["fundo"], foreground=c["sutil"])
        self.desenhar_fuego()

#=========================================//Progresso//=========================================//

    def montar_progresso(self):
        barra = ttk.Frame(self.raiz)
        barra.pack(fill="x", padx=6, pady=6)
        self.progresso = ttk.Progressbar(barra, mode="determinate", maximum=100)
        self.progresso.pack(side="left", fill="x", expand=True)
        self.status = ttk.Label(barra, text="Pronto", width=60, anchor="w")
        self.status.pack(side="left", padx=(8, 0))

    def definir_status(self, texto, porcentagem=None):
        self.status.config(text=texto)
        if porcentagem is not None:
            self.progresso["value"] = porcentagem

#===========================================//Fuego//===========================================//

    def montar_fuego(self):
        self.linhas_robo = linhas_ascii_robo()
        self.robo_base = None                 # Imagem antes da escala
        self.robo_zoom_max = 1                # Fotos nao ampliam (perdem nitidez); a ASCII sim
        self.robo_escala = None               # (zoom, subsample) da imagem exibida
        self.robo_ajuste = None
        self.arte_robo = tk.Label(self.aba_fuego, borderwidth=0, highlightthickness=0)
        self.arte_robo.place(relx=0.5, rely=0.5, anchor="center")
        self.aba_fuego.bind("<Configure>", lambda _: self.agendar_ajuste_fuego())

    def desenhar_fuego(self):
        escolha = self.preferencias["imagem_aba"]
        tema = "noturno" if self.preferencias["modo_noturno"] else "claro"
        if escolha == "ascii_robo":
            self.robo_base = imagem_ascii_robo(self.linhas_robo, tema)
            self.robo_zoom_max = 8
            faltando = ARTE_ROBO
        else:
            self.robo_base = imagem_foto(escolha, "aba")
            self.robo_zoom_max = 1
            faltando = PASTA_IMAGENS / f"fuego_{escolha}_aba.png (rode tools/gerar_imagens.py)"
        self.arte_robo.config(image="", text="" if self.robo_base else f"Nao encontrei {faltando}")
        self.robo_escala = None
        self.ajustar_fuego()

    def agendar_ajuste_fuego(self):
        # A janela manda varios <Configure> seguidos ao redimensionar: so ajusta no ultimo
        if self.robo_ajuste:
            self.raiz.after_cancel(self.robo_ajuste)
        self.robo_ajuste = self.raiz.after(100, self.ajustar_fuego)

    def ajustar_fuego(self):
        self.robo_ajuste = None
        if not self.robo_base:
            return
        largura = self.aba_fuego.winfo_width() - 2 * ARTE_ROBO_MARGEM
        altura = self.aba_fuego.winfo_height() - 2 * ARTE_ROBO_MARGEM
        if largura <= 1 or altura <= 1:
            return
        maximo = min(largura / self.robo_base.width(), altura / self.robo_base.height(), self.robo_zoom_max)
        escala = melhor_escala(maximo)
        if escala == self.robo_escala:
            return
        self.robo_escala = escala
        self.robo_imagem = escalar(self.robo_base, *escala)
        self.arte_robo.config(image=self.robo_imagem)

#===========================================//Guia//============================================//

    def montar_guia(self):
        texto = tk.Text(self.aba_guia, wrap="word", padx=14, pady=10, font=("Segoe UI", 10),
                        borderwidth=0)
        self.guia = texto
        rolagem = ttk.Scrollbar(self.aba_guia, command=texto.yview)
        texto.config(yscrollcommand=rolagem.set)
        rolagem.pack(side="right", fill="y")
        texto.pack(fill="both", expand=True)

        texto.tag_config("h1", font=("Segoe UI", 18, "bold"), spacing1=6, spacing3=6)
        texto.tag_config("h2", font=("Segoe UI", 14, "bold"), spacing1=10, spacing3=4)
        texto.tag_config("h3", font=("Segoe UI", 11, "bold"), spacing1=8, spacing3=2)
        texto.tag_config("negrito", font=("Segoe UI", 10, "bold"))
        texto.tag_config("codigo", font=("Consolas", 10))
        texto.tag_config("bloco", font=("Consolas", 10), lmargin1=20, lmargin2=20)
        texto.tag_config("tabela", font=("Consolas", 9))
        texto.tag_config("citacao", lmargin1=20, lmargin2=20)

        try:
            linhas = README.read_text(encoding="utf-8").splitlines()
        except OSError:
            linhas = [f"Nao encontrei {README}"]

        em_bloco = False
        for linha in linhas:
            if linha.strip().startswith("```"):
                em_bloco = not em_bloco
                continue
            if em_bloco:
                texto.insert("end", linha + "\n", "bloco")
            elif linha.startswith("### "):
                texto.insert("end", linha[4:] + "\n", "h3")
            elif linha.startswith("## "):
                texto.insert("end", linha[3:] + "\n", "h2")
            elif linha.startswith("# "):
                texto.insert("end", linha[2:] + "\n", "h1")
            elif linha.lstrip().startswith("|"):
                if not re.fullmatch(r"\s*\|[\s|:-]+\|\s*", linha):   # Pula a linha |---|---|
                    texto.insert("end", re.sub(r"\*\*|`", "", linha) + "\n", "tabela")
            elif linha.startswith("> "):
                self.inserir_inline(texto, linha[2:], ("citacao",))
            else:
                self.inserir_inline(texto, linha)
        texto.config(state="disabled")

    def inserir_inline(self, texto, linha, extra=()):
        # **negrito** e `codigo` dentro da linha
        for parte in re.split(r"(\*\*[^*]+\*\*|`[^`]+`)", linha):
            if parte.startswith("**") and parte.endswith("**") and len(parte) > 4:
                texto.insert("end", parte[2:-2], ("negrito",) + extra)
            elif parte.startswith("`") and parte.endswith("`") and len(parte) > 2:
                texto.insert("end", parte[1:-1], ("codigo",) + extra)
            else:
                texto.insert("end", parte, extra)
        texto.insert("end", "\n", extra)

#==========================================//Coleta//===========================================//

    def montar_ferramentas(self):
        aba = self.aba_ferramentas
        aba.columnconfigure(0, weight=1)
        aba.columnconfigure(1, weight=1)
        aba.rowconfigure(1, weight=1)

        # Conexao e origem
        conexao = ttk.LabelFrame(aba, text="1. Origem da luta", padding=8)
        conexao.grid(row=0, column=0, columnspan=2, sticky="ew", padx=6, pady=6)

        ttk.Label(conexao, text="Porta:").grid(row=0, column=0, sticky="w")
        self.porta = ttk.Combobox(conexao, width=40, state="readonly")
        self.porta.grid(row=0, column=1, sticky="w", padx=4)
        ttk.Button(conexao, text="Atualizar portas", command=self.atualizar_portas).grid(row=0, column=2, padx=4)
        ttk.Label(conexao, text="Baud:").grid(row=0, column=3, sticky="e", padx=(12, 0))
        self.baud = ttk.Entry(conexao, width=10)
        self.baud.insert(0, str(BAUD_PADRAO))
        self.baud.grid(row=0, column=4, sticky="w", padx=4)

        ttk.Label(conexao, text="Pasta:").grid(row=1, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(conexao, textvariable=self.pasta, width=70).grid(row=1, column=1, columnspan=3,
                                                                   sticky="ew", padx=4, pady=(6, 0))
        ttk.Button(conexao, text="Escolher...", command=self.escolher_pasta).grid(row=1, column=4, padx=4, pady=(6, 0))

        botoes = ttk.Frame(conexao)
        botoes.grid(row=2, column=0, columnspan=5, sticky="w", pady=(10, 0))
        self.botao_usb = ttk.Button(botoes, text="Coletar pela USB", command=self.coletar_usb)
        self.botao_usb.pack(side="left")
        self.botao_arquivo = ttk.Button(botoes, text="Abrir exportacao do BT (.txt)...",
                                        command=self.abrir_arquivo)
        self.botao_arquivo.pack(side="left", padx=8)
        ttk.Label(botoes, text="Os rounds ficam na flash do robo (ate 5) ate serem limpos.",
                  style="Aviso.TLabel").pack(side="left", padx=8)

        # Rounds recebidos
        quadro_resumo = ttk.LabelFrame(aba, text="2. Rounds recebidos (na ordem das lutas)", padding=8)
        quadro_resumo.grid(row=1, column=0, sticky="nsew", padx=6, pady=6)
        self.seletor_round = ttk.Combobox(quadro_resumo, state="readonly", width=60)
        self.seletor_round.pack(fill="x", pady=(0, 6))
        self.seletor_round.bind("<<ComboboxSelected>>", lambda _: self.mostrar_round(self.seletor_round.current()))
        self.resumo = tk.Text(quadro_resumo, height=16, width=50, font=("Consolas", 10),
                              state="disabled", borderwidth=0)
        self.resumo.pack(fill="both", expand=True)

        # Resultado
        quadro_resultado = ttk.LabelFrame(aba, text="3. Resultado", padding=8)
        quadro_resultado.grid(row=1, column=1, sticky="nsew", padx=6, pady=6)
        quadro_resultado.columnconfigure(1, weight=1)

        self.resultado = tk.StringVar(value="")
        ttk.Label(quadro_resultado, text="Resultado:").grid(row=0, column=0, sticky="nw")
        radios = ttk.Frame(quadro_resultado)
        radios.grid(row=0, column=1, sticky="w")
        for texto, valor in (("Vitoria", "vitoria"), ("Derrota", "derrota"), ("Empate", "empate"),
                             ("Sem resultado", "")):
            ttk.Radiobutton(radios, text=texto, value=valor, variable=self.resultado).pack(side="left", padx=(0, 10))

        ttk.Label(quadro_resultado, text="Tipo:").grid(row=1, column=0, sticky="w", pady=(10, 0))
        self.tipo = ttk.Combobox(quadro_resultado, values=["(sem tipo)"] + coleta.TIPOS, state="readonly", width=42)
        self.tipo.current(0)
        self.tipo.grid(row=1, column=1, sticky="w", pady=(10, 0))

        ttk.Label(quadro_resultado, text="Observacao:").grid(row=2, column=0, sticky="nw", pady=(10, 0))
        self.observacao = tk.Text(quadro_resultado, height=5, width=42, font=("Segoe UI", 10))
        self.observacao.grid(row=2, column=1, sticky="ew", pady=(10, 0))

        self.limpar_depois = tk.BooleanVar(value=True)
        self.check_limpar = ttk.Checkbutton(quadro_resultado, text="Limpar os rounds do robo depois de salvar",
                                            variable=self.limpar_depois)
        self.check_limpar.grid(row=3, column=1, sticky="w", pady=(10, 0))

        acoes = ttk.Frame(quadro_resultado)
        acoes.grid(row=4, column=1, sticky="w", pady=(14, 0))
        self.botao_salvar = ttk.Button(acoes, text="Salvar todos os rounds", command=self.salvar, state="disabled")
        self.botao_salvar.pack(side="left")
        self.botao_descartar = ttk.Button(acoes, text="Descartar", command=self.descartar, state="disabled")
        self.botao_descartar.pack(side="left", padx=8)
        ttk.Label(quadro_resultado, text="O resultado e de cada round: troque o round no quadro 2 e preencha.",
                  style="Aviso.TLabel").grid(row=5, column=0, columnspan=2, sticky="w", pady=(10, 0))

        # Registro
        quadro_log = ttk.LabelFrame(aba, text="Registro", padding=4)
        quadro_log.grid(row=2, column=0, columnspan=2, sticky="ew", padx=6, pady=(0, 6))
        self.log = tk.Text(quadro_log, height=6, font=("Consolas", 9), state="disabled", borderwidth=0)
        self.log.pack(fill="x")

    def registrar(self, texto):
        self.log.config(state="normal")
        self.log.insert("end", texto + "\n")
        self.log.see("end")
        self.log.config(state="disabled")

    def atualizar_portas(self):
        try:
            portas = coleta.listar_portas()
        except coleta.ErroColeta as erro:
            self.registrar(str(erro))
            return
        valores = [f"{device} - {descricao}" for device, descricao in portas]
        self.porta["values"] = valores
        self.porta_fw["values"] = valores
        if valores and not self.porta.get():
            self.porta.current(0)
        if valores and not self.porta_fw.get():
            self.porta_fw.current(0)
        self.registrar(f"{len(valores)} porta(s) encontrada(s)")

    def escolher_pasta(self):
        pasta = filedialog.askdirectory(initialdir=self.pasta.get())
        if pasta:
            self.pasta.set(pasta)
            self.atualizar_arquivos()

    def definir_ocupado(self, ocupado):
        self.ocupado = ocupado
        estado = "disabled" if ocupado else "normal"
        self.botao_usb.config(state=estado)
        self.botao_arquivo.config(state=estado)
        self.botao_ler_nvs.config(state=estado)
        for botao in self.botoes_firmware:
            botao.config(state=estado)

    def coletar_usb(self):
        if not self.porta.get():
            messagebox.showwarning("Coleta", "Escolha a porta do robo")
            return
        try:
            baud = int(self.baud.get())
        except ValueError:
            messagebox.showwarning("Coleta", "Baud invalido")
            return
        if self.lutas and not messagebox.askyesno("Coleta", "Os rounds recebidos ainda nao foram salvos. Descartar?"):
            return

        porta = self.porta.get().split(" - ")[0]
        self.definir_ocupado(True)
        self.definir_status(f"Pedindo os rounds em {porta}...", 0)
        self.registrar(f"Pedido enviado em {porta}")

        def trabalho():
            try:
                blocos = coleta.ler_serial(porta, baud,
                                           lambda n, total: self.fila.put(("progresso", n, total)))
                self.fila.put(("lutas", [coleta.processar(linhas) for linhas in blocos], (porta, baud)))
            except coleta.ErroColeta as erro:
                self.fila.put(("erro", str(erro)))
            except Exception as erro:          # Nao deixa a thread morrer sem avisar a interface
                self.fila.put(("erro", f"Erro inesperado: {erro}"))

        threading.Thread(target=trabalho, daemon=True).start()

    def abrir_arquivo(self):
        caminho = filedialog.askopenfilename(title="Exportacao salva do terminal BT",
                                             filetypes=[("Texto", "*.txt *.log"), ("Todos", "*.*")])
        if not caminho:
            return
        if self.lutas and not messagebox.askyesno("Coleta", "Os rounds recebidos ainda nao foram salvos. Descartar?"):
            return
        try:
            lutas = [coleta.processar(linhas) for linhas in coleta.ler_arquivo(caminho)]
        except coleta.ErroColeta as erro:
            self.mostrar_erro(str(erro))
            return
        self.registrar(f"Lido: {caminho}")
        self.receber_lutas(lutas, None)

    def processar_fila(self):
        try:
            while True:
                mensagem = self.fila.get_nowait()
                if mensagem[0] == "progresso":
                    _, n, total = mensagem
                    self.definir_status(f"Recebendo: {n}/{total} linhas", 100 * n / total)
                elif mensagem[0] == "lutas":
                    self.definir_ocupado(False)
                    self.receber_lutas(mensagem[1], mensagem[2])
                elif mensagem[0] == "limpos":
                    self.definir_ocupado(False)
                    self.registrar(f"Rounds apagados do robo: {mensagem[1]}")
                    self.definir_status(f"Rounds salvos e {mensagem[1]} apagado(s) do robo", 100)
                elif mensagem[0] == "erro":
                    self.definir_ocupado(False)
                    self.mostrar_erro(mensagem[1])
                elif mensagem[0] == "fw_log":
                    self.registrar_fw(mensagem[1], mensagem[2])
                elif mensagem[0] == "fw_progresso":
                    _, porcentagem, texto = mensagem
                    self.etapa_fw.config(text=f"{texto} ({porcentagem:.0f}%)")
                    self.definir_status(texto, porcentagem)
                elif mensagem[0] == "fw_fim":
                    self.terminar_firmware(*mensagem[1:])
                elif mensagem[0] == "config":
                    self.definir_ocupado(False)
                    self.receber_config(mensagem[1])
        except queue.Empty:
            pass
        self.raiz.after(100, self.processar_fila)

    def mostrar_erro(self, texto):
        self.definir_status("Erro na coleta", 0)
        self.registrar(texto)
        messagebox.showerror("Coleta", texto)

    def receber_lutas(self, lutas, origem):
        """lutas na ordem do robo (o round mais antigo primeiro). origem = (porta, baud) da USB, que
        recebe o pedido de limpeza depois de salvar, ou None (arquivo do BT)"""
        self.lutas = lutas
        self.origem = origem
        self.indice = None
        for luta in lutas:
            luta["formulario"] = ("", 0, "")  # Resultado, indice do tipo e observacao de cada round
        self.seletor_round["values"] = [f"{i} de {len(lutas)}: {coleta.nome_round(l)} - {coleta.descrever_luta(l)}"
                                        for i, l in enumerate(lutas, 1)]
        self.check_limpar.config(state="normal" if origem else "disabled")
        self.botao_salvar.config(state="normal")
        self.botao_descartar.config(state="normal")
        self.mostrar_round(0)

        nomes = ", ".join(coleta.nome_round(l) for l in lutas)
        self.definir_status(f"{len(lutas)} round(s) recebido(s). Preencha o resultado de cada um e salve", 100)
        self.registrar(f"Recebido(s): {nomes}. Preencha o resultado de cada round e clique em Salvar")

    def guardar_formulario(self):
        if self.indice is None:
            return
        observacao = " ".join(self.observacao.get("1.0", "end").split())   # Uma linha so no CSV
        self.lutas[self.indice]["formulario"] = (self.resultado.get(), max(0, self.tipo.current()), observacao)

    def mostrar_round(self, indice):
        self.guardar_formulario()
        self.indice = indice
        self.seletor_round.current(indice)
        luta = self.lutas[indice]
        config, resumo = luta["config"], luta["resumo"]
        linhas = [f"{coleta.nome_round(luta).upper()}: {len(luta['amostras'])} amostras, "
                  f"{len(luta['eventos'])} eventos", ""]
        linhas += [f"{chave:20s} {config.get(chave, '?')}" for chave in coleta.CHAVES_CONFIG]
        linhas.append("")
        linhas += [f"{chave:20s} {valor if valor != '' else '-'}" for chave, valor in resumo.items()]

        self.resumo.config(state="normal")
        self.resumo.delete("1.0", "end")
        self.resumo.insert("1.0", "\n".join(linhas))
        self.resumo.config(state="disabled")

        resultado, tipo, observacao = luta["formulario"]
        self.resultado.set(resultado)
        self.tipo.current(tipo)
        self.observacao.delete("1.0", "end")
        self.observacao.insert("1.0", observacao)

    def salvar(self):
        if not self.lutas:
            return
        self.guardar_formulario()
        sem_resultado = [coleta.nome_round(l) for l in self.lutas if not l["formulario"][0]]
        if sem_resultado and not messagebox.askyesno(
                "Coleta", "Sem resultado: " + ", ".join(sem_resultado) + ".\nSalvar assim mesmo?"):
            return

        data = datetime.now()
        nomes = []
        try:
            for luta in self.lutas:
                resultado, tipo, observacao = luta["formulario"]
                tipo = coleta.TIPOS[tipo - 1] if tipo > 0 else ""
                nomes.append(coleta.salvar_luta(self.pasta.get(), luta, resultado, tipo, observacao, data))
        except OSError as erro:
            self.mostrar_erro(f"Nao foi possivel salvar: {erro}. O robo nao foi limpo")
            return
        for nome in nomes:
            self.registrar(f"Salvo: {nome} em {self.pasta.get()}")
        self.definir_status(f"{len(nomes)} round(s) salvo(s)", 100)
        origem = self.origem
        self.limpar_luta()
        self.atualizar_arquivos()

        # So depois de tudo salvo: o robo apaga apenas os rounds da ultima exportacao
        if origem and self.limpar_depois.get():
            porta, baud = origem
            self.definir_ocupado(True)
            self.registrar(f"Pedindo a limpeza dos rounds em {porta}")

            def trabalho():
                try:
                    self.fila.put(("limpos", coleta.limpar_robo(porta, baud)))
                except coleta.ErroColeta as erro:
                    self.fila.put(("erro", f"Os arquivos foram salvos, mas a limpeza falhou: {erro}"))
                except Exception as erro:
                    self.fila.put(("erro", f"Os arquivos foram salvos, mas a limpeza falhou: {erro}"))

            threading.Thread(target=trabalho, daemon=True).start()

    def descartar(self):
        if messagebox.askyesno("Coleta", "Descartar os rounds recebidos sem salvar? Eles continuam no robo"):
            self.limpar_luta()
            self.definir_status("Rounds descartados", 0)

    def limpar_luta(self):
        self.lutas = []
        self.origem = None
        self.indice = None
        self.seletor_round.set("")
        self.seletor_round["values"] = []
        self.botao_salvar.config(state="disabled")
        self.botao_descartar.config(state="disabled")

#==========================================//Firmware//=========================================//

    def montar_firmware(self):
        aba = self.aba_firmware
        aba.columnconfigure(0, weight=1)
        aba.columnconfigure(1, weight=1)
        aba.rowconfigure(2, weight=1)
        self.execucao = None                  # firmware.Execucao em andamento (para cancelar)
        self.botoes_firmware = []

        # Controle PS4
        ps4 = ttk.LabelFrame(aba, text="1. Controle PS4 (pelo cabo USB)", padding=8)
        ps4.grid(row=0, column=0, sticky="nsew", padx=6, pady=6)
        ps4.columnconfigure(1, weight=1)
        self.ps4_modelo = ttk.Label(ps4, text="-")
        self.ps4_proprio = ttk.Label(ps4, text="-")
        self.ps4_mestre = ttk.Label(ps4, text="-")
        for linha, (rotulo, valor) in enumerate((("Controle:", self.ps4_modelo), ("MAC do controle:", self.ps4_proprio),
                                                 ("Mestre atual:", self.ps4_mestre))):
            ttk.Label(ps4, text=rotulo).grid(row=linha, column=0, sticky="w")
            valor.grid(row=linha, column=1, sticky="w", padx=4)
        ttk.Label(ps4, text="Mestre novo:").grid(row=3, column=0, sticky="w", pady=(8, 0))
        self.ps4_novo = ttk.Entry(ps4, width=20)
        self.ps4_novo.grid(row=3, column=1, sticky="w", padx=4, pady=(8, 0))
        self.ps4_codigo = ttk.Label(ps4, text="", style="Dica.TLabel")
        self.ps4_codigo.grid(row=4, column=0, columnspan=2, sticky="w", pady=(2, 0))
        botoes = ttk.Frame(ps4)
        botoes.grid(row=5, column=0, columnspan=2, sticky="w", pady=(10, 0))
        for texto, comando in (("Ler controle", self.ler_controle), ("Atualizar mestre", self.atualizar_mestre)):
            botao = ttk.Button(botoes, text=texto, command=comando)
            botao.pack(side="left", padx=(0, 8))
            self.botoes_firmware.append(botao)

        # Robo
        robo = ttk.LabelFrame(aba, text="2. Codigo no robo (pela USB)", padding=8)
        robo.grid(row=0, column=1, sticky="nsew", padx=6, pady=6)
        ttk.Label(robo, text="Porta:").grid(row=0, column=0, sticky="w")
        self.porta_fw = ttk.Combobox(robo, width=34, state="readonly")
        self.porta_fw.grid(row=0, column=1, sticky="w", padx=4)
        botao = ttk.Button(robo, text="Atualizar portas", command=self.atualizar_portas)
        botao.grid(row=0, column=2, padx=4)
        self.botoes_firmware.append(botao)
        self.limpar_antes = tk.BooleanVar(value=False)
        ttk.Checkbutton(robo, text="Limpar a flash antes de gravar", variable=self.limpar_antes).grid(
            row=1, column=1, columnspan=2, sticky="w", pady=(8, 0))
        botoes = ttk.Frame(robo)
        botoes.grid(row=2, column=0, columnspan=3, sticky="w", pady=(10, 0))
        for texto, comando in (("Compilar", self.compilar_codigo), ("Compilar e gravar", self.gravar_codigo),
                               ("Limpar flash", self.limpar_flash)):
            botao = ttk.Button(botoes, text=texto, command=comando)
            botao.pack(side="left", padx=(0, 8))
            self.botoes_firmware.append(botao)
        ttk.Label(robo, text="Limpar a flash apaga a NVS (pinos, parametros, iniciacoes salvas)\n"
                             "e os rounds guardados. Colete os rounds antes.",
                  style="Aviso.TLabel").grid(row=3, column=0, columnspan=3, sticky="w", pady=(10, 0))

        # GitHub
        github = ttk.LabelFrame(aba, text="3. Versao no GitHub", padding=8)
        github.grid(row=1, column=0, columnspan=2, sticky="ew", padx=6, pady=6)
        github.columnconfigure(1, weight=1)
        ttk.Label(github, text="Branch:").grid(row=0, column=0, sticky="w")
        linha_branch = ttk.Frame(github)
        linha_branch.grid(row=0, column=1, sticky="w")
        self.branch = ttk.Combobox(linha_branch, width=30)  # Editavel: um nome novo cria a branch
        self.branch.pack(side="left", padx=4)
        botao = ttk.Button(linha_branch, text="Atualizar", command=self.atualizar_git)
        botao.pack(side="left", padx=4)
        self.botoes_firmware.append(botao)
        self.git_info = ttk.Label(linha_branch, text="", style="Dica.TLabel")
        self.git_info.pack(side="left", padx=8)
        ttk.Label(github, text="Comentario:").grid(row=1, column=0, sticky="nw", pady=(8, 0))
        self.mensagem_commit = tk.Text(github, height=3, font=("Segoe UI", 10))
        self.mensagem_commit.grid(row=1, column=1, sticky="ew", padx=4, pady=(8, 0))
        acoes = ttk.Frame(github)
        acoes.grid(row=2, column=1, sticky="w", pady=(8, 0))
        botao = ttk.Button(acoes, text="Commitar", command=self.commitar_codigo)
        botao.pack(side="left", padx=4)
        self.botoes_firmware.append(botao)
        self.enviar_github = tk.BooleanVar(value=True)
        ttk.Checkbutton(acoes, text="Enviar para o GitHub (push)", variable=self.enviar_github).pack(side="left", padx=8)

        # Log
        quadro_log = ttk.LabelFrame(aba, text="Log", padding=4)
        quadro_log.grid(row=2, column=0, columnspan=2, sticky="nsew", padx=6, pady=(0, 6))
        quadro_log.columnconfigure(0, weight=1)
        quadro_log.rowconfigure(0, weight=1)
        self.log_fw = tk.Text(quadro_log, height=10, font=("Consolas", 9), state="disabled", borderwidth=0, wrap="none")
        self.log_fw.grid(row=0, column=0, sticky="nsew")
        rolagem = ttk.Scrollbar(quadro_log, orient="vertical", command=self.log_fw.yview)
        rolagem.grid(row=0, column=1, sticky="ns")
        self.log_fw.config(yscrollcommand=rolagem.set)
        rodape = ttk.Frame(quadro_log)
        rodape.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(4, 0))
        self.etapa_fw = ttk.Label(rodape, text="Pronto")
        self.etapa_fw.pack(side="left")
        self.botao_cancelar = ttk.Button(rodape, text="Cancelar", command=self.cancelar_firmware, state="disabled")
        self.botao_cancelar.pack(side="right")
        ttk.Button(rodape, text="Limpar log", command=self.limpar_log_fw).pack(side="right", padx=8)

        self.mostrar_mac_codigo()

    def registrar_fw(self, texto, tag="saida"):
        self.log_fw.config(state="normal")
        self.log_fw.insert("end", texto + "\n", tag)
        if int(self.log_fw.index("end-1c").split(".")[0]) > LOG_FIRMWARE_LINHAS:
            self.log_fw.delete("1.0", "500.0")  # Limita a memoria do log
        self.log_fw.see("end")
        self.log_fw.config(state="disabled")

    def limpar_log_fw(self):
        self.log_fw.config(state="normal")
        self.log_fw.delete("1.0", "end")
        self.log_fw.config(state="disabled")

    def mostrar_mac_codigo(self):
        mac, arquivo, linha = ps4_mac.mac_do_codigo()
        if mac:
            self.ps4_codigo.config(text=f"MAC do robo no codigo: {mac} ({arquivo.name}:{linha})")
            self.ps4_novo.delete(0, "end")
            self.ps4_novo.insert(0, mac)
        else:
            self.ps4_codigo.config(text=f"FUEGO nao encontrado em {arquivo.name}")

    def porta_firmware(self):
        if not self.porta_fw.get():
            messagebox.showwarning("Firmware", "Escolha a porta do robo")
            return None
        return self.porta_fw.get().split(" - ")[0]

    def executar_firmware(self, titulo, trabalho, ao_terminar=None):
        """Roda trabalho(execucao, log, progresso) numa thread. O log e o progresso voltam pela fila"""
        self.definir_ocupado(True)
        self.execucao = firmware.Execucao()
        self.botao_cancelar.config(state="normal")
        self.registrar_fw(f"[{datetime.now():%H:%M:%S}] {titulo}", "etapa")
        self.fila.put(("fw_progresso", 0, titulo))
        execucao = self.execucao

        def log(texto):
            self.fila.put(("fw_log", texto, "etapa" if texto.startswith(">>") else "saida"))

        def progresso(porcentagem, texto):
            self.fila.put(("fw_progresso", porcentagem, texto))

        def rodar():
            try:
                resultado = trabalho(execucao, log, progresso)
                self.fila.put(("fw_fim", True, resultado, ao_terminar))
            except (firmware.ErroFirmware, ps4_mac.ErroPS4) as erro:
                self.fila.put(("fw_fim", False, str(erro), None))
            except Exception as erro:          # Nao deixa a thread morrer sem avisar a interface
                self.fila.put(("fw_fim", False, f"Erro inesperado: {erro}", None))

        threading.Thread(target=rodar, daemon=True).start()

    def terminar_firmware(self, ok, resultado, ao_terminar):
        self.definir_ocupado(False)
        self.execucao = None
        self.botao_cancelar.config(state="disabled")
        if ok:
            texto = resultado if isinstance(resultado, str) else "Concluido"
            self.registrar_fw(f"OK: {texto}", "ok")
            self.etapa_fw.config(text=texto)
            self.definir_status(texto, 100)
            if ao_terminar:
                ao_terminar(resultado)
        else:
            self.registrar_fw(f"ERRO: {resultado}", "erro")
            self.etapa_fw.config(text="Erro: veja o log")
            self.definir_status("Erro: veja o log da aba Firmware", 0)
            messagebox.showerror("Firmware", resultado)

    def cancelar_firmware(self):
        if self.execucao:
            self.registrar_fw("Cancelando...", "etapa")
            self.execucao.cancelar()

    # Controle PS4

    def ler_controle(self):
        def trabalho(execucao, log, progresso):
            log(">> Procurando um DualShock 4 na USB")
            with ps4_mac.abrir_controle() as controle:
                proprio, mestre = controle.ler()
            log(f"{controle.modelo}: MAC {proprio}, mestre {mestre}")
            progresso(100, "Controle lido")
            return controle.modelo, proprio, mestre

        self.executar_firmware("Lendo o controle PS4", trabalho, self.mostrar_controle)

    def mostrar_controle(self, dados):
        modelo, proprio, mestre = dados
        self.ps4_modelo.config(text=modelo)
        self.ps4_proprio.config(text=proprio)
        mac_codigo = ps4_mac.mac_do_codigo()[0]
        igual = mac_codigo and mestre == mac_codigo
        self.ps4_mestre.config(text=mestre + ("  (igual ao codigo)" if igual else "  (diferente do codigo)"),
                               style="TLabel" if igual else "Aviso.TLabel")
        self.etapa_fw.config(text="Controle lido")
        self.registrar_fw("O mestre ja e o MAC do robo: nada a fazer" if igual else
                          "O mestre e diferente do MAC do robo: use Atualizar mestre", "ok" if igual else "etapa")

    def atualizar_mestre(self):
        novo = self.ps4_novo.get().strip().lower()
        if not ps4_mac.mac_valido(novo):
            messagebox.showwarning("Firmware", "MAC invalido. Use o formato aa:bb:cc:dd:ee:ff")
            return
        if not messagebox.askyesno("Firmware", f"Gravar {novo} como mestre do controle?\n"
                                               "Depois disso, ele so se conecta a esse MAC."):
            return

        def trabalho(execucao, log, progresso):
            log(">> Procurando um DualShock 4 na USB")
            with ps4_mac.abrir_controle() as controle:
                proprio, antigo = controle.ler()
                log(f"{controle.modelo}: MAC {proprio}, mestre atual {antigo}")
                progresso(40, "Gravando o mestre novo")
                log(f">> Gravando o mestre {novo}")
                controle.gravar_mestre(novo)
                progresso(80, "Conferindo")
                log(">> Lendo de novo para conferir")
                _, mestre = controle.ler()
            if mestre != novo:
                raise ps4_mac.ErroPS4(f"O controle continua com o mestre {mestre}. Desconecte, conecte e tente de novo")
            progresso(100, f"Mestre atualizado: {mestre}")
            return controle.modelo, proprio, mestre

        self.executar_firmware("Atualizando o mestre do controle PS4", trabalho, self.mostrar_controle)

    # Robo

    def compilar_codigo(self):
        def trabalho(execucao, log, progresso):
            firmware.compilar(execucao, log, progresso)
            return "Compilado sem erros"

        self.executar_firmware("Compilando o codigo", trabalho)

    def gravar_codigo(self):
        porta = self.porta_firmware()
        if not porta:
            return
        limpar = self.limpar_antes.get()
        if limpar and not messagebox.askyesno("Firmware", AVISO_LIMPAR_FLASH):
            return

        def trabalho(execucao, log, progresso):
            if limpar:
                log(">> Apagando a flash")
                firmware.limpar_flash(execucao, porta, log, lambda p, t: progresso(p * 0.2, t))
            log(f">> Compilando e gravando em {porta}")
            firmware.gravar(execucao, porta, log, (lambda p, t: progresso(20 + p * 0.8, t)) if limpar else progresso)
            return f"Firmware gravado em {porta}"

        self.executar_firmware(f"Compilando e gravando em {porta}" + (" (com a flash limpa antes)" if limpar else ""),
                               trabalho)

    def limpar_flash(self):
        porta = self.porta_firmware()
        if not porta or not messagebox.askyesno("Firmware", AVISO_LIMPAR_FLASH +
                                                "\n\nO robo fica sem programa ate gravar de novo."):
            return

        def trabalho(execucao, log, progresso):
            firmware.limpar_flash(execucao, porta, log, progresso)
            return f"Flash apagada em {porta}. Grave o firmware de novo"

        self.executar_firmware(f"Apagando a flash em {porta}", trabalho)

    # GitHub

    def atualizar_git(self):
        def trabalho(execucao, log, progresso):
            return firmware.estado_git(execucao)

        self.executar_firmware("Lendo o estado do git", trabalho, self.mostrar_git)

    def mostrar_git(self, estado):
        self.estado_git = estado
        self.branch["values"] = estado["branches"]
        if not self.branch.get():
            self.branch.set(estado["atual"])
        self.git_info.config(text=f"Atual: {estado['atual']} | {len(estado['alterados'])} arquivo(s) alterado(s)"
                                  + (f" | {estado['remoto']}" if estado["remoto"] else " | sem remoto"))
        if estado["alterados"]:
            self.registrar_fw("Arquivos alterados:", "etapa")
            for linha in estado["alterados"]:
                self.registrar_fw("  " + linha)
        self.etapa_fw.config(text="Estado do git lido")

    def commitar_codigo(self):
        branch = self.branch.get().strip()
        mensagem = self.mensagem_commit.get("1.0", "end").strip()
        if not branch or not mensagem:
            messagebox.showwarning("Firmware", "Escolha a branch e escreva o comentario do commit")
            return
        estado = getattr(self, "estado_git", None) or {"atual": "?", "alterados": [], "branches": []}
        alterados = estado["alterados"]
        lista = "\n".join(alterados[:15]) + (f"\n... e mais {len(alterados) - 15}" if len(alterados) > 15 else "")
        nova = "" if branch in estado["branches"] else " (branch nova)"
        enviar = self.enviar_github.get()
        if not messagebox.askyesno(
                "Firmware",
                f"Commitar todas as alteracoes na branch {branch}{nova}"
                + (" e enviar para o GitHub" if enviar else "") + f"?\n\nComentario: {mensagem}\n\n"
                + (f"Arquivos ({len(alterados)}):\n{lista}" if alterados else "Use Atualizar para ver os arquivos")):
            return

        def trabalho(execucao, log, progresso):
            hash_curto = firmware.commitar(execucao, branch, mensagem, enviar, log, progresso)
            texto = f"Commit {hash_curto} na branch {branch}" + (" enviado ao GitHub" if enviar else "")
            return texto

        def depois(_):
            self.mensagem_commit.delete("1.0", "end")
            self.raiz.after(100, self.atualizar_git)

        self.executar_firmware(f"Commit na branch {branch}", trabalho, depois)

#=========================================//Parametros//========================================//

    def montar_parametros(self):
        aba = self.aba_parametros
        topo = ttk.Frame(aba, padding=6)
        topo.pack(fill="x")
        self.botao_aplicar_param = ttk.Button(topo, text="Aplicar no codigo", command=self.aplicar_parametros)
        self.botao_aplicar_param.pack(side="left")
        ttk.Button(topo, text="Descartar alteracoes", command=self.descartar_parametros).pack(side="left", padx=6)
        ttk.Button(topo, text="Recarregar codigo", command=self.recarregar_parametros).pack(side="left")
        self.resumo_param = ttk.Label(topo, text="", style="Dica.TLabel")
        self.resumo_param.pack(side="left", padx=16)

        colunas = [("codigo", "Codigo", 80), ("novo", "Novo valor", 90), ("faixa", "Faixa", 90),
                   ("nvs", "NVS", 50), ("comentario", "Comentario", 380), ("local", "Local", 130)]
        quadro = ttk.Frame(aba)
        quadro.pack(fill="both", expand=True, padx=6)
        self.tabela_param = ttk.Treeview(quadro, show="tree headings", columns=[c[0] for c in colunas])
        self.tabela_param.heading("#0", text="Parametro")
        self.tabela_param.column("#0", width=260, minwidth=120)
        for coluna, titulo, largura in colunas:
            self.tabela_param.heading(coluna, text=titulo)
            self.tabela_param.column(coluna, width=largura, minwidth=40, stretch=(coluna == "comentario"))
        rolagem = ttk.Scrollbar(quadro, orient="vertical", command=self.tabela_param.yview)
        self.tabela_param.config(yscrollcommand=rolagem.set)
        self.tabela_param.pack(side="left", fill="both", expand=True)
        rolagem.pack(side="right", fill="y")
        self.tabela_param.bind("<Double-1>", self.editar_parametro)
        self.tabela_param.bind("<Return>", self.editar_parametro)

        ttk.Label(aba, padding=6, style="Dica.TLabel", justify="left", text=(
            "Dois cliques (ou ENTER) na linha para digitar o valor novo; ENTER confirma e ESC cancela. Amarelo: alterado, "
            "ainda nao aplicado.\nAplicar troca so o numero no codigo. Depois, compile e grave na aba Firmware. "
            "Itens da NVS: um valor salvo no robo vale mais que o do codigo.")).pack(fill="x")

        self.parametros = {}                  # iid -> parametro
        self.pendentes = {}                   # iid -> valor novo
        self.editor_param = None
        self.recarregar_parametros()

    def recarregar_parametros(self):
        if self.pendentes and not messagebox.askyesno("Parametros", "Descartar as alteracoes nao aplicadas?"):
            return
        self.fechar_editor_parametro()
        self.pendentes = {}
        self.parametros = {}
        abertos = {self.tabela_param.item(g, "text") for g in self.tabela_param.get_children()
                   if self.tabela_param.item(g, "open")}
        self.tabela_param.delete(*self.tabela_param.get_children())
        try:
            lista = parametros.ler_parametros()
        except OSError as erro:
            self.resumo_param.config(text=f"Nao foi possivel ler o codigo: {erro}")
            return

        grupos = {}
        for i, p in enumerate(lista):
            if p["grupo"] not in grupos:
                grupos[p["grupo"]] = self.tabela_param.insert(
                    "", "end", text=p["grupo"], open=(p["grupo"] in abertos or not abertos), tags=("grupo",))
            iid = f"p{i}"
            self.parametros[iid] = p
            minimo, maximo = parametros.faixa(p)
            texto_faixa = "" if minimo is None and maximo is None else \
                f"{'' if minimo is None else minimo}..{'' if maximo is None else maximo}"
            self.tabela_param.insert(grupos[p["grupo"]], "end", iid=iid, text=p["variavel"], values=(
                p["valor"], "", texto_faixa, p["nvs"]["chave"] if p["nvs"] else "", p["comentario"],
                f"{p['arquivo'].name}:{p['linha']}"))
        self.atualizar_resumo_param(len(lista))

    def atualizar_resumo_param(self, total=None):
        total = total if total is not None else len(self.parametros)
        self.resumo_param.config(text=f"{total} parametros | {len(self.pendentes)} alterado(s), nao aplicado(s)")

    def editar_parametro(self, evento):
        if evento.type == tk.EventType.KeyPress:
            iid = self.tabela_param.focus()
        else:
            iid = self.tabela_param.identify_row(evento.y)
        if iid not in self.parametros:
            return
        self.fechar_editor_parametro()
        self.tabela_param.see(iid)
        self.tabela_param.update_idletasks()
        caixa = self.tabela_param.bbox(iid, "novo")
        if not caixa:
            return
        x, y, largura, altura = caixa
        editor = ttk.Entry(self.tabela_param)
        editor.insert(0, str(self.pendentes.get(iid, self.parametros[iid]["valor"])))
        editor.select_range(0, "end")
        editor.place(x=x, y=y, width=largura, height=altura)
        editor.focus_set()
        editor.bind("<Return>", lambda _: self.confirmar_parametro(iid))
        editor.bind("<KP_Enter>", lambda _: self.confirmar_parametro(iid))
        editor.bind("<Escape>", lambda _: self.fechar_editor_parametro())
        editor.bind("<FocusOut>", lambda _: self.confirmar_parametro(iid, silencioso=True))
        self.editor_param = editor

    def fechar_editor_parametro(self):
        if self.editor_param:
            editor, self.editor_param = self.editor_param, None
            editor.destroy()

    def confirmar_parametro(self, iid, silencioso=False):
        if not self.editor_param:
            return
        texto = self.editor_param.get()
        p = self.parametros[iid]
        try:
            valor = parametros.validar(p, texto)
        except ValueError as erro:
            if silencioso:
                self.fechar_editor_parametro()
                self.definir_status(f"Valor ignorado: {erro}")
            else:
                messagebox.showwarning("Parametros", str(erro))
                self.editor_param.focus_set()
            return
        self.fechar_editor_parametro()
        if valor == p["valor"]:
            self.pendentes.pop(iid, None)
            self.tabela_param.set(iid, "novo", "")
            self.tabela_param.item(iid, tags=())
        else:
            self.pendentes[iid] = valor
            self.tabela_param.set(iid, "novo", valor)
            self.tabela_param.item(iid, tags=("diferente",))
        self.atualizar_resumo_param()
        self.tabela_param.focus_set()
        self.tabela_param.focus(iid)
        self.tabela_param.selection_set(iid)

    def descartar_parametros(self):
        if not self.pendentes:
            return
        if messagebox.askyesno("Parametros", f"Descartar {len(self.pendentes)} alteracao(oes)?"):
            self.pendentes = {}
            self.recarregar_parametros()

    def aplicar_parametros(self):
        self.fechar_editor_parametro()
        if not self.pendentes:
            messagebox.showinfo("Parametros", "Nenhum valor alterado. De dois cliques numa linha para editar")
            return
        alteracoes = [(self.parametros[iid], valor) for iid, valor in self.pendentes.items()]
        previa = "\n".join(f"{p['variavel']}: {p['valor']} -> {novo}  ({p['arquivo'].name}:{p['linha']})"
                           for p, novo in alteracoes)
        da_nvs = [p["variavel"] for p, _ in alteracoes if p["nvs"]]
        aviso = ("\n\nConfiguraveis pela NVS: " + ", ".join(da_nvs) + ". Se o robo tiver um valor salvo deles, "
                 "o salvo continua valendo (ajuste pelo menu de Configuracao ou limpe a flash).") if da_nvs else ""
        if not messagebox.askyesno("Parametros", f"Trocar no codigo:\n\n{previa}{aviso}\n\nContinuar?"):
            return
        try:
            feitas = parametros.aplicar(alteracoes)
        except (coleta.ErroColeta, OSError) as erro:
            self.pendentes = {}
            self.recarregar_parametros()
            messagebox.showerror("Parametros", f"{erro}\nNada foi alterado neste arquivo. Os valores foram recarregados")
            return
        for descricao in feitas:
            self.registrar(f"Codigo alterado: {descricao}")
            self.registrar_fw(f"Codigo alterado: {descricao}", "etapa")
        self.pendentes = {}
        self.recarregar_parametros()
        self.recarregar_codigo()              # A aba NVS mostra os mesmos valores do codigo
        self.definir_status(f"{len(feitas)} parametro(s) aplicados ao codigo. Compile e grave na aba Firmware", 100)
        messagebox.showinfo("Parametros", f"{len(feitas)} valor(es) aplicados ao codigo.\n"
                                          "Compile e grave o firmware na aba Firmware para valerem no robo.")

#===========================================//Dados//===========================================//

    def montar_dados(self):
        aba = self.aba_dados
        topo = ttk.Frame(aba, padding=6)
        topo.pack(fill="x")

        ttk.Label(topo, text="Arquivo:").pack(side="left")
        self.arquivo = ttk.Combobox(topo, width=45, state="readonly")
        self.arquivo.pack(side="left", padx=4)
        self.arquivo.bind("<<ComboboxSelected>>", lambda _: self.carregar_tabela())
        ttk.Button(topo, text="Atualizar", command=self.atualizar_arquivos).pack(side="left", padx=4)
        ttk.Button(topo, text="Abrir pasta", command=self.abrir_pasta).pack(side="left", padx=4)

        ttk.Label(topo, text="Filtro:").pack(side="left", padx=(16, 0))
        self.filtro = tk.StringVar()
        self.filtro.trace_add("write", lambda *_: self.preencher_tabela())
        ttk.Entry(topo, textvariable=self.filtro, width=25).pack(side="left", padx=4)

        self.botao_amostras = ttk.Button(topo, text="Ver amostras", command=lambda: self.abrir_da_luta("amostras"))
        self.botao_amostras.pack(side="left", padx=(16, 4))
        self.botao_eventos = ttk.Button(topo, text="Ver eventos", command=lambda: self.abrir_da_luta("eventos"))
        self.botao_eventos.pack(side="left", padx=4)

        self.contagem = ttk.Label(topo, text="")
        self.contagem.pack(side="right")

        quadro = ttk.Frame(aba)
        quadro.pack(fill="both", expand=True, padx=6, pady=(0, 6))
        self.tabela = ttk.Treeview(quadro, show="headings")
        rolagem_v = ttk.Scrollbar(quadro, orient="vertical", command=self.tabela.yview)
        rolagem_h = ttk.Scrollbar(quadro, orient="horizontal", command=self.tabela.xview)
        self.tabela.config(yscrollcommand=rolagem_v.set, xscrollcommand=rolagem_h.set)
        self.tabela.grid(row=0, column=0, sticky="nsew")
        rolagem_v.grid(row=0, column=1, sticky="ns")
        rolagem_h.grid(row=1, column=0, sticky="ew")
        quadro.rowconfigure(0, weight=1)
        quadro.columnconfigure(0, weight=1)
        self.tabela.bind("<Double-1>", lambda _: self.abrir_da_luta("amostras"))

        self.cabecalho = []
        self.linhas_tabela = []
        self.ordem = (None, False)

    def atualizar_arquivos(self):
        pasta = Path(self.pasta.get())
        arquivos = []
        if pasta.is_dir():
            if (pasta / "lutas.csv").exists():
                arquivos.append("lutas.csv")
            arquivos += sorted((p.name for p in pasta.glob("luta_*.csv")), reverse=True)
        atual = self.arquivo.get()
        self.arquivo["values"] = arquivos
        if atual in arquivos:
            self.arquivo.set(atual)
        elif arquivos:
            self.arquivo.current(0)
        else:
            self.arquivo.set("")
        self.carregar_tabela()

    def abrir_pasta(self):
        pasta = Path(self.pasta.get())
        pasta.mkdir(parents=True, exist_ok=True)
        os.startfile(pasta)

    def carregar_tabela(self):
        self.cabecalho, self.linhas_tabela = [], []
        nome = self.arquivo.get()
        if nome:
            try:
                with open(Path(self.pasta.get()) / nome, newline="", encoding="utf-8") as f:
                    leitor = csv.reader(f)
                    self.cabecalho = next(leitor, [])
                    self.linhas_tabela = list(leitor)
            except OSError as erro:
                self.registrar(f"Nao foi possivel abrir {nome}: {erro}")

        self.tabela["columns"] = self.cabecalho
        for coluna in self.cabecalho:
            self.tabela.heading(coluna, text=coluna, command=lambda c=coluna: self.ordenar(c))
            largura = max(60, min(220, 9 * len(coluna) + 16))
            self.tabela.column(coluna, width=largura, minwidth=40, stretch=False, anchor="w")
        self.ordem = (None, False)

        de_resumo = (nome == "lutas.csv")
        estado = "normal" if de_resumo else "disabled"
        self.botao_amostras.config(state=estado)
        self.botao_eventos.config(state=estado)
        self.preencher_tabela()

    def preencher_tabela(self):
        self.tabela.delete(*self.tabela.get_children())
        filtro = self.filtro.get().strip().lower()
        visiveis = 0
        for linha in self.linhas_tabela:
            if filtro and not any(filtro in valor.lower() for valor in linha):
                continue
            self.tabela.insert("", "end", values=linha)
            visiveis += 1
        self.contagem.config(text=f"{visiveis} de {len(self.linhas_tabela)} linhas")

    def ordenar(self, coluna):
        indice = self.cabecalho.index(coluna)
        decrescente = (self.ordem[0] == coluna and not self.ordem[1])

        def chave(linha):
            valor = linha[indice] if indice < len(linha) else ""
            try:
                return (0, float(valor), "")
            except ValueError:
                return (1, 0.0, valor.lower())

        self.linhas_tabela.sort(key=chave, reverse=decrescente)
        self.ordem = (coluna, decrescente)
        for c in self.cabecalho:
            seta = (" v" if decrescente else " ^") if c == coluna else ""
            self.tabela.heading(c, text=c + seta)
        self.preencher_tabela()

    def abrir_da_luta(self, tipo):
        # Da linha selecionada em lutas.csv para a serie (amostras ou eventos) da mesma luta
        if self.arquivo.get() != "lutas.csv" or "luta" not in self.cabecalho:
            return
        selecionado = self.tabela.selection()
        if not selecionado:
            messagebox.showinfo("Dados", "Selecione uma luta na tabela")
            return
        luta = self.tabela.item(selecionado[0], "values")[self.cabecalho.index("luta")]
        nome = f"{luta}_{tipo}.csv"
        if nome not in self.arquivo["values"]:
            messagebox.showwarning("Dados", f"Arquivo nao encontrado: {nome}")
            return
        self.arquivo.set(nome)
        self.filtro.set("")
        self.carregar_tabela()

#============================================//NVS//============================================//

    def montar_nvs(self):
        aba = self.aba_nvs
        topo = ttk.Frame(aba, padding=6)
        topo.pack(fill="x")

        self.botao_ler_nvs = ttk.Button(topo, text="Ler do robo (USB)", command=self.ler_nvs)
        self.botao_ler_nvs.pack(side="left")
        ttk.Button(topo, text="Recarregar codigo", command=self.recarregar_codigo).pack(side="left", padx=6)
        ttk.Button(topo, text="Selecionar diferentes", command=self.selecionar_diferentes).pack(side="left", padx=(16, 6))
        ttk.Button(topo, text="Aplicar valores do robo ao codigo", command=self.aplicar_nvs).pack(side="left")
        ttk.Label(topo, text="Usa a porta da aba Coleta", style="Dica.TLabel").pack(side="right")

        colunas = [("id", "ID", 40), ("nome", "Componente", 170), ("chave", "Chave", 60),
                   ("robo", "Robo", 70), ("padrao", "Padrao gravado", 100), ("codigo", "Codigo", 70),
                   ("nvs", "Da NVS", 60), ("faixa", "Faixa", 80), ("variavel", "Variavel", 170),
                   ("local", "Local", 140), ("situacao", "Situacao", 260)]
        quadro = ttk.Frame(aba)
        quadro.pack(fill="both", expand=True, padx=6)
        self.tabela_nvs = ttk.Treeview(quadro, show="headings", columns=[c[0] for c in colunas])
        for coluna, titulo, largura in colunas:
            self.tabela_nvs.heading(coluna, text=titulo)
            self.tabela_nvs.column(coluna, width=largura, minwidth=40, stretch=(coluna == "situacao"))
        rolagem = ttk.Scrollbar(quadro, orient="vertical", command=self.tabela_nvs.yview)
        self.tabela_nvs.config(yscrollcommand=rolagem.set)
        self.tabela_nvs.pack(side="left", fill="both", expand=True)
        rolagem.pack(side="right", fill="y")

        ttk.Label(aba, padding=6, style="Dica.TLabel", justify="left", text=(
            "Amarelo: o robo usa um valor diferente do codigo (salvo na NVS). Vermelho: o firmware gravado foi "
            "compilado com outro padrao.\nAplicar troca so o numero da inicializacao no codigo. Depois, compile e "
            "grave o firmware (pio run -t upload).")).pack(fill="x")

        self.config_robo = None                # Ultima tabela lida do robo
        self.mapa_codigo = {}

    def recarregar_codigo(self):
        try:
            self.mapa_codigo = config_nvs.mapear_codigo()
        except OSError as erro:
            self.mapa_codigo = {}
            self.registrar(f"Nao foi possivel ler o codigo: {erro}")
        self.preencher_nvs()

    def ler_nvs(self):
        if not self.porta.get():
            messagebox.showwarning("NVS", "Escolha a porta do robo na aba Coleta")
            return
        try:
            baud = int(self.baud.get())
        except ValueError:
            messagebox.showwarning("NVS", "Baud invalido")
            return
        porta = self.porta.get().split(" - ")[0]
        self.definir_ocupado(True)
        self.definir_status(f"Lendo a configuracao em {porta}...", 30)

        def trabalho():
            try:
                self.fila.put(("config", config_nvs.ler_config_serial(porta, baud)))
            except coleta.ErroColeta as erro:
                self.fila.put(("erro", str(erro)))
            except Exception as erro:
                self.fila.put(("erro", f"Erro inesperado: {erro}"))

        threading.Thread(target=trabalho, daemon=True).start()

    def receber_config(self, itens):
        self.config_robo = itens
        self.recarregar_codigo()              # O codigo pode ter mudado desde a abertura
        diferentes = sum(1 for i in itens if i["valor"] != i["padrao"])
        self.definir_status(f"Configuracao lida: {len(itens)} itens, {diferentes} fora do padrao", 100)
        self.registrar(f"Configuracao lida do robo: {len(itens)} itens")

    def linhas_nvs(self):
        # Une a tabela do robo (se lida) com o mapa do codigo, na ordem dos IDs
        robo = {i["chave"]: i for i in (self.config_robo or [])}
        chaves = list(robo) + [c for c in self.mapa_codigo if c not in robo]
        for chave in chaves:
            item = robo.get(chave)
            codigo = self.mapa_codigo.get(chave, {})
            yield chave, item, codigo

    def preencher_nvs(self):
        self.tabela_nvs.delete(*self.tabela_nvs.get_children())
        for chave, item, codigo in self.linhas_nvs():
            valor_codigo = codigo.get("valor")
            local = f"{codigo['arquivo'].name}:{codigo['linha']}" if codigo.get("arquivo") else ""
            tag = ""
            if codigo.get("arquivo") is None:
                situacao = "variavel nao encontrada no codigo"
            elif item is None:
                situacao = "sem leitura do robo"
            else:
                partes = []
                if item["valor"] != valor_codigo:
                    partes.append("robo difere do codigo")
                    tag = "diferente"
                if item["padrao"] != valor_codigo:
                    partes.append("firmware gravado com outro padrao (compile e grave)")
                    tag = "aviso"
                situacao = ", ".join(partes) or "igual"

            faixa = ""
            if item and item["minimo"] is not None:
                faixa = f"{item['minimo']}-{item['maximo']}"
            valores = (item["id"] if item else "", item["nome"] if item else "", chave,
                       item["valor"] if item else "", item["padrao"] if item else "",
                       valor_codigo if valor_codigo is not None else "",
                       ("sim" if item["nvs"] else "nao") if item else "", faixa,
                       codigo.get("variavel", ""), local, situacao)
            self.tabela_nvs.insert("", "end", iid=chave, values=valores, tags=(tag,) if tag else ())

    def selecionar_diferentes(self):
        diferentes = [chave for chave, item, codigo in self.linhas_nvs()
                      if item and codigo.get("valor") is not None and item["valor"] != codigo["valor"]]
        self.tabela_nvs.selection_set(diferentes)
        if not diferentes:
            self.definir_status("Nenhum valor do robo difere do codigo" if self.config_robo
                                else "Leia a configuracao do robo primeiro")

    def aplicar_nvs(self):
        if not self.config_robo:
            messagebox.showinfo("NVS", "Leia a configuracao do robo primeiro")
            return
        robo = {i["chave"]: i for i in self.config_robo}
        alteracoes = []
        for chave in self.tabela_nvs.selection():
            item, codigo = robo.get(chave), self.mapa_codigo.get(chave, {})
            if item and codigo.get("arquivo") and item["valor"] != codigo["valor"]:
                alteracoes.append((codigo["variavel"], codigo["arquivo"], codigo["linha"],
                                   codigo["valor"], item["valor"]))
        if not alteracoes:
            messagebox.showinfo("NVS", "Selecione itens em que o valor do robo difere do codigo")
            return

        previa = "\n".join(f"{v}: {antigo} -> {novo}  ({Path(a).name}:{l})" for v, a, l, antigo, novo in alteracoes)
        if not messagebox.askyesno("NVS", f"Trocar no codigo:\n\n{previa}\n\nContinuar?"):
            return
        try:
            feitas = config_nvs.aplicar_no_codigo(alteracoes)
        except (coleta.ErroColeta, OSError) as erro:
            self.recarregar_codigo()
            messagebox.showerror("NVS", str(erro))
            return
        for descricao in feitas:
            self.registrar(f"Codigo alterado: {descricao}")
        self.recarregar_codigo()
        self.definir_status(f"{len(feitas)} valor(es) aplicados ao codigo. Compile e grave o firmware", 100)
        messagebox.showinfo("NVS", "Valores aplicados ao codigo.\nCompile e grave o firmware para valerem como padrao.")


#==========================================//Abertura//=========================================//

def arte_fuego():
    # A arte fica no cabecalho do src/main.cpp, entre as duas linhas de blocos cheios
    try:
        linhas = MAIN_CPP.read_text(encoding="utf-8").splitlines()
        inicio = next(i for i, l in enumerate(linhas) if l.startswith("█"))
        fim = next(i for i in range(inicio + 1, len(linhas)) if linhas[i].startswith("█"))
        # As linhas de blocos so servem de referencia para centralizar: nao aparecem na abertura
        return centralizar_arte(linhas[inicio + 1:fim])
    except (OSError, StopIteration):
        return ["FUEGO"]


def centralizar_arte(linhas):
    """Centraliza cada bloco da arte (o desenho e o nome) um sobre o outro.

    No main.cpp os dois blocos tem centros diferentes. Cada bloco (linhas seguidas com conteudo) e
    movido inteiro, para nao desalinhar as linhas entre si, ate o centro do bloco mais largo. O
    braille vazio (U+2800) conta como espaco
    """
    vazio = " ⠀"
    linhas = [l.rstrip(vazio) for l in linhas]
    resultado = list(linhas)

    blocos, bloco = [], []
    for i, linha in enumerate(linhas):
        if linha.strip(vazio):
            bloco.append(i)
        elif bloco:
            blocos.append(bloco)
            bloco = []
    if bloco:
        blocos.append(bloco)

    def limites(indices):
        esquerda = min(len(linhas[i]) - len(linhas[i].lstrip(vazio)) for i in indices if linhas[i].strip(vazio))
        return esquerda, max(len(linhas[i]) for i in indices)

    largura = max(d - e for e, d in map(limites, blocos))

    for indices in blocos:
        esquerda, direita = limites(indices)
        deslocamento = (largura - (direita - esquerda)) // 2 - esquerda
        for i in indices:
            linha = linhas[i]
            resultado[i] = " " * deslocamento + linha if deslocamento >= 0 else linha[-deslocamento:]
    return resultado


def fonte_arte(raiz):
    # A arte mistura braille e ASCII: precisa de uma fonte em que os dois tenham a mesma largura
    for familia in FONTES_ARTE:
        fonte = tkfont.Font(root=raiz, family=familia, size=ABERTURA_FONTE)
        if fonte.measure("⣿") == fonte.measure("#") == fonte.measure("█"):
            return fonte
    return tkfont.Font(root=raiz, family="Consolas", size=ABERTURA_FONTE)


def cor_degrade(fracao):
    inicio, fim = ABERTURA_DEGRADE
    r, g, b = (round(a + (c - a) * fracao) for a, c in zip(inicio, fim))
    return f"#{r:02x}{g:02x}{b:02x}"


def misturar(cor_a, cor_b, fracao):
    """Cor entre cor_a (fracao 0) e cor_b (fracao 1), em '#rrggbb'"""
    a = [int(cor_a[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(cor_b[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(x + (y - x) * fracao):02x}" for x, y in zip(a, b))


def blocos_arte(linhas):
    """Separa a arte em blocos de linhas com conteudo (no main.cpp: o desenho e o nome)"""
    blocos, bloco = [], []
    for linha in linhas:
        if linha.strip(" ⠀"):
            bloco.append(linha)
        elif bloco:
            blocos.append(bloco)
            bloco = []
    if bloco:
        blocos.append(bloco)
    return blocos


#=====================================//Imagens do robo//=======================================//

def linhas_ascii_robo():
    try:
        return ARTE_ROBO.read_text(encoding="utf-8").splitlines()
    except OSError:
        return []


def imagem_ascii_robo(linhas, tema, fundo=None):
    """Cada caractere vira um bloco de 1 x 2 pixels com a cor dele: a fonte que caberia na tela
    (~2 px) borra as letras, e a imagem mantem a proporcao de um caractere (o dobro da altura)"""
    if not linhas:
        return None
    cores = ARTE_ROBO_CORES[tema]
    fundo = fundo or TEMAS[tema]["fundo"]
    colunas = max(len(l) for l in linhas)
    linhas_px = []
    for linha in linhas:
        px = "{" + " ".join(cores.get(c, fundo) for c in linha.ljust(colunas)) + "}"
        linhas_px += [px, px]
    imagem = tk.PhotoImage(width=colunas, height=2 * len(linhas))
    imagem.put(" ".join(linhas_px))
    return imagem


def imagem_foto(tipo, uso):
    """Foto ("foto") ou foto tratada ("tratada") gerada pelo gerar_imagens.py, no tamanho do uso
    ("aba" ou "abertura"). None se o arquivo nao existir"""
    try:
        return tk.PhotoImage(file=str(PASTA_IMAGENS / f"fuego_{tipo}_{uso}.png"))
    except tk.TclError:
        return None


def melhor_escala(maximo):
    # Maior fracao zoom/subsample (denominador ate 4) ate o maximo: o PhotoImage so escala assim
    return max(((int(maximo * q), q) for q in range(1, 5) if int(maximo * q) >= 1),
               key=lambda e: e[0] / e[1], default=(1, 2))


def escalar(imagem, zoom, reducao):
    imagem = imagem.zoom(zoom) if zoom > 1 else imagem
    return imagem.subsample(reducao) if reducao > 1 else imagem


#==========================================//Abertura//=========================================//

class Abertura:
    """Introducao na propria janela da ferramenta: uma camada escura por cima das abas.

    Estilos: "ascii_fuego" (a arte do main.cpp) ou o robo ("ascii_robo", "foto", "tratada") com o
    nome "Fuego" da arte do main.cpp embaixo. O texto surge e some misturando a cor dele com a do
    fundo, e a imagem com um veu da cor do fundo semitransparente por cima (a janela nao fica
    transparente). Depois a tela fica vazia por ABERTURA_VAZIO_MS e a camada sai. Tudo cabe em
    ABERTURA_DURACAO_MS. Um clique ou ESC pula para o sumir"""

    def __init__(self, raiz, degrade, estilo="ascii_fuego", ao_terminar=None):
        self.raiz = raiz
        self.ao_terminar = ao_terminar
        self.sumindo = False
        self.espera = None

        self.tela = tk.Canvas(raiz, background=ABERTURA_FUNDO, highlightthickness=0, borderwidth=0,
                              cursor="arrow")
        self.tela.place(x=0, y=0, relwidth=1, relheight=1)
        tk.Misc.lift(self.tela)               # No Canvas, lift() sobe itens do desenho, nao o widget

        arte = arte_fuego()
        self.imagem = None
        if estilo == "ascii_robo":
            base = imagem_ascii_robo(linhas_ascii_robo(), "noturno", ABERTURA_FUNDO)
            self.imagem = escalar(base, 5, 4) if base else None
        elif estilo in ("foto", "tratada"):
            self.imagem = imagem_foto(estilo, "abertura")
        if estilo != "ascii_fuego":
            blocos = blocos_arte(arte)
            arte = blocos[-1] if blocos else arte   # So o nome "Fuego"
            recuo = min(len(l) - len(l.lstrip(" ⠀")) for l in arte)
            arte = [l[recuo:] for l in arte]  # Centraliza pelo proprio nome, sem o recuo do desenho

        fonte = fonte_arte(raiz)
        altura_linha = fonte.metrics("linespace")
        largura_texto = max(len(l) for l in arte) * fonte.measure("#")
        altura_imagem = self.imagem.height() + ABERTURA_ESPACO if self.imagem else 0
        altura_total = altura_imagem + len(arte) * altura_linha

        # Posicoes relativas ao centro da tela, aplicadas em <Configure>
        self.itens = []                       # (item, dx, dy)
        topo = -altura_total // 2
        if self.imagem:
            item = self.tela.create_image(0, 0, image=self.imagem, anchor="n")
            self.itens.append((item, 0, topo))
            self.veu = None
            self.veu_item = self.tela.create_image(0, 0, anchor="n")
            self.itens.append((self.veu_item, 0, topo))
        self.textos = []                      # (item, cor final)
        for i, linha in enumerate(arte):
            cor = cor_degrade(i / max(1, len(arte) - 1)) if degrade else ABERTURA_COR
            item = self.tela.create_text(0, 0, text=linha, font=fonte, fill=ABERTURA_FUNDO, anchor="nw")
            self.itens.append((item, -largura_texto // 2, topo + altura_imagem + i * altura_linha))
            self.textos.append((item, cor))
        self.tela.bind("<Configure>", lambda e: self.posicionar(e.width, e.height))

        self.tela.bind("<Button-1>", lambda _: self.pular())
        raiz.bind("<Escape>", lambda _: self.pular())

        self.pintar(0.0)
        self.transicao = int(ABERTURA_INTERVALO_MS / ABERTURA_PASSO) + ABERTURA_INTERVALO_MS
        self.inicio = time.monotonic()
        self.raiz.after(ABERTURA_INTERVALO_MS, self.surgir, 0.0)

    def posicionar(self, largura, altura):
        for item, dx, dy in self.itens:
            self.tela.coords(item, largura // 2 + dx, altura // 2 + dy)

    def pintar(self, alfa):
        for item, cor in self.textos:
            self.tela.itemconfig(item, fill=misturar(ABERTURA_FUNDO, cor, alfa))
        if self.imagem:
            # Veu da cor do fundo com opacidade (1 - alfa) por cima da imagem
            ponto = tk.PhotoImage(width=1, height=1)
            ponto.put(f"{ABERTURA_FUNDO}{round((1 - alfa) * 255):02x}")
            self.veu = ponto.zoom(self.imagem.width(), self.imagem.height())
            self.tela.itemconfig(self.veu_item, image=self.veu)

    def surgir(self, alfa):
        if self.sumindo:
            return
        alfa = min(1.0, alfa + ABERTURA_PASSO)
        self.pintar(alfa)
        if alfa < 1.0:
            self.raiz.after(ABERTURA_INTERVALO_MS, self.surgir, alfa)
            return
        # Pelo relogio, e nao pela soma dos passos: os atrasos do after nao alongam a abertura
        decorrido = int((time.monotonic() - self.inicio) * 1000)
        exibicao = ABERTURA_DURACAO_MS - decorrido - self.transicao - ABERTURA_VAZIO_MS
        self.espera = self.raiz.after(max(0, exibicao), self.sumir, 1.0)

    def pular(self):
        if self.sumindo:
            return
        if self.espera:
            self.raiz.after_cancel(self.espera)
        self.sumir(1.0)

    def sumir(self, alfa):
        self.sumindo = True
        alfa = max(0.0, alfa - ABERTURA_PASSO)
        self.pintar(alfa)
        if alfa > 0.0:
            self.raiz.after(ABERTURA_INTERVALO_MS, self.sumir, alfa)
            return
        self.raiz.after(ABERTURA_VAZIO_MS, self.encerrar)   # Tela vazia antes da ferramenta

    def encerrar(self):
        self.raiz.unbind("<Escape>")
        self.tela.destroy()
        if self.ao_terminar:
            self.ao_terminar()


def main():
    raiz = tk.Tk()
    preferencias = carregar_preferencias()
    try:
        App(raiz, preferencias)               # Montada por baixo da abertura
    except Exception as erro:                 # Aberta pelo pythonw: sem console para mostrar o erro
        messagebox.showerror(NOME_FERRAMENTA, f"Erro ao abrir a interface:\n{erro}")
        raise
    if preferencias["abertura_animada"]:
        Abertura(raiz, preferencias["abertura_degrade"], preferencias["abertura_estilo"])
    raiz.mainloop()


if __name__ == "__main__":
    main()
