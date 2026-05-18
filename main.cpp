#include <GL/freeglut.h>
#include <cstdio>

// Dimensões da Janela
const int LARGURA_JANELA = 800;
const int ALTURA_JANELA = 600;

void display() {
  glClear(GL_COLOR_BUFFER_BIT);
  glutSwapBuffers();
}

void teclado(unsigned char key, int x, int y) {
  switch (key) {
  case 'b':
  case 'B':
    printf("Beta!\n");
    break;
  case 'a':
  case 'A':
    printf("Alfa!\n");
    break;
  case 'h':
  case 'H':
    printf("Ajuda!\n");
    break;
  }
}

int main(int argc, char **argv) {
  // Inicializa GLUT
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(LARGURA_JANELA, ALTURA_JANELA);

  // Centraliza a janela no ecrã
  int larguraEcra = glutGet(GLUT_SCREEN_WIDTH);
  int alturaEcra = glutGet(GLUT_SCREEN_HEIGHT);
  glutInitWindowPosition((larguraEcra - LARGURA_JANELA) / 2,
                         (alturaEcra - ALTURA_JANELA) / 2);

  glutCreateWindow("Space Dilemma");

  glutDisplayFunc(display);
  glutKeyboardFunc(teclado);

  glutMainLoop();
  return 0;
}
