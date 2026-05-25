#include <GL/freeglut.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Dimensões da Janela
const int LARGURA_JANELA = 800;
const int ALTURA_JANELA = 600;

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

// Desenha texto pequeno (para labels secundárias)
void desenharTextoPequeno(const char *texto, float x, float y) {
  glRasterPos2f(x, y);
  for (const char *c = texto; *c != '\0'; c++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
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

  desenharPainelSubstancias();

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
  glutKeyboardFunc(teclado);

  glutTimerFunc(1000, timerFatores, 0);
  glutTimerFunc(2000, timerSubstancias, 0);

  glutMainLoop();
  return 0;
}
