"""
Gera as imagens do robo usadas pela interface (aba Fuego e abertura) a partir de
tools/imagens/fuego_original.png. Rode de novo se trocar a imagem original:

  python tools/gerar_imagens.py

Saidas em tools/imagens/:
  fuego_foto_aba.png         foto reduzida para a aba Fuego
  fuego_foto_abertura.png    foto menor, para caber com o nome "Fuego" na abertura
  fuego_tratada_aba.png      tratamento: fundo cinza removido (transparente) e robo em tons de fogo
  fuego_tratada_abertura.png

So usa o tkinter (Tk 8.6+ le e grava PNG), sem bibliotecas de imagem.
"""

import tkinter as tk
from pathlib import Path

PASTA = Path(__file__).resolve().parent / "imagens"
ORIGINAL = PASTA / "fuego_original.png"

# Escalas como fracao (zoom, reducao): a reducao faz a media de blocos, sem serrilhado
ESCALA_ABA = (3, 4)
ESCALA_ABERTURA = (1, 2)

# Fundo do render: cinza sem cor. Pixel com pouca saturacao nessa faixa de brilho vira transparente
FUNDO_SATURACAO_MAX = 18
FUNDO_BRILHO_MIN = 62
FUNDO_BRILHO_MAX = 128

# Tons de fogo do tratamento, do escuro ao claro (pontos do degrade)
DEGRADE_FOGO = [(0.0, (70, 8, 8)), (0.35, (200, 25, 25)), (0.7, (255, 120, 40)), (1.0, (255, 235, 190))]


def ler_pixels(imagem):
    linhas = imagem.data()                    # Uma string "#rrggbb #rrggbb ..." por linha
    return [[(int(p[1:3], 16), int(p[3:5], 16), int(p[5:7], 16)) for p in linha.split()] for linha in linhas]


def reduzir(pixels, zoom, reducao):
    """Escala por zoom/reducao com media de area (cada pixel de saida e a media do bloco)"""
    altura, largura = len(pixels), len(pixels[0])
    nova_l, nova_a = largura * zoom // reducao, altura * zoom // reducao
    saida = []
    for y in range(nova_a):
        linha = []
        y0, y1 = y * reducao // zoom, max(y * reducao // zoom + 1, (y + 1) * reducao // zoom)
        for x in range(nova_l):
            x0, x1 = x * reducao // zoom, max(x * reducao // zoom + 1, (x + 1) * reducao // zoom)
            soma = [0, 0, 0, 0]
            for yy in range(y0, min(y1, altura)):
                for xx in range(x0, min(x1, largura)):
                    p = pixels[yy][xx]
                    soma[0] += p[0]; soma[1] += p[1]; soma[2] += p[2]
                    soma[3] += 1
            n = soma[3]
            linha.append((soma[0] // n, soma[1] // n, soma[2] // n))
        saida.append(linha)
    return saida


def eh_fundo(p):
    maior, menor = max(p), min(p)
    return maior - menor <= FUNDO_SATURACAO_MAX and FUNDO_BRILHO_MIN <= (maior + menor) / 2 <= FUNDO_BRILHO_MAX


def cor_fogo(brilho):
    for (a, ca), (b, cb) in zip(DEGRADE_FOGO, DEGRADE_FOGO[1:]):
        if brilho <= b:
            f = (brilho - a) / (b - a)
            return tuple(round(x + (y - x) * f) for x, y in zip(ca, cb))
    return DEGRADE_FOGO[-1][1]


def tratar(pixels):
    """Fundo transparente e robo em tons de fogo. Partes ja coloridas (rodas, fio) mantem a cor"""
    saida = []
    for linha in pixels:
        nova = []
        for p in linha:
            if eh_fundo(p):
                nova.append(None)
                continue
            maior, menor = max(p), min(p)
            brilho = (0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]) / 255
            fogo = cor_fogo(brilho)
            if maior - menor > 60:            # Ja tem cor: mistura metade com o fogo
                fogo = tuple((a + b) // 2 for a, b in zip(p, fogo))
            nova.append(fogo)
        saida.append(nova)
    return saida


def gravar(pixels, caminho):
    altura, largura = len(pixels), len(pixels[0])
    imagem = tk.PhotoImage(width=largura, height=altura)
    linhas = []
    for linha in pixels:
        linhas.append("{" + " ".join("#00000000" if p is None else f"#{p[0]:02x}{p[1]:02x}{p[2]:02x}"
                                     for p in linha) + "}")
    imagem.put(" ".join(linhas))
    imagem.write(str(caminho), format="png")
    print(f"{caminho.name}: {largura}x{altura}")


def main():
    raiz = tk.Tk()
    raiz.withdraw()
    original = ler_pixels(tk.PhotoImage(file=str(ORIGINAL)))
    for sufixo, (zoom, reducao) in (("aba", ESCALA_ABA), ("abertura", ESCALA_ABERTURA)):
        foto = reduzir(original, zoom, reducao)
        gravar(foto, PASTA / f"fuego_foto_{sufixo}.png")
        gravar(tratar(foto), PASTA / f"fuego_tratada_{sufixo}.png")
    raiz.destroy()


if __name__ == "__main__":
    main()
