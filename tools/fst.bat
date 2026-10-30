@echo off
rem Abre o Fuego Sumo Tool (FST). As bibliotecas ficam em tools\lib: so precisa do Python
setlocal
cd /d "%~dp0"

set "PY="
set "PYW="
where py >nul 2>nul && (set "PY=py -3" & set "PYW=pyw -3")
if not defined PY where python >nul 2>nul && (set "PY=python" & set "PYW=pythonw")
if not defined PY (
    echo Python nao encontrado. Instale o Python 3 em https://www.python.org
    echo e marque a opcao "Add python.exe to PATH" na instalacao.
    pause
    exit /b 1
)

%PY% -c "import tkinter" 2>nul || (
    echo Este Python nao tem o tkinter. Reinstale o Python marcando "tcl/tk and IDLE".
    pause
    exit /b 1
)

start "" %PYW% "%~dp0fst.py"
