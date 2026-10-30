"""
Le e grava o MAC do "mestre" de um controle DualShock 4 conectado pela USB, como o SixaxisPairTool.

O controle so se conecta por Bluetooth ao aparelho cujo MAC esta gravado nele (o mestre). No Fuego, o
PS4.begin(FUEGO) (include/rc_mode.hpp) faz o ESP32 usar esse MAC no Bluetooth, entao o mestre do
controle tem que ser igual ao FUEGO do codigo.

Pela USB, o DS4 responde a dois relatorios de configuracao (feature reports):
  0x12 (leitura): bytes 1-6 = MAC do controle, bytes 10-15 = MAC do mestre (os dois invertidos)
  0x13 (escrita): bytes 1-6 = MAC do mestre novo (invertido) e bytes 7-22 = chave de pareamento

Usa o hid.dll e o setupapi.dll do Windows (ctypes): nenhuma biblioteca para instalar. Se o
SixaxisPairTool ja trocou o driver do controle pelo libusb-win32, o controle some dos HIDs, e os
mesmos relatorios vao pelo libusb0.dll (copia em tools/lib/libusb0) como transferencias de
controle USB, do mesmo jeito que o SixaxisPairTool faz.

Uso pelo terminal:
  python tools/ps4_mac.py                       (mostra o controle e o mestre atual)
  python tools/ps4_mac.py c8:85:41:2c:1e:90     (grava o mestre novo)
"""

import ctypes
import re
import sys
from ctypes import wintypes
from pathlib import Path

VID_SONY = 0x054C
MODELOS = {0x05C4: "DualShock 4 (1a geracao, CUH-ZCT1)", 0x09CC: "DualShock 4 (2a geracao, CUH-ZCT2)"}
RELATORIO_LER = 0x12
RELATORIO_GRAVAR = 0x13
TAMANHO_GRAVAR = 23                           # ID + 6 do MAC + 16 da chave
# Chave de pareamento gravada junto com o mestre (a mesma das ferramentas livres de pareamento do DS4)
CHAVE_PAREAMENTO = bytes.fromhex("56E8813808065141C07F12AAD9663CCE")

ARQUIVO_MAC = Path(__file__).resolve().parent.parent / "include" / "rc_mode.hpp"
PADRAO_MAC = re.compile(r'const\s+char\s*\*\s*FUEGO\s*=\s*"([0-9A-Fa-f:]{17})"')
PADRAO_MAC_TEXTO = re.compile(r"^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$")


class ErroPS4(Exception):
    """Falha ao falar com o controle, com a mensagem para o usuario"""


#=========================================//Windows HID//=======================================//

class GUID(ctypes.Structure):
    _fields_ = [("Data1", wintypes.DWORD), ("Data2", wintypes.WORD), ("Data3", wintypes.WORD),
                ("Data4", ctypes.c_ubyte * 8)]


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.DWORD), ("InterfaceClassGuid", GUID), ("Flags", wintypes.DWORD),
                ("Reserved", ctypes.c_size_t)]


class HIDD_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("Size", wintypes.ULONG), ("VendorID", wintypes.USHORT), ("ProductID", wintypes.USHORT),
                ("VersionNumber", wintypes.USHORT)]


class HIDP_CAPS(ctypes.Structure):
    _fields_ = [("Usage", wintypes.USHORT), ("UsagePage", wintypes.USHORT),
                ("InputReportByteLength", wintypes.USHORT), ("OutputReportByteLength", wintypes.USHORT),
                ("FeatureReportByteLength", wintypes.USHORT), ("Reserved", wintypes.USHORT * 17)] + \
               [(nome, wintypes.USHORT) for nome in (
                   "NumberLinkCollectionNodes", "NumberInputButtonCaps", "NumberInputValueCaps",
                   "NumberInputDataIndices", "NumberOutputButtonCaps", "NumberOutputValueCaps",
                   "NumberOutputDataIndices", "NumberFeatureButtonCaps", "NumberFeatureValueCaps",
                   "NumberFeatureDataIndices")]


DIGCF_PRESENT = 0x02
DIGCF_DEVICEINTERFACE = 0x10
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x01
FILE_SHARE_WRITE = 0x02
OPEN_EXISTING = 3
INVALID_HANDLE = ctypes.c_void_p(-1).value
BLUETOOTH_HID = "{00001124-0000-1000-8000-00805f9b34fb}"   # Caminho de um HID conectado por Bluetooth


def _bibliotecas():
    if sys.platform != "win32":
        raise ErroPS4("A leitura do controle so funciona no Windows")
    hid = ctypes.WinDLL("hid")
    setupapi = ctypes.WinDLL("setupapi")
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    setupapi.SetupDiGetClassDevsW.restype = ctypes.c_void_p
    setupapi.SetupDiGetClassDevsW.argtypes = [ctypes.POINTER(GUID), wintypes.LPCWSTR, wintypes.HWND, wintypes.DWORD]
    setupapi.SetupDiEnumDeviceInterfaces.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(GUID),
                                                     wintypes.DWORD, ctypes.POINTER(SP_DEVICE_INTERFACE_DATA)]
    setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [ctypes.c_void_p, ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
                                                          ctypes.c_void_p, wintypes.DWORD,
                                                          ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    setupapi.SetupDiDestroyDeviceInfoList.argtypes = [ctypes.c_void_p]
    kernel32.CreateFileW.restype = ctypes.c_void_p
    kernel32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                     wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    hid.HidD_GetHidGuid.argtypes = [ctypes.POINTER(GUID)]
    hid.HidD_GetAttributes.argtypes = [ctypes.c_void_p, ctypes.POINTER(HIDD_ATTRIBUTES)]
    hid.HidD_GetFeature.argtypes = [ctypes.c_void_p, ctypes.c_void_p, wintypes.ULONG]
    hid.HidD_SetFeature.argtypes = [ctypes.c_void_p, ctypes.c_void_p, wintypes.ULONG]
    hid.HidD_GetPreparsedData.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    hid.HidD_FreePreparsedData.argtypes = [ctypes.c_void_p]
    hid.HidP_GetCaps.argtypes = [ctypes.c_void_p, ctypes.POINTER(HIDP_CAPS)]
    return hid, setupapi, kernel32


def _caminhos_hid(hid, setupapi):
    guid = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(guid))
    lista = setupapi.SetupDiGetClassDevsW(ctypes.byref(guid), None, None, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if not lista or lista == INVALID_HANDLE:
        return []
    caminhos = []
    try:
        indice = 0
        while True:
            dados = SP_DEVICE_INTERFACE_DATA()
            dados.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
            if not setupapi.SetupDiEnumDeviceInterfaces(lista, None, ctypes.byref(guid), indice, ctypes.byref(dados)):
                break
            indice += 1
            necessario = wintypes.DWORD()
            setupapi.SetupDiGetDeviceInterfaceDetailW(lista, ctypes.byref(dados), None, 0, ctypes.byref(necessario), None)
            detalhe = ctypes.create_string_buffer(necessario.value)
            # cbSize da estrutura de detalhe: DWORD + 1 WCHAR, com o alinhamento da plataforma
            ctypes.c_uint32.from_buffer(detalhe).value = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            if setupapi.SetupDiGetDeviceInterfaceDetailW(lista, ctypes.byref(dados), detalhe, necessario, None, None):
                caminhos.append(ctypes.wstring_at(ctypes.addressof(detalhe) + 4))
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(lista)
    return caminhos


def _formatar(mac_invertido):
    return ":".join(f"{b:02x}" for b in reversed(bytes(mac_invertido)))


class Controle:
    """DS4 aberto pela USB. Use com 'with'"""

    def __init__(self, hid, kernel32, handle, modelo, caminho):
        self.hid, self.kernel32, self.handle = hid, kernel32, handle
        self.modelo, self.caminho = modelo, caminho
        dados = ctypes.c_void_p()
        caps = HIDP_CAPS()
        if hid.HidD_GetPreparsedData(handle, ctypes.byref(dados)):
            hid.HidP_GetCaps(dados, ctypes.byref(caps))
            hid.HidD_FreePreparsedData(dados)
        self.tamanho_feature = max(caps.FeatureReportByteLength, 64)

    def __enter__(self):
        return self

    def __exit__(self, *erro):
        self.kernel32.CloseHandle(self.handle)

    def ler(self):
        """Devolve (mac_do_controle, mac_do_mestre) como 'aa:bb:cc:dd:ee:ff'"""
        buffer = (ctypes.c_ubyte * self.tamanho_feature)()
        buffer[0] = RELATORIO_LER
        if not self.hid.HidD_GetFeature(self.handle, buffer, self.tamanho_feature):
            raise ErroPS4("O controle nao respondeu a leitura (relatorio 0x12). Ele esta conectado pela USB?")
        return _formatar(buffer[1:7]), _formatar(buffer[10:16])

    def gravar_mestre(self, mac):
        mac_bytes = bytes(int(parte, 16) for parte in mac.split(":"))
        relatorio = bytes([RELATORIO_GRAVAR]) + mac_bytes[::-1] + CHAVE_PAREAMENTO
        # O Windows aceita o tamanho exato do relatorio ou o maior relatorio do aparelho, conforme o driver
        for tamanho in (TAMANHO_GRAVAR, self.tamanho_feature):
            buffer = (ctypes.c_ubyte * tamanho)(*relatorio)
            if self.hid.HidD_SetFeature(self.handle, buffer, tamanho):
                return
        raise ErroPS4("O controle recusou a gravacao (relatorio 0x13)")


def abrir_controle():
    """Abre o primeiro DS4 conectado pela USB"""
    hid, setupapi, kernel32 = _bibliotecas()
    pelo_bluetooth = False
    bloqueado = False
    for caminho in _caminhos_hid(hid, setupapi):
        if "054c" not in caminho.lower():    # VID da Sony no caminho (USB: vid_054c, Bluetooth: ...054c_pid)
            continue
        handle = kernel32.CreateFileW(caminho, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      None, OPEN_EXISTING, 0, None)
        if not handle or handle == INVALID_HANDLE:
            bloqueado = True
            continue
        atributos = HIDD_ATTRIBUTES()
        atributos.Size = ctypes.sizeof(HIDD_ATTRIBUTES)
        hid.HidD_GetAttributes(handle, ctypes.byref(atributos))
        if atributos.VendorID == VID_SONY and atributos.ProductID in MODELOS:
            if BLUETOOTH_HID in caminho.lower():
                pelo_bluetooth = True
            else:
                return Controle(hid, kernel32, handle, MODELOS[atributos.ProductID], caminho)
        kernel32.CloseHandle(handle)

    # Sem HID: o SixaxisPairTool pode ter trocado o driver do controle pelo libusb-win32
    controle = abrir_controle_libusb()
    if controle:
        return controle

    if pelo_bluetooth:
        raise ErroPS4("O controle esta conectado por Bluetooth. Conecte-o pelo cabo USB")
    if bloqueado:
        raise ErroPS4("Achei um aparelho da Sony, mas outro programa esta com ele aberto "
                      "(DS4Windows, Steam?). Feche o programa e tente de novo")
    raise ErroPS4("Nenhum DualShock 4 encontrado na USB. Confira o cabo (alguns so carregam, sem dados)")


#====================================//libusb-win32 (Sixaxis)//==================================//

# O SixaxisPairTool instala o driver libusb-win32 nas interfaces do controle. Com ele, o controle
# some da lista de HIDs, e os mesmos relatorios vao por transferencias de controle USB da classe HID
# A copia de tools/lib (da arquitetura do Python) primeiro, depois a do Windows
LIBUSB_DLLS = [Path(__file__).resolve().parent / "lib" / "libusb0" /
               ("x64" if ctypes.sizeof(ctypes.c_void_p) == 8 else "x86") / "libusb0.dll", "libusb0.dll"]
LIBUSB_PATH_MAX = 512
HID_INTERFACE = 3                             # Interface HID do DS4 (MI_03)
HID_GET_REPORT = 0x01
HID_SET_REPORT = 0x09
HID_TIPO_FEATURE = 0x03                       # Byte alto do wValue
USB_PARA_HOST = 0xA1                          # Classe, interface, do aparelho para o PC
USB_PARA_APARELHO = 0x21
TAMANHO_LER = 16                              # Relatorio 0x12
USB_TIMEOUT_MS = 1000


class usb_device_descriptor(ctypes.Structure):
    _fields_ = [("bLength", ctypes.c_ubyte), ("bDescriptorType", ctypes.c_ubyte), ("bcdUSB", ctypes.c_ushort),
                ("bDeviceClass", ctypes.c_ubyte), ("bDeviceSubClass", ctypes.c_ubyte),
                ("bDeviceProtocol", ctypes.c_ubyte), ("bMaxPacketSize0", ctypes.c_ubyte),
                ("idVendor", ctypes.c_ushort), ("idProduct", ctypes.c_ushort), ("bcdDevice", ctypes.c_ushort),
                ("iManufacturer", ctypes.c_ubyte), ("iProduct", ctypes.c_ubyte),
                ("iSerialNumber", ctypes.c_ubyte), ("bNumConfigurations", ctypes.c_ubyte)]


class usb_device(ctypes.Structure):
    pass


class usb_bus(ctypes.Structure):
    pass


usb_device._fields_ = [("next", ctypes.POINTER(usb_device)), ("prev", ctypes.POINTER(usb_device)),
                       ("filename", ctypes.c_char * LIBUSB_PATH_MAX), ("bus", ctypes.POINTER(usb_bus)),
                       ("descriptor", usb_device_descriptor), ("config", ctypes.c_void_p), ("dev", ctypes.c_void_p),
                       ("devnum", ctypes.c_ubyte), ("num_children", ctypes.c_ubyte), ("children", ctypes.c_void_p)]
usb_bus._fields_ = [("next", ctypes.POINTER(usb_bus)), ("prev", ctypes.POINTER(usb_bus)),
                    ("dirname", ctypes.c_char * LIBUSB_PATH_MAX), ("devices", ctypes.POINTER(usb_device)),
                    ("location", ctypes.c_ulong), ("root_dev", ctypes.POINTER(usb_device))]


class ControleLibusb:
    """DS4 com o driver libusb-win32. Mesma interface do Controle (HID)"""

    def __init__(self, usb, handle, modelo):
        self.usb, self.handle, self.modelo = usb, handle, modelo
        self.caminho = "libusb-win32"

    def __enter__(self):
        return self

    def __exit__(self, *erro):
        self.usb.usb_close(self.handle)

    def _ler_relatorio(self):
        buffer = ctypes.create_string_buffer(TAMANHO_LER)
        lidos = self.usb.usb_control_msg(self.handle, USB_PARA_HOST, HID_GET_REPORT,
                                         (HID_TIPO_FEATURE << 8) | RELATORIO_LER, HID_INTERFACE,
                                         buffer, TAMANHO_LER, USB_TIMEOUT_MS)
        return buffer.raw[:lidos] if lidos >= TAMANHO_LER else None

    def ler(self):
        dados = self._ler_relatorio()
        if not dados:
            raise ErroPS4("O controle nao respondeu a leitura pelo libusb (relatorio 0x12)")
        return _formatar(dados[1:7]), _formatar(dados[10:16])

    def gravar_mestre(self, mac):
        mac_bytes = bytes(int(parte, 16) for parte in mac.split(":"))
        relatorio = bytes([RELATORIO_GRAVAR]) + mac_bytes[::-1] + CHAVE_PAREAMENTO
        buffer = ctypes.create_string_buffer(relatorio, len(relatorio))
        enviados = self.usb.usb_control_msg(self.handle, USB_PARA_APARELHO, HID_SET_REPORT,
                                            (HID_TIPO_FEATURE << 8) | RELATORIO_GRAVAR, HID_INTERFACE,
                                            buffer, len(relatorio), USB_TIMEOUT_MS)
        if enviados != len(relatorio):
            raise ErroPS4("O controle recusou a gravacao pelo libusb (relatorio 0x13)")


def abrir_controle_libusb():
    """Primeiro DS4 visivel pelo libusb-win32 que responda ao relatorio 0x12, ou None"""
    usb = None
    for dll in LIBUSB_DLLS:
        try:
            usb = ctypes.CDLL(str(dll))
            break
        except OSError:
            continue
    if not usb:
        return None
    usb.usb_get_busses.restype = ctypes.POINTER(usb_bus)
    usb.usb_open.restype = ctypes.c_void_p
    usb.usb_open.argtypes = [ctypes.POINTER(usb_device)]
    usb.usb_close.argtypes = [ctypes.c_void_p]
    usb.usb_control_msg.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                    ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    usb.usb_init()
    usb.usb_find_busses()
    usb.usb_find_devices()

    barramento = usb.usb_get_busses()
    while barramento:
        aparelho = barramento.contents.devices
        while aparelho:
            descritor = aparelho.contents.descriptor
            if descritor.idVendor == VID_SONY and descritor.idProduct in MODELOS:
                # Cada interface com o driver aparece como um aparelho: fica o que responder
                handle = usb.usb_open(aparelho)
                if handle:
                    controle = ControleLibusb(usb, handle, MODELOS[descritor.idProduct] + " (driver libusb)")
                    if controle._ler_relatorio():
                        return controle
                    usb.usb_close(handle)
            aparelho = aparelho.contents.next
        barramento = barramento.contents.next
    return None


#==========================================//Codigo//===========================================//

def mac_valido(mac):
    return bool(PADRAO_MAC_TEXTO.match(mac.strip()))


def mac_do_codigo():
    """(mac, arquivo, linha) do FUEGO em rc_mode.hpp, ou (None, arquivo, None)"""
    try:
        linhas = ARQUIVO_MAC.read_text(encoding="utf-8").splitlines()
    except OSError:
        return None, ARQUIVO_MAC, None
    for numero, linha in enumerate(linhas, 1):
        achado = PADRAO_MAC.search(linha)
        if achado:
            return achado.group(1).lower(), ARQUIVO_MAC, numero
    return None, ARQUIVO_MAC, None


def main():
    try:
        with abrir_controle() as controle:
            proprio, mestre = controle.ler()
            print(f"{controle.modelo}\n  MAC do controle: {proprio}\n  Mestre atual:    {mestre}")
            if len(sys.argv) > 1:
                novo = sys.argv[1].lower()
                if not mac_valido(novo):
                    sys.exit(f"MAC invalido: {novo} (use aa:bb:cc:dd:ee:ff)")
                controle.gravar_mestre(novo)
                print(f"  Mestre gravado:  {controle.ler()[1]}")
    except ErroPS4 as erro:
        sys.exit(str(erro))


if __name__ == "__main__":
    main()
