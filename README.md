# Mundo Invertido

Projeto desenvolvido em C++ utilizando OpenGL, GLFW e GLEW.

O jogo apresenta dois mundos conectados:

* **Summer** → cenário de verão
* **Graveyard** → mundo invertido e sombrio

Um gatinho começa caminhando pelo mundo summer e, após alguns segundos, a tela realiza uma transição para o mundo invertido, criando um efeito visual de espelhamento entre os cenários.

---

## Autoras

* Eduarda Fernandes
* Maria Eduarda Dias

---

## Funcionalidades

* Parallax horizontal
* Parallax vertical durante o pulo
* Transição animada entre os mundos
* Mundo invertido com renderização espelhada
* Animação de sprite da personagem
* Loop infinito dos cenários

---

## Controles

| Tecla  | Ação          |
| ------ | ------------- |
| ← / →  | Movimentar    |
| Espaço | Pular         |
| ESC    | Fechar o jogo |

---

## Tecnologias Utilizadas

* C++
* OpenGL
* GLFW
* GLEW

---

## Estrutura Esperada dos Arquivos

Os assets devem estar no mesmo diretório do executável:

```text
summer1-1.png
summer1-2.png
summer1-3.png
summer1-4.png

graveyard1-1.png
graveyard1-2.png
graveyard1-3.png
graveyard1-4.png

orange-cat-run.png

stb_image.h
stb_image.cpp
Layer.h
main.cpp
```

---

## Compilação

### Windows (MinGW)

```bash
g++ main.cpp stb_image.cpp -o jogo-av2.exe -lglfw3 -lglew32 -lopengl32 -lgdi32
```

### Linux

```bash
g++ main.cpp stb_image.cpp -o jogo-av2 -lglfw -lGLEW -lGL
```

### macOS

```bash
g++ main.cpp stb_image.cpp -o jogo-av2 -lglfw -lGLEW -framework OpenGL
```

---

## Execução

### Windows

```bash
jogo-av2.exe
```

### Linux/macOS

```bash
./jogo-av2
```

---

## Observações

O projeto foi desenvolvido com foco em:

* efeitos visuais utilizando parallax;
* manipulação de câmera;
* renderização em OpenGL;
* transições entre cenários;
* uso de sprite sheets e animações 2D.
