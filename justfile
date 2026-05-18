build:
	g++ -o main main.cpp -lGL -lGLU -lglut

run: build
	./main
