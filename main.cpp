#include <GL/freeglut.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Dimensões da Janela
const int LARGURA_JANELA = 800;
const int ALTURA_JANELA = 600;
int larguraJanela = LARGURA_JANELA;
int alturaJanela = ALTURA_JANELA;

// ---------------------------------------------------------------------------
// Estados do Sistema
// ---------------------------------------------------------------------------
enum EstadoSistema {
  NORMAL,
  PRE_IGNICAO, // Alfa > 70%
  CORROSAO,    // Beta > 80%
  FALHA        // Alfa >= 100% ou Beta >= 100% -> fim de jogo
};

// ---------------------------------------------------------------------------
// Fatores Externos
// ---------------------------------------------------------------------------
struct FatorExterno {
  const char *nome;
  float valor; // valor atual (0.0 - 1.0)
  float valorMin;
  float valorMax;
};

// ---------------------------------------------------------------------------
// Estado Principal do Sistema
// ---------------------------------------------------------------------------
struct EstadoPrincipal {
  // Substâncias
  float alfa; // 0.0 - 100.0 (%)
  float beta; // 0.0 - 100.0 (%)

  // Estado atual
  EstadoSistema estado;

  // Flags de alerta
  bool alertaPreIgnicao; // Alfa > 70%
  bool alertaCorrosao;   // Beta > 80%
  bool sistemaTerminado; // Alfa >= 100% ou Beta >= 100%

  // Fatores externos
  FatorExterno pressaoMangueiras;
  FatorExterno pressaoAtmosferica;
  FatorExterno temperaturaAmbiente;
};

// Instância global do estado
EstadoPrincipal sistema;

// ---------------------------------------------------------------------------
// Inicialização
// ---------------------------------------------------------------------------
void inicializarSistema() {
  sistema.alfa = 50.0f;
  sistema.beta = 50.0f;
  sistema.estado = NORMAL;
  sistema.alertaPreIgnicao = false;
  sistema.alertaCorrosao = false;
  sistema.sistemaTerminado = false;

  sistema.pressaoMangueiras = {"Pressao Mangueiras", 0.5f, 0.1f, 1.0f};
  sistema.pressaoAtmosferica = {"Pressao Atmosferica", 0.5f, 0.2f, 0.9f};
  sistema.temperaturaAmbiente = {"Temperatura Ambiente", 0.5f, 0.1f, 1.0f};
}

// ---------------------------------------------------------------------------
// Lógica de atualização
// ---------------------------------------------------------------------------

float aleatorio(float min, float max) {
  // Gera um número aleatório entre um valor mínimo e máximo
  return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// Atualiza os fatores externos (a cada ~1 segundo).
// Associações:
//   pressaoMangueiras   -> afeta Beta
//   pressaoAtmosferica  -> afeta Alfa e Beta
//   temperaturaAmbiente -> afeta Alfa
//
// Todos os fatores têm o mesmo intervalo [0.0, 1.0] centrado em 0.5,
// para que a pressão média sobre Alfa e Beta seja simétrica e nenhum
// químico tenha vantagem estrutural.
void atualizarFatores() {
  sistema.pressaoMangueiras.valor = aleatorio(0.0f, 1.0f);
  sistema.pressaoAtmosferica.valor = aleatorio(0.0f, 1.0f);
  sistema.temperaturaAmbiente.valor = aleatorio(0.0f, 1.0f);

  // Feedback das substâncias sobre os fatores (simétrico):
  //   Alfa acima de 50% pressiona a temperatura
  //   Beta acima de 50% pressiona as mangueiras
  float desvioAlfa = (sistema.alfa - 50.0f) / 100.0f; // -0.5 a +0.5
  float desvioBeta = (sistema.beta - 50.0f) / 100.0f; // -0.5 a +0.5

  sistema.temperaturaAmbiente.valor += desvioAlfa * 0.3f;
  sistema.pressaoMangueiras.valor += desvioBeta * 0.3f;

  // Garante que os fatores ficam dentro de [0.0, 1.0]
  auto clamp01 = [](float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  };
  sistema.pressaoMangueiras.valor = clamp01(sistema.pressaoMangueiras.valor);
  sistema.pressaoAtmosferica.valor = clamp01(sistema.pressaoAtmosferica.valor);
  sistema.temperaturaAmbiente.valor =
      clamp01(sistema.temperaturaAmbiente.valor);
}

// Atualiza Alfa e Beta mantendo a soma = 100.
//
// Cada fator contribui para o deltaAlfa com o mesmo peso:
//   temperaturaAmbiente -> sobe Alfa  (+)
//   pressaoMangueiras   -> desce Alfa (-) [sobe Beta]
//   pressaoAtmosferica  -> sobe ou desce Alfa consoante o seu valor
//
// Com fatores uniformes em [0,1] e pesos iguais, E[deltaAlfa] = 0,
// ou seja, em média a mistura não deriva para nenhum lado.
void atualizarSubstancias() {
  // Normaliza cada fator para [-1.0, +1.0] (neutro = 0 quando fator = 0.5)
  float dTemp = (sistema.temperaturaAmbiente.valor - 0.5f) * 2.0f;
  float dMang = (sistema.pressaoMangueiras.valor - 0.5f) * 2.0f;
  float dAtm = (sistema.pressaoAtmosferica.valor - 0.5f) * 2.0f;

  // Pesos iguais; magnitude aleatória comum a todos os fatores
  float magnitude = aleatorio(1.0f, 4.0f);

  float deltaAlfa = 0.0f;
  deltaAlfa += dTemp * magnitude; // temperatura sobe Alfa
  deltaAlfa -= dMang * magnitude; // mangueiras sobem Beta (inverso em Alfa)
  deltaAlfa += dAtm * magnitude;  // atmosférica afeta ambos igualmente

  sistema.alfa += deltaAlfa;

  if (sistema.alfa < 0.0f)
    sistema.alfa = 0.0f;
  if (sistema.alfa > 100.0f)
    sistema.alfa = 100.0f;

  sistema.beta = 100.0f - sistema.alfa;
}

// Avalia o estado do sistema com base nos valores atuais
// Com soma = 100: Alfa >= 100 significa Beta = 0, e vice-versa
void avaliarEstado() {
  if (sistema.alfa >= 100.0f || sistema.beta >= 100.0f) {
    sistema.estado = FALHA;
    sistema.sistemaTerminado = true;
    return;
  }

  // Atualiza os alertas
  sistema.alertaPreIgnicao = (sistema.alfa > 70.0f);
  sistema.alertaCorrosao = (sistema.beta > 80.0f);

  // PRE_IGNICAO tem prioridade (Alfa > 70 é mais perigoso a curto prazo)
  if (sistema.alertaPreIgnicao) {
    sistema.estado = PRE_IGNICAO;
  } else if (sistema.alertaCorrosao) {
    sistema.estado = CORROSAO;
  } else {
    sistema.estado = NORMAL;
  }
}

// ---------------------------------------------------------------------------
// Timers GLUT
// ---------------------------------------------------------------------------
void timerSubstancias(int valor) {
  if (sistema.sistemaTerminado)
    return;

  atualizarSubstancias();
  avaliarEstado();

  if (sistema.sistemaTerminado) {
    printf("FALHA CRITICA: sistema terminado.\n");
    glutPostRedisplay();

    exit(0); // Encerra o programa
  }

  glutPostRedisplay();
  glutTimerFunc(2000, timerSubstancias, 0);
}

void timerFatores(int valor) {
  if (sistema.sistemaTerminado)
    return;

  atualizarFatores();

  glutTimerFunc(1000, timerFatores, 0);
}

// ---------------------------------------------------------------------------
// HUD — Utilitários de Desenho
// ---------------------------------------------------------------------------

// Define a cor com r, g, b em [0.0, 1.0]
void definirCor(float r, float g, float b) { glColor3f(r, g, b); }

// Desenha um retângulo preenchido (coordenadas em píxeis, origem canto
// inferior-esquerdo)
void desenharRetangulo(float x, float y, float largura, float altura) {
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + largura, y);
  glVertex2f(x + largura, y + altura);
  glVertex2f(x, y + altura);
  glEnd();
}

// Desenha apenas o contorno de um retângulo
void desenharContorno(float x, float y, float largura, float altura,
                      float espessura) {
  // Topo
  desenharRetangulo(x, y + altura - espessura, largura, espessura);
  // Fundo
  desenharRetangulo(x, y, largura, espessura);
  // Esquerda
  desenharRetangulo(x, y, espessura, altura);
  // Direita
  desenharRetangulo(x + largura - espessura, y, espessura, altura);
}

// Desenha texto numa posição (coordenadas em píxeis)
void desenharTexto(const char *texto, float x, float y) {
  glRasterPos2f(x, y);
  for (const char *c = texto; *c != '\0'; c++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
  }
}

// Desenha texto pequeno (12px — para labels secundárias nas barras)
void desenharTextoPequeno(const char *texto, float x, float y) {
  glRasterPos2f(x, y);
  for (const char *c = texto; *c != '\0'; c++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
  }
}

// Desenha texto médio (18px — para painel de fatores e topbar)
void desenharTextoMedio(const char *texto, float x, float y) {
  glRasterPos2f(x, y);
  for (const char *c = texto; *c != '\0'; c++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
  }
}

// ---------------------------------------------------------------------------
// HUD — Barra de Substância
//
//  xBase, yBase  : canto inferior-esquerdo da barra
//  largura       : largura total da barra (100% = largura)
//  altura        : altura da barra
//  percentagem   : valor atual (0.0 - 100.0)
//  limiar        : posição do risco vermelho (ex: 70.0 para Alfa)
//  rFill, gFill, bFill : cor do preenchimento
//  nome          : label à esquerda (ex: "ALFA")
// ---------------------------------------------------------------------------
void desenharBarraSubstancia(float xBase, float yBase, float largura,
                             float altura, float percentagem, float limiar,
                             float rFill, float gFill, float bFill,
                             const char *nome) {

  const float ESPESSURA_BORDA = 2.0f;
  const float ESPACO_LABEL = 6.0f; // espaço entre label+% e a barra
  const float OFFSET_Y_LABEL =
      4.0f; // afastamento vertical do texto acima da barra

  // --- Fundo escuro da barra ---
  definirCor(0.08f, 0.10f, 0.12f);
  desenharRetangulo(xBase, yBase, largura, altura);

  // --- Preenchimento proporcional ---
  float larguraPreench = (percentagem / 100.0f) * largura;
  if (larguraPreench > 0.0f) {
    // Gradiente simulado: lado esquerdo mais escuro
    definirCor(rFill * 0.6f, gFill * 0.6f, bFill * 0.0f);
    desenharRetangulo(xBase, yBase, larguraPreench * 0.4f, altura);

    definirCor(rFill, gFill, bFill);
    desenharRetangulo(xBase + larguraPreench * 0.4f, yBase,
                      larguraPreench * 0.6f, altura);
  }

  // --- Risco de limiar (vermelho) ---
  float xLimiar = xBase + (limiar / 100.0f) * largura;
  definirCor(0.9f, 0.05f, 0.05f);
  desenharRetangulo(xLimiar - 1.5f, yBase - 3.0f, 3.0f, altura + 6.0f);

  // --- Borda / contorno ---
  definirCor(0.35f, 0.40f, 0.45f);
  desenharContorno(xBase, yBase, largura, altura, ESPESSURA_BORDA);

  // --- Label do nome (esquerda, acima da barra) ---
  definirCor(rFill, gFill, bFill);
  desenharTexto(nome, xBase, yBase + altura + ESPACO_LABEL + OFFSET_Y_LABEL);

  // --- Percentagem (direita, acima da barra) ---
  char bufPerc[16];
  snprintf(bufPerc, sizeof(bufPerc), "%.0f%%", percentagem);

  // Calcular largura aproximada do texto para alinhar à direita
  int larguraTexto = (int)(strlen(bufPerc) * 11);
  definirCor(rFill, gFill, bFill);
  desenharTexto(bufPerc, xBase + largura - larguraTexto,
                yBase + altura + ESPACO_LABEL + OFFSET_Y_LABEL);

  // --- Label do limiar (pequena, por baixo do risco) ---
  char bufLimiar[16];
  snprintf(bufLimiar, sizeof(bufLimiar), "%.0f%%", limiar);
  definirCor(0.85f, 0.15f, 0.15f);
  desenharTextoPequeno(bufLimiar, xLimiar - 8.0f, yBase - 16.0f);
}

// ---------------------------------------------------------------------------
// HUD — Painel principal das substâncias
// ---------------------------------------------------------------------------
void desenharPainelSubstancias() {
  // Dimensões e posicionamento central
  const float LARGURA_BARRA = 420.0f;
  const float ALTURA_BARRA = 32.0f;
  const float ESPACAMENTO = 70.0f; // distância vertical entre as duas barras
  const float xCentro = (LARGURA_JANELA - LARGURA_BARRA) / 2.0f;
  const float yCentroJanela = ALTURA_JANELA / 2.0f;

  // Posições Y das barras (a barra de Alfa fica acima da de Beta)
  float yAlfa = yCentroJanela + ESPACAMENTO / 2.0f;
  float yBeta = yCentroJanela - ALTURA_BARRA - ESPACAMENTO / 2.0f;

  // Fundo do painel — caixa semitransparente atrás das duas barras
  definirCor(0.05f, 0.07f, 0.09f);
  desenharRetangulo(xCentro - 20.0f, yBeta - 30.0f, LARGURA_BARRA + 40.0f,
                    (yAlfa + ALTURA_BARRA + 36.0f) - (yBeta - 30.0f));

  // Borda do painel
  definirCor(0.20f, 0.28f, 0.35f);
  desenharContorno(xCentro - 20.0f, yBeta - 30.0f, LARGURA_BARRA + 40.0f,
                   (yAlfa + ALTURA_BARRA + 36.0f) - (yBeta - 30.0f), 1.5f);

  // --- Barra ALFA: amarelo-âmbar, limiar em 70% ---
  desenharBarraSubstancia(xCentro, yAlfa, LARGURA_BARRA, ALTURA_BARRA,
                          sistema.alfa, 70.0f, 1.0f, 0.72f,
                          0.0f, // amarelo-âmbar
                          "ALFA");

  // --- Barra BETA: laranja, limiar em 80% ---
  desenharBarraSubstancia(xCentro, yBeta, LARGURA_BARRA, ALTURA_BARRA,
                          sistema.beta, 80.0f, 1.0f, 0.38f, 0.0f, // laranja
                          "BETA");
}

// ---------------------------------------------------------------------------
// HUD — Topbar de alertas
// ---------------------------------------------------------------------------
void desenharTopbar() {
  const float ALTURA_TOPBAR = 44.0f;
  const float ALTURA_FONTE = 14.0f; // altura aprox. HELVETICA_18
  // yBase ancorado ao topo real da janela (atualizado pelo reshape)
  const float yBase = 560.0f;
  // Centra o texto verticalmente dentro da barra
  const float yTexto = yBase + (ALTURA_TOPBAR - ALTURA_FONTE) / 2.0f;

  // Fundo
  if (sistema.alertaPreIgnicao || sistema.alertaCorrosao)
    definirCor(0.18f, 0.03f, 0.03f);
  else
    definirCor(0.06f, 0.07f, 0.09f);
  desenharRetangulo(0.0f, yBase, (float)larguraJanela, ALTURA_TOPBAR);

  // Linha divisória na base da topbar
  definirCor(0.25f, 0.30f, 0.36f);
  desenharRetangulo(0.0f, yBase - 1.5f, (float)larguraJanela, 1.5f);

  float xCursor = 20.0f;

  if (!sistema.alertaPreIgnicao && !sistema.alertaCorrosao) {
    definirCor(0.30f, 0.38f, 0.42f);
    desenharTextoMedio("SISTEMA NORMAL", xCursor, yTexto);
    return;
  }

  if (sistema.alertaPreIgnicao) {
    // Indicador colorido
    definirCor(1.0f, 0.72f, 0.0f);
    desenharRetangulo(xCursor, yTexto, 6.0f, ALTURA_FONTE);
    xCursor += 14.0f;

    definirCor(1.0f, 0.20f, 0.10f);
    desenharTextoMedio("ALERTA: PRE-IGNICAO", xCursor, yTexto);
    xCursor += 232.0f;

    definirCor(0.85f, 0.85f, 0.85f);
    desenharTextoMedio("ALFA acima do limite critico (70%)", xCursor, yTexto);
    xCursor += 310.0f;
  }

  if (sistema.alertaCorrosao) {
    if (sistema.alertaPreIgnicao) {
      definirCor(0.35f, 0.20f, 0.10f);
      desenharRetangulo(xCursor, yTexto, 1.5f, ALTURA_FONTE);
      xCursor += 16.0f;
    }

    definirCor(1.0f, 0.38f, 0.0f);
    desenharRetangulo(xCursor, yTexto, 6.0f, ALTURA_FONTE);
    xCursor += 14.0f;

    definirCor(1.0f, 0.20f, 0.10f);
    desenharTextoMedio("ALERTA: CORROSAO", xCursor, yTexto);
    xCursor += 204.0f;

    definirCor(0.85f, 0.85f, 0.85f);
    desenharTextoMedio("BETA acima do limite critico (80%)", xCursor, yTexto);
  }
}

// ---------------------------------------------------------------------------
// HUD — Painel de fatores externos (canto inferior esquerdo)
// ---------------------------------------------------------------------------
void desenharPainelFatores() {
  // Todas as medidas em píxeis fixos — não escalam com a janela
  const float MARGEM = 16.0f;
  const float LARGURA_PANEL = 270.0f;
  const float ALTURA_TITULO = 22.0f; // altura da linha de título
  const float ALTURA_LINHA = 22.0f;  // altura fixa de cada linha de fator
  const float SEP = 1.0f;            // espessura da linha separadora
  const float ALTURA_PANEL = ALTURA_TITULO + SEP + ALTURA_LINHA * 3.0f + 16.0f;
  const float xBase = MARGEM;
  const float yBase = MARGEM;
  const float xTexto = xBase + 12.0f;
  const float xValor =
      xBase + LARGURA_PANEL - 52.0f; // coluna de valores alinhada à direita

  // Fundo do painel
  definirCor(0.05f, 0.07f, 0.09f);
  desenharRetangulo(xBase, yBase, LARGURA_PANEL, ALTURA_PANEL);

  // Borda
  definirCor(0.20f, 0.28f, 0.35f);
  desenharContorno(xBase, yBase, LARGURA_PANEL, ALTURA_PANEL, 1.5f);

  // Título — posicionado a partir do topo do painel
  float yTitulo = yBase + ALTURA_PANEL - ALTURA_TITULO + 4.0f;
  definirCor(0.45f, 0.55f, 0.62f);
  desenharTextoMedio("FATORES EXTERNOS", xTexto, yTitulo);

  // Linha separadora sob o título
  float ySep = yBase + ALTURA_PANEL - ALTURA_TITULO;
  definirCor(0.20f, 0.28f, 0.35f);
  desenharRetangulo(xBase + 8.0f, ySep, LARGURA_PANEL - 16.0f, SEP);

  // Linhas de fatores — espaçamento fixo de ALTURA_LINHA px
  char buf[32];

  float yLinha = ySep - ALTURA_LINHA + 4.0f;
  definirCor(0.55f, 0.65f, 0.70f);
  desenharTextoMedio("Pressao Mangueiras:", xTexto, yLinha);
  snprintf(buf, sizeof(buf), "%.2f", sistema.pressaoMangueiras.valor);
  definirCor(1.0f, 0.38f, 0.0f);
  desenharTextoMedio(buf, xValor, yLinha);

  yLinha -= ALTURA_LINHA;
  definirCor(0.55f, 0.65f, 0.70f);
  desenharTextoMedio("Pressao Atmosferica:", xTexto, yLinha);
  snprintf(buf, sizeof(buf), "%.2f", sistema.pressaoAtmosferica.valor);
  definirCor(0.75f, 0.55f, 0.10f);
  desenharTextoMedio(buf, xValor, yLinha);

  yLinha -= ALTURA_LINHA;
  definirCor(0.55f, 0.65f, 0.70f);
  desenharTextoMedio("Temp. Ambiente:     ", xTexto, yLinha);
  snprintf(buf, sizeof(buf), "%.2f", sistema.temperaturaAmbiente.valor);
  definirCor(1.0f, 0.72f, 0.0f);
  desenharTextoMedio(buf, xValor, yLinha);
}

// ---------------------------------------------------------------------------
// Reshape — atualiza dimensões reais da janela
// ---------------------------------------------------------------------------
void reshape(int novaLargura, int novaAltura) {
  larguraJanela = novaLargura;
  alturaJanela = novaAltura;

  glViewport(0, 0, novaLargura, novaAltura);

  // Reconfigurar a projeção aqui garante que está sempre sincronizada
  // com as dimensões reais da janela antes de qualquer draw
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, (float)novaLargura, 0, (float)novaAltura);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glutPostRedisplay();
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void display() {
  glClear(GL_COLOR_BUFFER_BIT);

  // Configura projeção 2D ortogonal (píxeis)
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, LARGURA_JANELA, 0, ALTURA_JANELA);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  desenharTopbar();
  desenharPainelSubstancias();
  desenharPainelFatores();

  glutSwapBuffers();
}

// ---------------------------------------------------------------------------
// Teclado
// ---------------------------------------------------------------------------
void teclado(unsigned char key, int x, int y) {
  switch (key) {
  case 'b':
  case 'B':
    printf("Beta: %.1f%%\n", sistema.beta);
    break;
  case 'a':
  case 'A':
    printf("Alfa: %.1f%%\n", sistema.alfa);
    break;
  case 'h':
  case 'H':
    printf("--- Estado do Sistema ---\n");
    printf("Alfa : %.1f%%\n", sistema.alfa);
    printf("Beta : %.1f%%\n", sistema.beta);
    printf("Estado: %d\n", sistema.estado);
    printf("Alerta Pre-Ignicao: %s\n",
           sistema.alertaPreIgnicao ? "SIM" : "NAO");
    printf("Alerta Corrosao   : %s\n", sistema.alertaCorrosao ? "SIM" : "NAO");
    printf("Pressao Mangueiras  : %.2f\n", sistema.pressaoMangueiras.valor);
    printf("Pressao Atmosferica : %.2f\n", sistema.pressaoAtmosferica.valor);
    printf("Temperatura Ambiente: %.2f\n", sistema.temperaturaAmbiente.valor);
    break;
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
  // Inicializa o gerador de números aleatórios
  // Usamos o tempo atual como semente para garantir que os valores sejam
  // diferentes a cada execução
  srand((unsigned int)time(NULL));

  inicializarSistema();

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(LARGURA_JANELA, ALTURA_JANELA);

  int larguraEcra = glutGet(GLUT_SCREEN_WIDTH);
  int alturaEcra = glutGet(GLUT_SCREEN_HEIGHT);
  glutInitWindowPosition((larguraEcra - LARGURA_JANELA) / 2,
                         (alturaEcra - ALTURA_JANELA) / 2);

  glutCreateWindow("Space Dilemma");

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(teclado);

  glutTimerFunc(1000, timerFatores, 0);
  glutTimerFunc(2000, timerSubstancias, 0);

  glutMainLoop();
  return 0;
}
