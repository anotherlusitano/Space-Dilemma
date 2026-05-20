#include <GL/freeglut.h>
#include <cstdio>
#include <cstdlib>
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
  sistema.alfa = 0.0f;
  sistema.beta = 0.0f;
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

// Atualiza os fatores externos (a cada ~1 segundo)
void atualizarFatores() {
  sistema.pressaoMangueiras.valor = aleatorio(
      sistema.pressaoMangueiras.valorMin, sistema.pressaoMangueiras.valorMax);
  sistema.pressaoAtmosferica.valor = aleatorio(
      sistema.pressaoAtmosferica.valorMin, sistema.pressaoAtmosferica.valorMax);
  sistema.temperaturaAmbiente.valor =
      aleatorio(sistema.temperaturaAmbiente.valorMin,
                sistema.temperaturaAmbiente.valorMax);

  // As substâncias afetam a pressão das mangueiras: mais alfa+beta = mais
  // pressão
  float pressaoExtra = (sistema.alfa + sistema.beta) / 200.0f;
  sistema.pressaoMangueiras.valor += pressaoExtra * 0.3f;
  if (sistema.pressaoMangueiras.valor > sistema.pressaoMangueiras.valorMax)
    sistema.pressaoMangueiras.valor = sistema.pressaoMangueiras.valorMax;
}

// Calcula o multiplicador de aumento com base nos fatores externos
//   fatores baixos  (media < 0.4) -> 1x  (~1 a 5)
//   fatores médios  (media < 0.7) -> 2x  (~5 a 10)
//   fatores altos   (media >= 0.7)-> 3x  (~10 a 15)
float calcularMultiplicador() {
  // Calcula a média dos fatores externos
  float media =
      (sistema.pressaoMangueiras.valor + sistema.pressaoAtmosferica.valor +
       sistema.temperaturaAmbiente.valor) /
      3.0f;

  if (media < 0.4f)
    return 1.0f;
  if (media < 0.7f)
    return 2.0f;
  return 3.0f;
}

// Atualiza Alfa e Beta (a cada ~2 segundos)
void atualizarSubstancias() {
  float mult = calcularMultiplicador();

  // Aumenta Alfa e Beta com base no multiplicador
  sistema.alfa += aleatorio(1.0f, 5.0f) * mult;
  sistema.beta += aleatorio(1.0f, 5.0f) * mult;

  // Limita os valores para não ultrapassarem 100%
  if (sistema.alfa > 100.0f)
    sistema.alfa = 100.0f;
  if (sistema.beta > 100.0f)
    sistema.beta = 100.0f;
}

// Avalia o estado do sistema com base nos valores atuais
void avaliarEstado() {
  // Verifica falha crítica
  if (sistema.alfa >= 100.0f || sistema.beta >= 100.0f) {
    sistema.estado = FALHA;
    sistema.sistemaTerminado = true;
    return;
  }

  // Atualiza os alertas
  sistema.alertaPreIgnicao = (sistema.alfa > 70.0f);
  sistema.alertaCorrosao = (sistema.beta > 80.0f);

  // Define o estado com base nos alertas
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
    return;
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
// Display
// ---------------------------------------------------------------------------
void display() {
  glClear(GL_COLOR_BUFFER_BIT);
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
