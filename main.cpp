#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Dimensões da Janela
const int LARGURA_JANELA = 800;
const int ALTURA_JANELA = 600;
int larguraJanela = LARGURA_JANELA;
int alturaJanela = ALTURA_JANELA;

// Ecrã ativo: 1 = Monitorização, 2 = Controlo & Histórico
int ecraoAtivo = 1;

// Snackbar — mensagem temporária no ecrã 1
char snackbarMsg[64] = "";
int snackbarTicks = 0; // ticks restantes (a 1s cada)
const int SNACKBAR_DURACAO = 4;

// ---------------------------------------------------------------------------
// Sistema de Logs
// ---------------------------------------------------------------------------
FILE *ficheiroLog = nullptr;

void inicializarLog() {
  ficheiroLog = fopen("logs.txt", "a");
  if (!ficheiroLog) {
    printf("[AVISO] Nao foi possivel abrir logs.txt\n");
    return;
  }
  // Separador de sessão
  fprintf(ficheiroLog, "\n================================================\n");
  fprintf(ficheiroLog, "     Nova Sessao de Monitorizacao\n");
  fprintf(ficheiroLog, "================================================\n");
}

void gravarLog(const char *mensagem) {
  if (!ficheiroLog)
    return;

  time_t agora = time(nullptr);
  struct tm *t = localtime(&agora);
  fprintf(ficheiroLog, "[%04d-%02d-%02d : %02d:%02d:%02d] %s\n",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min,
          t->tm_sec, mensagem);
  fflush(ficheiroLog); // garante escrita imediata
}

void fecharLog() {
  if (!ficheiroLog)
    return;
  gravarLog("Sistema encerrado.");
  fclose(ficheiroLog);
  ficheiroLog = nullptr;
}

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
// Fase de comportamento de cada fator externo
enum FaseDeriva {
  FASE_ESTAVEL, // oscila aleatoriamente perto do neutro
  FASE_DERIVA,  // afasta-se consistentemente do neutro
  FASE_CRITICO  // preso na zona extrema, só a equipa resolve
};

struct FatorExterno {
  const char *nome;
  const char *unidade; // ex: "C", "bar", "kPa"
  float valor;
  float neutro;
  float valorMin;
  float valorMax;
  FaseDeriva fase;
  float direcaoDeriva; // +1.0 = afasta para cima, -1.0 = afasta para baixo
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

  // Equipa — índice do fator a ser estabilizado (-1 = nenhum)
  // 0 = pressaoMangueiras, 1 = pressaoAtmosferica, 2 = temperaturaAmbiente
  int fatorEmEstabilizacao;
};

// Instância global do estado
EstadoPrincipal sistema;

// ---------------------------------------------------------------------------
// Zonas de clique dos botões (calculadas em desenharPainelSubstancias)
// ---------------------------------------------------------------------------
struct ZonaClique {
  float x, y, largura, altura;
  bool contem(float mx, float my) const {
    return mx >= x && mx <= x + largura && my >= y && my <= y + altura;
  }
};

ZonaClique botaoAumentarAlfa = {0, 0, 0, 0};
ZonaClique botaoAumentarBeta = {0, 0, 0, 0};

// Zonas de clique dos botões da equipa (ecrã 2)
ZonaClique botaoEquipaMang = {0, 0, 0, 0};
ZonaClique botaoEquipaCam = {0, 0, 0, 0};
ZonaClique botaoEquipaTemp = {0, 0, 0, 0};

// Zona de clique do botão de navegação entre ecrãs
ZonaClique botaoNavegacao = {0, 0, 0, 0};

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

  // nome  unidade  valor  neutro  min   max  fase direção
  sistema.pressaoMangueiras = {
      "Pressao Mangueiras", "bar", 6.0f, 6.0f, 2.0f, 12.0f, FASE_ESTAVEL, 0.0f};
  sistema.pressaoAtmosferica = {"Pressao Camara", "kPa", 101.0f,
                                101.0f,           80.0f, 140.0f,
                                FASE_ESTAVEL,     0.0f};
  sistema.temperaturaAmbiente = {"Temp. Camara", "C",   20.0f,        20.0f,
                                 -20.0f,         60.0f, FASE_ESTAVEL, 0.0f};

  sistema.fatorEmEstabilizacao = -1;
}

// ---------------------------------------------------------------------------
// Lógica de atualização
// ---------------------------------------------------------------------------

float aleatorio(float min, float max) {
  // Gera um número aleatório entre um valor mínimo e máximo
  return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// Normaliza um fator para [-1.0, +1.0] relativamente ao seu neutro.
// neutro = 0.0, max positivo = +1.0, max negativo = -1.0
float normalizarFator(const FatorExterno &f) {
  if (f.valor >= f.neutro)
    return (f.valor - f.neutro) / (f.valorMax - f.neutro);
  else
    return (f.valor - f.neutro) / (f.neutro - f.valorMin);
}

// Chama a equipa para estabilizar um fator (0=mangueiras, 1=camara, 2=temp)
// Só aceita se não houver outro fator em estabilização
void chamarEquipa(int indiceFator) {
  if (sistema.fatorEmEstabilizacao != -1)
    return; // equipa ocupada
  sistema.fatorEmEstabilizacao = indiceFator;

  const char *nomes[3] = {sistema.pressaoMangueiras.nome,
                          sistema.pressaoAtmosferica.nome,
                          sistema.temperaturaAmbiente.nome};
  char buf[128];
  snprintf(buf, sizeof(buf), "Operador chamou equipa para: %s",
           nomes[indiceFator]);
  gravarLog(buf);
}

// Clampa um valor entre min e max
float clampF(float v, float min, float max) {
  return v < min ? min : (v > max ? max : v);
}

// ---------------------------------------------------------------------------
// Constantes de afinação dos fatores externos
// Ajusta estes valores para calibrar o comportamento do sistema
// ---------------------------------------------------------------------------
const float FATOR_LIMIAR_DERIVA =
    0.30f; // % do range desde neutro para entrar em deriva
const float FATOR_LIMIAR_CRITICO =
    0.70f; // % do range desde neutro para entrar em crítico

const float FATOR_OSCILACAO_ESTAVEL =
    0.05f; // oscilação aleatória em fase estável (% range/tick)
const float FATOR_OSCILACAO_DERIVA =
    0.03f; // oscilação aleatória em fase de deriva (% range/tick)
const float FATOR_OSCILACAO_CRITICO =
    0.02f; // oscilação aleatória em fase crítica (% range/tick)

const float FATOR_BIAS_DERIVA =
    0.03f; // bias de afastamento em fase de deriva (% range/tick)

// Atualiza os fatores externos (a cada ~1 segundo).
// Associações:
//   pressaoMangueiras   -> afeta Beta
//   pressaoAtmosferica  -> afeta Alfa e Beta
//   temperaturaAmbiente -> afeta Alfa
//
// Cada fator passa por 3 fases: ESTAVEL -> DERIVA -> CRITICO
// Só a equipa consegue repor um fator crítico para o neutro.
void atualizarFatores() {

  auto atualizarFase = [](FatorExterno &f) {
    float range = f.valorMax - f.valorMin;
    float distancia = f.valor - f.neutro; // positivo = acima do neutro

    // Calcula o desvio normalizado: 0.0 = neutro, 1.0 = extremo
    float desvioNorm;
    if (distancia >= 0.0f)
      desvioNorm = distancia / (f.valorMax - f.neutro);
    else
      desvioNorm = -distancia / (f.neutro - f.valorMin);

    // --- Transições de fase ---
    if (f.fase == FASE_ESTAVEL) {
      if (desvioNorm >= FATOR_LIMIAR_DERIVA) {
        f.fase = FASE_DERIVA;
        f.direcaoDeriva = (distancia >= 0.0f) ? 1.0f : -1.0f;
      }
    } else if (f.fase == FASE_DERIVA) {
      if (desvioNorm >= FATOR_LIMIAR_CRITICO) {
        f.fase = FASE_CRITICO;
      } else if (desvioNorm < FATOR_LIMIAR_DERIVA * 0.5f) {
        // Voltou perto do neutro (histerese) -> estável de novo
        f.fase = FASE_ESTAVEL;
        f.direcaoDeriva = 0.0f;
      }
    }
    // FASE_CRITICO só sai via equipa (ver estabilizarFator)

    // --- Variação por fase ---
    float variacao = 0.0f;
    switch (f.fase) {
    case FASE_ESTAVEL:
      variacao = aleatorio(-range * FATOR_OSCILACAO_ESTAVEL,
                           range * FATOR_OSCILACAO_ESTAVEL);
      break;
    case FASE_DERIVA:
      // Bias consistente na direção de deriva + pequena oscilação
      variacao = f.direcaoDeriva * range * FATOR_BIAS_DERIVA;
      variacao += aleatorio(-range * FATOR_OSCILACAO_DERIVA,
                            range * FATOR_OSCILACAO_DERIVA);
      break;
    case FASE_CRITICO:
      // Oscila ligeiramente mas fica preso na zona crítica
      variacao = aleatorio(-range * FATOR_OSCILACAO_CRITICO,
                           range * FATOR_OSCILACAO_CRITICO);
      break;
    }

    f.valor = clampF(f.valor + variacao, f.valorMin, f.valorMax);
  };

  atualizarFase(sistema.pressaoMangueiras);
  atualizarFase(sistema.pressaoAtmosferica);
  atualizarFase(sistema.temperaturaAmbiente);

  // Feedback das substâncias sobre os fatores (só em fase estável/deriva,
  // não interfere com fator crítico)
  float desvioAlfa = (sistema.alfa - 50.0f) / 100.0f;
  float desvioBeta = (sistema.beta - 50.0f) / 100.0f;

  if (sistema.pressaoMangueiras.fase != FASE_CRITICO) {
    float range =
        sistema.pressaoMangueiras.valorMax - sistema.pressaoMangueiras.valorMin;
    sistema.pressaoMangueiras.valor = clampF(
        sistema.pressaoMangueiras.valor + desvioBeta * range * 0.05f,
        sistema.pressaoMangueiras.valorMin, sistema.pressaoMangueiras.valorMax);
  }
  if (sistema.temperaturaAmbiente.fase != FASE_CRITICO) {
    float range = sistema.temperaturaAmbiente.valorMax -
                  sistema.temperaturaAmbiente.valorMin;
    sistema.temperaturaAmbiente.valor =
        clampF(sistema.temperaturaAmbiente.valor + desvioAlfa * range * 0.05f,
               sistema.temperaturaAmbiente.valorMin,
               sistema.temperaturaAmbiente.valorMax);
  }

  // Estabilização gradual pela equipa
  if (sistema.fatorEmEstabilizacao >= 0) {
    FatorExterno *f = nullptr;
    if (sistema.fatorEmEstabilizacao == 0)
      f = &sistema.pressaoMangueiras;
    else if (sistema.fatorEmEstabilizacao == 1)
      f = &sistema.pressaoAtmosferica;
    else if (sistema.fatorEmEstabilizacao == 2)
      f = &sistema.temperaturaAmbiente;

    if (f != nullptr) {
      f->valor += (f->neutro - f->valor) * 0.15f;
      // Repõe fase a estável e liberta a equipa quando próximo do neutro
      if (fabsf(f->valor - f->neutro) < 0.5f) {
        f->valor = f->neutro;
        f->fase = FASE_ESTAVEL;
        f->direcaoDeriva = 0.0f;
        // Grava log antes de libertar o ponteiro
        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf),
                 "Equipa concluiu estabilizacao de: %s (valor neutro: %.1f %s)",
                 f->nome, f->neutro, f->unidade);
        gravarLog(logBuf);
        // Ativa snackbar no ecrã 1
        snprintf(snackbarMsg, sizeof(snackbarMsg), "Equipa: %s estabilizado.",
                 f->nome);
        snackbarTicks = SNACKBAR_DURACAO;
        sistema.fatorEmEstabilizacao = -1;
      }
    }
  }
}

// Atualiza Alfa e Beta mantendo a soma = 100.
//
// Cada fator é normalizado para [-1.0, +1.0] relativamente ao seu neutro
// com normalizarFator(), garantindo que a escala real (bar, kPa, °C) não
// introduz assimetria na influência sobre as substâncias.
//   temperaturaAmbiente -> sobe Alfa  (+)
//   pressaoMangueiras   -> desce Alfa (-) [sobe Beta]
//   pressaoAtmosferica  -> afeta ambos
void atualizarSubstancias() {
  float dTemp = normalizarFator(sistema.temperaturaAmbiente);
  float dMang = normalizarFator(sistema.pressaoMangueiras);
  float dAtm = normalizarFator(sistema.pressaoAtmosferica);

  // Magnitude aleatória comum — pesos iguais para todos os fatores
  float magnitude = aleatorio(1.0f, 4.0f);

  float deltaAlfa = 0.0f;
  deltaAlfa += dTemp * magnitude; // temperatura alta -> mais Alfa
  deltaAlfa -= dMang * magnitude; // pressão mangueiras alta -> mais Beta
  deltaAlfa += dAtm * magnitude;  // pressão câmara afeta ambos

  sistema.alfa = clampF(sistema.alfa + deltaAlfa, 0.0f, 100.0f);
  sistema.beta = 100.0f - sistema.alfa;
}

// Avalia o estado do sistema com base nos valores atuais.
// Deteta transições de alerta e grava-as no log.
void avaliarEstado() {
  if (sistema.alfa >= 100.0f || sistema.beta >= 100.0f) {
    if (!sistema.sistemaTerminado) {
      char buf[128];
      snprintf(buf, sizeof(buf), "FALHA CRITICA — Alfa: %.1f%% | Beta: %.1f%%",
               sistema.alfa, sistema.beta);
      gravarLog(buf);
    }
    sistema.estado = FALHA;
    sistema.sistemaTerminado = true;
    return;
  }

  bool novoPreIgnicao = (sistema.alfa > 70.0f);
  bool novaCorrosao = (sistema.beta > 80.0f);
  char buf[128];

  // Transições de pré-ignição
  if (novoPreIgnicao && !sistema.alertaPreIgnicao) {
    snprintf(buf, sizeof(buf), "ALERTA: Pre-ignicao ativado — Alfa em %.1f%%",
             sistema.alfa);
    gravarLog(buf);
  } else if (!novoPreIgnicao && sistema.alertaPreIgnicao) {
    snprintf(buf, sizeof(buf), "Pre-ignicao resolvido — Alfa em %.1f%%",
             sistema.alfa);
    gravarLog(buf);
  }

  // Transições de corrosão
  if (novaCorrosao && !sistema.alertaCorrosao) {
    snprintf(buf, sizeof(buf),
             "ALERTA: Corrosao de valvulas ativado — Beta em %.1f%%",
             sistema.beta);
    gravarLog(buf);
  } else if (!novaCorrosao && sistema.alertaCorrosao) {
    snprintf(buf, sizeof(buf),
             "Corrosao de valvulas resolvido — Beta em %.1f%%", sistema.beta);
    gravarLog(buf);
  }

  sistema.alertaPreIgnicao = novoPreIgnicao;
  sistema.alertaCorrosao = novaCorrosao;

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

  // Conta down da snackbar
  if (snackbarTicks > 0) {
    snackbarTicks--;
    glutPostRedisplay();
  }

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
  const float xCentro = (larguraJanela - LARGURA_BARRA) / 2.0f;
  const float yCentroJanela = alturaJanela / 1.7f;

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

  // -----------------------------------------------------------------------
  // --- Botões de aumento ---
  const float LARGURA_BTN = 180.0f;
  const float ALTURA_BTN = 28.0f;
  const float ESPACO_BTN = -50.0f; // espaço entre o fundo do painel e os botões
  // Botão Alfa: centrado sob a barra de Alfa, deslocado para a esquerda
  float xBtnAlfa = xCentro + LARGURA_BARRA / 2.0f - LARGURA_BTN - 8.0f;
  float yBtnAlfa = yBeta - 30.0f + ESPACO_BTN;
  // Botão Beta: centrado sob a barra de Beta, deslocado para a direita
  float xBtnBeta = xCentro + LARGURA_BARRA / 2.0f + 8.0f;
  float yBtnBeta = yBeta - 30.0f + ESPACO_BTN;

  // Guardar zonas de clique globais
  botaoAumentarAlfa = {xBtnAlfa, yBtnAlfa, LARGURA_BTN, ALTURA_BTN};
  botaoAumentarBeta = {xBtnBeta, yBtnBeta, LARGURA_BTN, ALTURA_BTN};

  // Desenhar botão ALFA
  bool alfaMax = sistema.alfa >= 100.0f || sistema.sistemaTerminado;
  definirCor(alfaMax ? 0.15f : 0.18f, alfaMax ? 0.15f : 0.14f,
             alfaMax ? 0.15f : 0.02f);
  desenharRetangulo(xBtnAlfa, yBtnAlfa, LARGURA_BTN, ALTURA_BTN);
  definirCor(alfaMax ? 0.30f : 1.0f, alfaMax ? 0.30f : 0.72f, 0.0f);
  desenharContorno(xBtnAlfa, yBtnAlfa, LARGURA_BTN, ALTURA_BTN, 1.5f);
  // Texto centrado no botão (HELVETICA_18 ~ 11px por char, 14px altura)
  const char *labelAlfa = "+ Aumentar ALFA";
  float largTextoAlfa = strlen(labelAlfa) * 9.5f;
  desenharTextoMedio(labelAlfa, xBtnAlfa + (LARGURA_BTN - largTextoAlfa) / 2.0f,
                     yBtnAlfa + (ALTURA_BTN - 14.0f) / 2.0f);

  // Desenhar botão BETA
  bool betaMax = sistema.beta >= 100.0f || sistema.sistemaTerminado;
  definirCor(betaMax ? 0.15f : 0.18f, betaMax ? 0.15f : 0.08f,
             betaMax ? 0.15f : 0.02f);
  desenharRetangulo(xBtnBeta, yBtnBeta, LARGURA_BTN, ALTURA_BTN);
  definirCor(betaMax ? 0.30f : 1.0f, betaMax ? 0.12f : 0.38f, 0.0f);
  desenharContorno(xBtnBeta, yBtnBeta, LARGURA_BTN, ALTURA_BTN, 1.5f);
  const char *labelBeta = "+ Aumentar BETA";
  float largTextoBeta = strlen(labelBeta) * 9.5f;
  desenharTextoMedio(labelBeta, xBtnBeta + (LARGURA_BTN - largTextoBeta) / 2.0f,
                     yBtnBeta + (ALTURA_BTN - 14.0f) / 2.0f);
}

// ---------------------------------------------------------------------------
// HUD — Barra de navegação entre ecrãs (abaixo da topbar)
// ---------------------------------------------------------------------------
void desenharBarraNavegacao() {
  const float ALTURA_TOPBAR = 44.0f;
  const float ALTURA_NAV = 30.0f;
  const float yBase = (float)alturaJanela - ALTURA_TOPBAR - ALTURA_NAV;

  // Fundo da barra de navegação
  definirCor(0.07f, 0.09f, 0.11f);
  desenharRetangulo(0.0f, yBase, (float)larguraJanela, ALTURA_NAV);

  // Linha divisória em baixo
  definirCor(0.18f, 0.24f, 0.30f);
  desenharRetangulo(0.0f, yBase, (float)larguraJanela, 1.0f);

  // Botão de navegação — canto direito da barra
  const float LARGURA_BTN = 160.0f;
  const float ALTURA_BTN = 22.0f;
  const float xBtn = (float)larguraJanela - LARGURA_BTN - 10.0f;
  const float yBtn = yBase + (ALTURA_NAV - ALTURA_BTN) / 2.0f;

  // Guardar zona de clique
  botaoNavegacao = {xBtn, yBtn, LARGURA_BTN, ALTURA_BTN};

  // Fundo do botão
  definirCor(0.10f, 0.16f, 0.22f);
  desenharRetangulo(xBtn, yBtn, LARGURA_BTN, ALTURA_BTN);

  // Borda do botão
  definirCor(0.25f, 0.45f, 0.60f);
  desenharContorno(xBtn, yBtn, LARGURA_BTN, ALTURA_BTN, 1.5f);

  // Label dinâmica
  const char *label =
      (ecraoAtivo == 1) ? "Ir para Ecra 2 >" : "< Ir para Ecra 1";
  float largTexto = strlen(label) * 9.0f;
  float xTexto = xBtn + (LARGURA_BTN - largTexto) / 2.0f;
  float yTexto = yBtn + (ALTURA_BTN - 14.0f) / 2.0f;

  definirCor(0.55f, 0.80f, 1.0f);
  desenharTextoMedio(label, xTexto, yTexto);

  // Indicador do ecrã ativo — lado esquerdo da barra
  definirCor(0.30f, 0.40f, 0.48f);
  const char *indicador =
      (ecraoAtivo == 1) ? "Ecra 1 — Monitorizacao" : "Ecra 2 — Controlo";
  desenharTextoMedio(indicador, 12.0f, yBase + (ALTURA_NAV - 14.0f) / 2.0f);
}

// ---------------------------------------------------------------------------
// HUD — Topbar de alertas
// ---------------------------------------------------------------------------
void desenharTopbar() {
  const float ALTURA_TOPBAR = 44.0f;
  const float ALTURA_FONTE = 14.0f; // altura aprox. HELVETICA_18
  // yBase ancorado ao topo real da janela (atualizado pelo reshape)
  const float yBase = alturaJanela - ALTURA_TOPBAR;
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
  const float LARGURA_PANEL = 310.0f;
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

  // Função auxiliar local para desenhar uma linha de fator
  auto desenharLinhaFator = [&](const FatorExterno &f, float yL, float r,
                                float g, float b) {
    definirCor(0.55f, 0.65f, 0.70f);
    desenharTextoMedio(f.nome, xTexto, yL);
    snprintf(buf, sizeof(buf), "%.1f %s", f.valor, f.unidade);
    definirCor(r, g, b);
    desenharTextoMedio(buf, xValor - 40.0f, yL);
  };

  float yLinha = ySep - ALTURA_LINHA + 4.0f;
  desenharLinhaFator(sistema.pressaoMangueiras, yLinha, 1.0f, 0.38f, 0.0f);
  yLinha -= ALTURA_LINHA;
  desenharLinhaFator(sistema.pressaoAtmosferica, yLinha, 0.75f, 0.55f, 0.10f);
  yLinha -= ALTURA_LINHA;
  desenharLinhaFator(sistema.temperaturaAmbiente, yLinha, 1.0f, 0.72f, 0.0f);
}

// ---------------------------------------------------------------------------
// HUD — Painel de comandos (canto inferior direito)
// ---------------------------------------------------------------------------
void desenharPainelComandos() {
  const float MARGEM = 16.0f;
  const float LARGURA_PANEL = 230.0f;
  const float ALTURA_TITULO = 65.0f;
  const float ALTURA_LINHA = 22.0f;
  const float SEP = 1.0f;
  const int NUM_LINHAS = 4;
  const float ALTURA_PANEL =
      ALTURA_TITULO + SEP + ALTURA_LINHA * NUM_LINHAS + 16.0f;
  const float xBase = (float)larguraJanela - LARGURA_PANEL - MARGEM;
  const float yBase = MARGEM;
  const float xTexto = xBase + 12.0f;
  const float xTecla = xBase + LARGURA_PANEL - 42.0f;

  // Fundo
  definirCor(0.05f, 0.07f, 0.09f);
  desenharRetangulo(xBase, yBase, LARGURA_PANEL, ALTURA_PANEL);

  // Borda
  definirCor(0.20f, 0.28f, 0.35f);
  desenharContorno(xBase, yBase, LARGURA_PANEL, ALTURA_PANEL, 1.5f);

  // Título
  float yTitulo = yBase + ALTURA_PANEL - 20.0f;
  definirCor(0.45f, 0.55f, 0.62f);
  desenharTextoMedio("COMANDOS", xTexto, yTitulo);

  // Indicador de ecrã ativo no título
  float yEcraoLabel = yBase + ALTURA_PANEL - 42.0f;
  definirCor(0.30f, 0.40f, 0.35f);
  desenharTextoMedio(ecraoAtivo == 1 ? "Ecra: Monitorizacao" : "Ecra: Controlo",
                     xTexto, yEcraoLabel);

  // Linha separadora
  float ySep = yBase + ALTURA_PANEL - ALTURA_TITULO + 12.0f;
  definirCor(0.20f, 0.28f, 0.35f);
  desenharRetangulo(xBase + 8.0f, ySep, LARGURA_PANEL - 16.0f, SEP);

  // Linhas de comandos
  float yLinha = ySep - ALTURA_LINHA + 4.0f;

  if (ecraoAtivo == 1) {
    // Aumentar Alfa (só no ecrã 1)
    definirCor(0.55f, 0.65f, 0.70f);
    desenharTextoMedio("Aumentar Alfa", xTexto, yLinha);
    definirCor(1.0f, 0.72f, 0.0f);
    desenharTextoMedio("[A]", xTecla, yLinha);
    yLinha -= ALTURA_LINHA;

    // Aumentar Beta (só no ecrã 1)
    definirCor(0.55f, 0.65f, 0.70f);
    desenharTextoMedio("Aumentar Beta", xTexto, yLinha);
    definirCor(1.0f, 0.38f, 0.0f);
    desenharTextoMedio("[B]", xTecla, yLinha);
    yLinha -= ALTURA_LINHA;
  }

  if (ecraoAtivo == 2) {
    // Chamar equipa — Mangueiras
    definirCor(0.55f, 0.65f, 0.70f);
    desenharTextoMedio("Equipa Mangueiras", xTexto, yLinha);
    definirCor(0.45f, 0.78f, 0.45f);
    desenharTextoMedio("[M]", xTecla, yLinha);
    yLinha -= ALTURA_LINHA;

    // Chamar equipa — Câmara
    definirCor(0.55f, 0.65f, 0.70f);
    desenharTextoMedio("Equipa Camara", xTexto, yLinha);
    definirCor(0.45f, 0.78f, 0.45f);
    desenharTextoMedio("[C]", xTecla, yLinha);
    yLinha -= ALTURA_LINHA;

    // Chamar equipa — Temperatura
    definirCor(0.55f, 0.65f, 0.70f);
    desenharTextoMedio("Equipa Temp.", xTexto, yLinha);
    definirCor(0.45f, 0.78f, 0.45f);
    desenharTextoMedio("[T]", xTecla, yLinha);
    yLinha -= ALTURA_LINHA;
  }

  // Ajuda (global)
  definirCor(0.55f, 0.65f, 0.70f);
  desenharTextoMedio("Ajuda (consola)", xTexto, yLinha);
  definirCor(0.45f, 0.55f, 0.62f);
  desenharTextoMedio("[H]", xTecla, yLinha);
  yLinha -= ALTURA_LINHA;

  // Mudar de ecrã (global)
  definirCor(0.55f, 0.65f, 0.70f);
  desenharTextoMedio(ecraoAtivo == 1 ? "Ecra Controlo" : "Ecra Monitor.",
                     xTexto, yLinha);
  definirCor(0.45f, 0.78f, 0.45f);
  desenharTextoMedio(ecraoAtivo == 1 ? "[2]" : "[1]", xTecla, yLinha);
}

// ---------------------------------------------------------------------------
// Rato — clique
// ---------------------------------------------------------------------------
void mouse(int botao, int estado, int x, int y) {
  // Só age no soltar do botão esquerdo
  if (botao != GLUT_LEFT_BUTTON || estado != GLUT_UP)
    return;
  if (sistema.sistemaTerminado)
    return;

  // Inverter Y: GLUT tem origem no canto superior esquerdo,
  // OpenGL no canto inferior esquerdo
  float mx = (float)x;
  float my = (float)(alturaJanela - y);

  if (botaoAumentarAlfa.contem(mx, my)) {
    if (sistema.alfa < 100.0f) {
      sistema.alfa += 5.0f;
      if (sistema.alfa > 100.0f)
        sistema.alfa = 100.0f;
      sistema.beta = 100.0f - sistema.alfa;
      avaliarEstado();
      {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Operador aumentou Alfa (botao) — Alfa: %.1f%% | Beta: %.1f%%",
                 sistema.alfa, sistema.beta);
        gravarLog(buf);
      }
      glutPostRedisplay();
    }
  } else if (botaoAumentarBeta.contem(mx, my)) {
    if (sistema.beta < 100.0f) {
      sistema.beta += 5.0f;
      if (sistema.beta > 100.0f)
        sistema.beta = 100.0f;
      sistema.alfa = 100.0f - sistema.beta;
      avaliarEstado();
      {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Operador aumentou Beta (botao) — Alfa: %.1f%% | Beta: %.1f%%",
                 sistema.alfa, sistema.beta);
        gravarLog(buf);
      }
      glutPostRedisplay();
    }
  }

  // Botão de navegação entre ecrãs (sempre disponível)
  if (botaoNavegacao.contem(mx, my)) {
    ecraoAtivo = (ecraoAtivo == 1) ? 2 : 1;
    glutPostRedisplay();
    return;
  }

  // Botões da equipa (ecrã 2)
  if (ecraoAtivo == 2) {
    if (botaoEquipaMang.contem(mx, my))
      chamarEquipa(0);
    else if (botaoEquipaCam.contem(mx, my))
      chamarEquipa(1);
    else if (botaoEquipaTemp.contem(mx, my))
      chamarEquipa(2);
    glutPostRedisplay();
  }
}

// ---------------------------------------------------------------------------
// HUD — Snackbar (mensagem temporária, centro inferior, ecrã 1)
// ---------------------------------------------------------------------------
void desenharSnackbar() {
  if (snackbarTicks <= 0 || snackbarMsg[0] == '\0')
    return;

  const float ALTURA_SB = 34.0f;
  const float MARGEM_SB = 60.0f;
  const float LARGURA_SB = (float)larguraJanela - 2.0f * MARGEM_SB;
  const float xBase = MARGEM_SB;
  const float yBase = 16.0f; // um pouco acima do fundo

  // Fundo
  definirCor(0.08f, 0.18f, 0.12f);
  desenharRetangulo(xBase, yBase, LARGURA_SB, ALTURA_SB);

  // Borda verde subtil
  definirCor(0.20f, 0.60f, 0.30f);
  desenharContorno(xBase, yBase, LARGURA_SB, ALTURA_SB, 1.5f);

  // Texto centrado
  int largTexto = (int)(strlen(snackbarMsg) * 10.0f);
  float xTexto = xBase + (LARGURA_SB - largTexto) / 2.0f;
  float yTexto = yBase + (ALTURA_SB - 14.0f) / 2.0f;

  definirCor(0.45f, 0.90f, 0.55f);
  desenharTextoMedio(snackbarMsg, xTexto, yTexto);
}

// ---------------------------------------------------------------------------
// Ecrã 2 — Controlo & Histórico
// ---------------------------------------------------------------------------
void desenharBotoesEquipa() {
  const float LARGURA_BTN = 320.0f;
  const float ALTURA_BTN = 52.0f;
  const float ESPACO_BTN = 14.0f;
  const float totalAltura = 3.0f * ALTURA_BTN + 2.0f * ESPACO_BTN;
  const float xBase = ((float)larguraJanela - LARGURA_BTN) / 2.0f;
  const float yBase = ((float)alturaJanela - totalAltura) / 2.0f;

  // Título central
  definirCor(0.45f, 0.55f, 0.62f);
  desenharTexto("CHAMAR EQUIPA", xBase, yBase + totalAltura + 20.0f);

  // Linha separadora sob título
  definirCor(0.20f, 0.28f, 0.35f);
  desenharRetangulo(xBase, yBase + totalAltura + 16.0f, LARGURA_BTN, 1.5f);

  // Dados dos 3 botões
  struct InfoBotao {
    const char *label;
    const char *tecla;
    const char *valor;
    int indice;
    ZonaClique *zona;
  };

  char bufM[24], bufC[24], bufT[24];
  snprintf(bufM, sizeof(bufM), "%.1f %s", sistema.pressaoMangueiras.valor,
           sistema.pressaoMangueiras.unidade);
  snprintf(bufC, sizeof(bufC), "%.1f %s", sistema.pressaoAtmosferica.valor,
           sistema.pressaoAtmosferica.unidade);
  snprintf(bufT, sizeof(bufT), "%.1f %s", sistema.temperaturaAmbiente.valor,
           sistema.temperaturaAmbiente.unidade);

  InfoBotao botoes[3] = {
      {sistema.pressaoMangueiras.nome, "[M]", bufM, 0, &botaoEquipaMang},
      {sistema.pressaoAtmosferica.nome, "[C]", bufC, 1, &botaoEquipaCam},
      {sistema.temperaturaAmbiente.nome, "[T]", bufT, 2, &botaoEquipaTemp},
  };

  for (int i = 0; i < 3; i++) {
    float yBtn = yBase + (2 - i) * (ALTURA_BTN + ESPACO_BTN);
    *botoes[i].zona = {xBase, yBtn, LARGURA_BTN, ALTURA_BTN};

    bool esteAEstabilizar = (sistema.fatorEmEstabilizacao == botoes[i].indice);
    bool outroAEstabilizar =
        (sistema.fatorEmEstabilizacao != -1 && !esteAEstabilizar);

    // Cor de fundo do botão
    if (esteAEstabilizar) {
      definirCor(0.18f, 0.16f, 0.02f); // amarelo escuro
    } else if (outroAEstabilizar) {
      definirCor(0.14f, 0.04f, 0.04f); // vermelho escuro
    } else {
      definirCor(0.08f, 0.10f, 0.13f); // cinzento disponível
    }
    desenharRetangulo(xBase, yBtn, LARGURA_BTN, ALTURA_BTN);

    // Borda
    if (esteAEstabilizar) {
      definirCor(0.85f, 0.72f, 0.10f);
    } else if (outroAEstabilizar) {
      definirCor(0.55f, 0.10f, 0.10f);
    } else {
      definirCor(0.25f, 0.35f, 0.42f);
    }
    desenharContorno(xBase, yBtn, LARGURA_BTN, ALTURA_BTN, 1.5f);

    // Texto — linha 1: nome do fator + tecla
    float yTexto1 = yBtn + ALTURA_BTN - 20.0f;
    float yTexto2 = yBtn + 8.0f;

    if (esteAEstabilizar) {
      definirCor(1.0f, 0.85f, 0.20f);
    } else if (outroAEstabilizar) {
      definirCor(0.55f, 0.22f, 0.22f);
    } else {
      definirCor(0.75f, 0.85f, 0.90f);
    }
    desenharTextoMedio(botoes[i].label, xBase + 12.0f, yTexto1);

    // Tecla alinhada à direita
    definirCor(0.45f, 0.78f, 0.45f);
    desenharTextoMedio(botoes[i].tecla, xBase + LARGURA_BTN - 42.0f, yTexto1);

    // Linha 2: valor atual ou estado
    if (esteAEstabilizar) {
      definirCor(1.0f, 0.85f, 0.20f);
      desenharTextoPequeno("A estabilizar...", xBase + 12.0f, yTexto2);
    } else if (outroAEstabilizar) {
      definirCor(0.55f, 0.22f, 0.22f);
      desenharTextoPequeno("Indisponivel", xBase + 12.0f, yTexto2);
    } else {
      definirCor(0.50f, 0.60f, 0.65f);
      desenharTextoPequeno(botoes[i].valor, xBase + 12.0f, yTexto2);
    }
  }
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
// Ecrã 2 — Controlo & Histórico
// ---------------------------------------------------------------------------
void desenharEcra2() {
  desenharBotoesEquipa();
  desenharPainelFatores();
  desenharPainelComandos();
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void display() {
  glClear(GL_COLOR_BUFFER_BIT);

  desenharTopbar();
  desenharBarraNavegacao();

  if (ecraoAtivo == 1) {
    desenharPainelSubstancias();
    desenharPainelFatores();
    desenharPainelComandos();
    desenharSnackbar();
  } else {
    desenharEcra2();
  }

  glutSwapBuffers();
}

// ---------------------------------------------------------------------------
// Teclado
// ---------------------------------------------------------------------------
void teclado(unsigned char key, int x, int y) {
  // Controlos exclusivos do ecrã 1
  if (ecraoAtivo == 1) {
    switch (key) {
    case 'a':
    case 'A':
      if (!sistema.sistemaTerminado && sistema.alfa < 100.0f) {
        sistema.alfa += 5.0f;
        if (sistema.alfa > 100.0f)
          sistema.alfa = 100.0f;
        sistema.beta = 100.0f - sistema.alfa;
        avaliarEstado();
        {
          char buf[128];
          snprintf(buf, sizeof(buf),
                   "Operador aumentou Alfa — Alfa: %.1f%% | Beta: %.1f%%",
                   sistema.alfa, sistema.beta);
          gravarLog(buf);
        }
        glutPostRedisplay();
      }
      break;
    case 'b':
    case 'B':
      if (!sistema.sistemaTerminado && sistema.beta < 100.0f) {
        sistema.beta += 5.0f;
        if (sistema.beta > 100.0f)
          sistema.beta = 100.0f;
        sistema.alfa = 100.0f - sistema.beta;
        avaliarEstado();
        {
          char buf[128];
          snprintf(buf, sizeof(buf),
                   "Operador aumentou Beta — Alfa: %.1f%% | Beta: %.1f%%",
                   sistema.alfa, sistema.beta);
          gravarLog(buf);
        }
        glutPostRedisplay();
      }
      break;
    }
  }

  // Controlos exclusivos do ecrã 2
  if (ecraoAtivo == 2) {
    switch (key) {
    case 'm':
    case 'M':
      chamarEquipa(0);
      glutPostRedisplay();
      break;
    case 'c':
    case 'C':
      chamarEquipa(1);
      glutPostRedisplay();
      break;
    case 't':
    case 'T':
      chamarEquipa(2);
      glutPostRedisplay();
      break;
    }
  }

  // Teclas globais — funcionam em qualquer ecrã
  switch (key) {
  case '1':
    ecraoAtivo = 1;
    glutPostRedisplay();
    break;
  case '2':
    ecraoAtivo = 2;
    glutPostRedisplay();
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

  inicializarLog();
  inicializarSistema();
  gravarLog("Sistema iniciado — Alfa: 50.0% | Beta: 50.0%");

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
  glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
  glutKeyboardFunc(teclado);
  glutMouseFunc(mouse);

  glutTimerFunc(1000, timerFatores, 0);
  glutTimerFunc(2000, timerSubstancias, 0);

  glutMainLoop();

  fecharLog();
  return 0;
}
