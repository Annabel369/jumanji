![Uploading Captura de tela de 2026-08-10 19-59-49.png…]()
# Jumanji DOS (SVGA Edition) 🎲🌲

Um jogo de tabuleiro clássico e retro construído em **C++ de 16-bits** para o sistema operacional **MS-DOS / FreeDOS**, rodando em alta resolução gráfica de **800x600 pixels (256 cores)** através do padrão **VESA VBE (SVGA)**. 

O projeto foi projetado para rodar com desempenho ideal em emuladores como o **DOSBox** ou hardware retro real, contendo animações de tokens por bank-switching, efeitos de áudio no PC Speaker e mecânicas dinâmicas de suspense e sobrevivência baseadas no clássico universo de *Jumanji*.

---

## 🎮 Funcionalidades do Jogo

### 1. Torneio Incial de Turnos (Pedra, Papel e Tesoura 1v1)
Ao iniciar, o jogo não decide a ordem de forma aleatória. Os 4 amigos (**Vermelho**, **Azul**, **Verde** e **Amarelo**) disputam suas posições em chaves de mata-mata rápidos:
- **Semifinal 1**: Vermelho vs Azul
- **Semifinal 2**: Verde vs Amarelo
- **Disputa de 3º/4º**: Perdedor da Semifinal 1 vs Perdedor da Semifinal 2
- **Grande Final**: Vencedor da Semifinal 1 vs Vencedor da Semifinal 2 (decide o 1º e 2º lugar)
- As chaves rolam de forma animada e rápida e salvam a ordem exata de turnos no HUD do jogo.

### 2. Tabuleiro Espiral de 60 Casas
O tabuleiro se organiza em formato espiral contendo caminhos numerados em pedra sobre um fundo de madeira realista.
- **Casas Especiais de Desastres**:
  - **Casa 10 (GUERRA MUNDIAL - Vermelha)**: O mundo entra em colapso. **Todos** os jogadores recuam 3 casas.
  - **Casa 30 (PANDEMIA GLOBAL - Verde)**: Quarentena mundial! **Todos** os jogadores perdem a vez na rodada seguinte.
  - **Casa 50 (INVASÃO ALIENÍGENA - Roxa)**: O jogador ativo é abduzido e teletransportado **10 casas para trás**.
- **Casas de Morte Absoluta**:
  - **Casas 20 e 40 (MORTE - Preta)**: Se um jogador cair aqui (ou for empurrado por desastres/cartas), ele **morre permanentemente**. Seu token fica preto no mapa, seu HUD marca "MORTO" e sua vez é pulada.
  - **Derrota Geral**: Se todos os 4 jogadores morrerem na selva, a tela de "FIM DE JOGO" é disparada.

### 3. Mecânica do Portal de Retorno (Ressuscitação)
Se o deck de cartas místicas sortear a carta **"PORTAL RETORNO!"**:
- O jogo resgata o último jogador que morreu na selva.
- O portal revive o jogador (`p_dead = 0`), restaura o token colorido na casa onde morreu e o coloca de volta na disputa, gerando suspense e esperança no tabuleiro.

### 4. Registro de Histórico de Vencedores (`INFO.LOG`)
Toda vez que um jogador vence cruzando a linha de chegada (Casa 59), o jogo salva instantaneamente o resultado no arquivo **`INFO.LOG`** no diretório do executável, reescrevendo-o para armazenar sempre a cor do último vencedor (ex: `VERMELHO`, `AZUL`, `VERDE` ou `AMARELO`).

---

## 🛠️ Detalhes Técnicos e Otimizações

- **Modo de Vídeo**: Inicializado no VESA VBE `Mode 103h` (800x600, 256 cores).
- **Otimização de VRAM**: Funções de preenchimento (`draw_circle`) reescritas para usar scanlines horizontais (`draw_hline`) diretas na memória de vídeo mapeada em `0xA000`, evitando chamadas caras e lags na renderização de tela.
- **Refresh Localizado**: Textos da bola de cristal central e rolagem do pedra-papel-tesoura utilizam redesenhos parciais em vez de atualizar todo o frame de madeira do fundo, garantindo atualização em 1ms.
- **Áudio retro**: Áudio via portas do PC Speaker executando frequências personalizadas para rolagens, desastres e vitórias.

---

## 🚀 Como Compilar e Rodar

### Pré-requisitos
- **DOSBox** (ou outro emulador x86 compatível com SVGA) instalado.
- Compilador **Borland C++ 3.1** (ou Turbo C++) configurado no ambiente DOS.

### Compilação
Dentro da pasta do projeto (`/game/jumanji`), você encontrará o script utilitário `compile.bat`.
1. Monte o diretório no DOSBox:
   ```bash
   mount c "/caminho/do/projeto"
   c:
   cd game\jumanji
   ```
2. Execute o script de compilação:
   ```cmd
   compile.bat
   ```
   Isso invocará o compilador do Borland C++ (`bcc.exe`) e linkador (`tlink.exe`) para gerar o executável final `JUMANJI.EXE` sem erros.

### Executando
Para iniciar o jogo diretamente no DOSBox, digite:
```cmd
JUMANJI.EXE
```

### Controles
- **ESPAÇO**: Roda os dados no seu turno.
- **ENTER / ESPAÇO**: Passa pelas telas de confronto no torneio e confirma mensagens na Bola de Cristal.
- **ESC**: Encerra o jogo e volta para o prompt do DOS.

---

## 📂 Estrutura de Arquivos do Projeto

- **`jumanji.cpp`**: Código-fonte principal contendo a engine de vídeo VESA, rotinas matemáticas de círculos/linhas, IA de pedra-papel-tesoura e loop de jogo.
- **`compile.bat`**: Script DOS para compilação rápida via linha de comando do Borland C++.
- **`COMPILE.LOG`**: Log de saída do compilador contendo o status da última montagem do executável.
- **`INFO.LOG`**: Arquivo gerado automaticamente contendo o nome da última cor vencedora.
- **`README.md`**: Guia completo de documentação do projeto.
