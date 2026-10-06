//===============================================================================================//
//===========================================//VSCODE//==========================================//
//===============================================================================================//

# Mostra o status dos arquivos
git status

# Adiciona os arquivos modificados
git add .

# Cria o commit com uma mensagem descritiva
git commit -m "(comentario)"

# Envia o commit para o GitHub
git push

# Baixa o commit do GitHub
git pull origin main

//===============================================================================================//
//=========================================//PLATFORMIO//========================================//
//===============================================================================================//

# Compila o código
pio run

# Limpa a memória flash
pio run --target erase

# faz upload do código
pio run --target upload

# abre o serial monitor
pio device monitor --baud 115200

# Compila, envia e depois abre o serial monitor
pio run -t upload -t monitor