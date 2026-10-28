# Servidor de Lobby & UDP Relay - CTR Turbocharged

Servidor dedicado para o modo **Multiplayer Online** do Crash Team Racing: Turbocharged, permitindo partidas pela internet sem VPN, sem Hamachi e sem configuração de arquivos pelos clientes.

---

## 1. Pré-Requisitos

- **Windows 10 / 11** ou **Linux (Ubuntu/Debian)**
- **Node.js v18+** instalado (o seu sistema já possui `v24.13.1`). Se necessário instalar em outro computador, baixe em [nodejs.org](https://nodejs.org).

---

## 2. Como Iniciar no seu PC (Desenvolvimento Local / Mesma Rede)

1. Basta dar 2 cliques em `INICIAR_SERVIDOR.bat`.
2. O servidor iniciará automaticamente:
   - **Lobby API (HTTP)**: `http://0.0.0.0:8080`
   - **Gameplay Relay (UDP)**: `0.0.0.0:31850`
3. Para parar o servidor, feche a janela ou execute `PARAR_SERVIDOR.bat`.

---

## 3. Como Jogar com Amigos pela Internet (Sem abrir portas / Sem VPN)

Para que amigos de fora da sua casa acessem o servidor no seu PC, use um agente de túnel gratuito que encaminha tráfego **HTTP e UDP**. A melhor solução gratuita do mercado para jogos é o **Playit.gg**:

### Passo a passo com Playit.gg:
1. Baixe o executável leve do [playit.gg/download](https://playit.gg/download) (apenas você baixa no PC do Host, seus amigos não baixam nada).
2. Execute o Playit e crie 2 túneis gratuitos na sua conta:
   - **Túnel 1 (HTTP Lobby)**: Redireciona para `127.0.0.1:8080` (TCP/HTTP).
   - **Túnel 2 (UDP Gameplay Relay)**: Redireciona para `127.0.0.1:31850` (UDP).
3. O Playit gerará um domínio público, por exemplo:
   - Lobby HTTP: `https://seu-nome.playit.gg`
   - Relay UDP: `seu-nome.playit.gg:12345`
4. No arquivo `.env` do servidor (ou no `online_server.txt` do jogo), aponte para esse endereço público.

---

## 4. Testes Automatizados

O servidor inclui uma suíte completa de testes unitários e de integração validando 10 cenários críticos:
- Geração de códigos e formato de 4 caracteres alfanuméricos (`XK82`, `AB12`).
- Prevenção de colisões de código ativo.
- Criação, listagem e busca de salas.
- Controle rigoroso de capacidade (máximo 4 jogadores).
- Autorização exclusiva do Host para iniciar a corrida.
- Heartbeat de presença e remoção automática de salas abandonadas.
- Roteamento e isolamento de pacotes UDP entre salas.

Para rodar os testes a qualquer momento:
```bash
cd server
npm test
```

---

## 5. Como Migrar Futuramente para uma VPS (Nuvem)

Quando você quiser deixar o servidor ligado 24/7 em uma VPS (Oracle Cloud Free Tier, DigitalOcean, Hetzner, AWS):
1. No terminal da VPS (Linux Ubuntu):
   ```bash
   sudo apt update && sudo apt install -y nodejs npm
   git clone <seu-repositorio>
   cd "CTR MULTIPLAYER2/server"
   npm install --production
   ```
2. Para manter o servidor rodando em segundo plano mesmo se você desconectar o SSH, use o **PM2**:
   ```bash
   sudo npm install -g pm2
   pm2 start src/server.js --name "ctr-server"
   pm2 startup
   pm2 save
   ```
3. O servidor estará rodando no IP fixo da VPS com latência mínima!
