# Bibliotecas do FST

Cópias das bibliotecas de terceiros que o FST usa, para ele não depender de nada instalado fora da pasta `tools`. Os scripts carregam estas cópias antes de qualquer versão instalada no PC.

| Pasta | Biblioteca | Versão | Licença | Uso |
|---|---|---|---|---|
| `serial/` | [pyserial](https://github.com/pyserial/pyserial) | 3.5 | BSD 3 cláusulas (`serial/LICENSE.txt`) | Porta serial da coleta e da aba NVS |
| `libusb0/x64`, `libusb0/x86` | [libusb-win32](https://github.com/mcuee/libusb-win32) (`libusb0.dll`) | 1.2.6.0 | LGPL 2.1 (`libusb0/LICENSE.txt`) | Controle PS4 com o driver instalado pelo SixaxisPairTool |

O `libusb0.dll` precisa ser da mesma versão do driver `libusb0.sys` instalado no controle. O SixaxisPairTool instala a 1.2.6.0. Os dois arquivos foram copiados sem alteração.

Para atualizar o pyserial, copie a pasta `serial` de uma instalação nova (`pip show -f pyserial`) por cima desta.
