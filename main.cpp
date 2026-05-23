// =====================================================
// main.cpp — Mundo Invertido
// =====================================================
// Alunas: Eduarda Fernandes e Maria Eduarda Dias
//
// Ideia do jogo: um gatinho caminha por um cenário de verão (summer)
// até que, depois de alguns segundos, a tela "sobe" e ela entra num mundo
// invertido (graveyard), onde tudo aparece de cabeça pra baixo.
// O jogo usa parallax horizontal (camadas mais longe se movem mais devagar)
// e também parallax vertical durante o pulo (efeito de profundidade).

#include <iostream>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb_image.h"
#include "Layer.h"

const GLint  WIDTH  = 800;
const GLint  HEIGHT = 600;

// splitY é a linha que divide o mundo summer (em cima) do graveyard (embaixo)
// No início, ela fica no fundo da tela (splitY = HEIGHT), ou seja, só vemos o summer
// Quando entra no mundo invertido, ela sobe pra deixar só 15% de summer visível
const float  SPLIT_A     = (float)HEIGHT;
const float  SPLIT_B     = (float)HEIGHT * 0.15f;
const float  SPLIT_SPEED = (SPLIT_A - SPLIT_B) / 1.5f;

// Física do pulo
const float  GRAVITY        = 1100.0f;   // quanto mais alto, volta mais rápido
const float  JUMP_VEL       = 450.0f;    // força inicial do pulo
const float  PLAYER_SPEED   = 280.0f;    // velocidade horizontal da personagem
const float  TIME_IN_SUMMER = 15.0f;     // segundos até a queda automática

// Suaviza o quanto as camadas se movem verticalmente durante o pulo
const float  PARALLAX_Y_SOFT = 0.4f;

// As imagens originais são 576x324 (proporção 16:9)
// Estico pra 800px de largura e calculo a altura mantendo proporção
const float  LAYER_DRAW_W = (float)WIDTH;
const float  LAYER_DRAW_H = LAYER_DRAW_W * 324.0f / 576.0f;

// Tamanho do gato na tela
const float  PLAYER_SIZE   = 220.0f * 0.55f;
// Posição Y do "chão" do summer (onde personagem fica parado)
const float  CHAO_SUMMER_Y = (float)HEIGHT - LAYER_DRAW_H * 0.13f;
const float  PLAYER_Y_TOP  = CHAO_SUMMER_Y - PLAYER_SIZE;

// Estados do personagem
enum PlayerState { STANDING_TOP, JUMPING_TOP, STANDING_BOTTOM, JUMPING_BOTTOM };

// -----------------------------------------------------
// CARREGAR TEXTURA (usando stb_image)
// -----------------------------------------------------
bool load_texture(const char* file_name, unsigned int* tex) {
    int x, y, n;
    unsigned char* data = stbi_load(file_name, &x, &y, &n, 4);
    if (!data) { fprintf(stderr, "ERRO: nao carregou %s\n", file_name); return false; }

    glGenTextures(1, tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // GL_REPEAT no X pra fazer o efeito de loop infinito das camadas
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    stbi_image_free(data);
    return true;
}

// Cria uma layer já com a textura carregada
Layer makeLayer(const char* file, float z, float ratex) {
    Layer L;
    L.z = z; L.tid = 0; L.filename = (char*)file;
    L.offsetx = L.offsety = 0.0f;
    // ratex = velocidade de scroll horizontal
    // ratey = uso o mesmo valor pro parallax vertical
    L.ratex = ratex; L.ratey = ratex;
    load_texture(file, &L.tid);
    return L;
}

// -----------------------------------------------------
// Usa quad pra desenhar camadas e personagem,
// só mudando a matriz model pra cada caso
// -----------------------------------------------------
GLuint quadVAO = 0;
void setupQuad() {
    GLfloat v[] = {
        // posição (x,y,z)   texCoord (u,v)
        0,0,0, 0,0,  1,0,0, 1,0,  1,1,0, 1,1,
        0,0,0, 0,0,  1,1,0, 1,1,  0,1,0, 0,1,
    };
    GLuint VBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// -----------------------------------------------------
// SHADERS
// -----------------------------------------------------
// Vertex shader: aplica as 3 matrizes (projeção, view e model)
const char* VERTEX_SHADER =
"#version 410\n"
"layout(location=0) in vec3 vPos; layout(location=1) in vec2 vUV;"
"uniform mat4 proj, view, model; out vec2 uv;"
"void main(){ uv=vUV; gl_Position=proj*view*model*vec4(vPos,1.0); }";

// Fragment shader: pinta cada pixel com a cor da textura.
// Faz duas coisas extras:
//   - chromaKey: descarta pixels pretos (uso isso porque os PNGs do graveyard
//     têm fundo preto sólido em vez de transparência alpha)
const char* FRAGMENT_SHADER =
"#version 410\n"
"in vec2 uv; uniform sampler2D tex;"
"uniform vec2 frameSize, frameOffset;"
"uniform int chromaKey; out vec4 fragColor;"
"void main(){"
"  vec4 c=texture(tex, uv*frameSize+frameOffset);"
"  if(chromaKey==1 && c.r<0.02 && c.g<0.02 && c.b<0.02) discard;"
"  fragColor=c; }";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s,1024,NULL,log); fprintf(stderr,"Shader: %s\n",log); }
    return s;
}
GLuint makeProgram() {
    GLuint p = glCreateProgram();
    glAttachShader(p, compileShader(GL_VERTEX_SHADER,   VERTEX_SHADER));
    glAttachShader(p, compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER));
    glLinkProgram(p); return p;
}

// -----------------------------------------------------
// DESENHA UMA LAYER
// -----------------------------------------------------
// Cada camada é desenhada 3 vezes lado a lado pra dar o efeito de loop infinito
// quando a câmera anda. Usa fmodf pra calcular onde a "primeira cópia" começa.
//
// scrollX: deslocamento horizontal (player.x * ratex da camada)
// scrollY: deslocamento vertical (cameraY * ratey) - se passar 0, fica estática
// anchorY: onde a base da imagem deve encostar na tela
// drawH:   altura desenhada (pode ser diferente da altura original pra esticar)
void drawLayer(GLuint prog, const Layer& L,
               float scrollX, float scrollY,
               float anchorY, float drawH,
               bool useChroma, bool gray)
{
    float baseX = -fmodf(scrollX, LAYER_DRAW_W);
    if (baseX > 0) baseX -= LAYER_DRAW_W;

    float adjustedAnchor = anchorY + scrollY;
    float baseY = adjustedAnchor - drawH;

    for (int i = 0; i < 3; i++) {
        glm::mat4 model = glm::translate(glm::mat4(1),
                              glm::vec3(baseX + i * LAYER_DRAW_W, baseY, 0.f));
        model = glm::scale(model, glm::vec3(LAYER_DRAW_W, drawH, 1.f));
        glUniformMatrix4fv(glGetUniformLocation(prog,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniform2f(glGetUniformLocation(prog,"frameSize"),   1.f, 1.f);
        glUniform2f(glGetUniformLocation(prog,"frameOffset"), 0.f, 0.f);
        glUniform1i(glGetUniformLocation(prog,"chromaKey"), useChroma ? 1 : 0);
        glUniform1i(glGetUniformLocation(prog,"grayscale"),  gray     ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, L.tid);
        glUniform1i(glGetUniformLocation(prog,"tex"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

// -----------------------------------------------------
// PERSONAGEM
// -----------------------------------------------------
struct Player {
    GLuint texWalk;                        // sprite sheet
    float  x, y, vy, w, h;                 // posição, velocidade vertical, tamanho
    int    cols, currentCol;               // sprite sheet tem várias colunas (frames)
    double frameTime, frameRate;           // controle de animação
    bool   facingRight, moving;
    PlayerState state;
};

// Avança o frame da animação a cada frameRate segundos
void updatePlayerAnim(Player& p, double now) {
    if (p.moving) {
        if (now - p.frameTime > p.frameRate) {
            p.currentCol = (p.currentCol + 1) % p.cols;
            p.frameTime  = now;
        }
    } else { p.currentCol = 0; }
}

// Desenha a personagem na tela
void drawPlayer(GLuint prog, const Player& p, float drawX, bool flipV) {
    float sx = p.facingRight ? p.w : -p.w;
    float sy = flipV ? -p.h : p.h;
    float tx = p.facingRight ? drawX : drawX + p.w;
    float ty = flipV ? p.y + p.h : p.y;

    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(tx, ty, 0));
    model = glm::scale(model, glm::vec3(sx, sy, 1));

    // Recorta o frame atual da sprite sheet (cada frame ocupa 1/cols da textura)
    float fw = 1.f / p.cols;
    glUniformMatrix4fv(glGetUniformLocation(prog,"model"),1,GL_FALSE,glm::value_ptr(model));
    glUniform2f(glGetUniformLocation(prog,"frameSize"),   fw, 1.f);
    glUniform2f(glGetUniformLocation(prog,"frameOffset"), p.currentCol * fw, 0.f);
    glUniform1i(glGetUniformLocation(prog,"chromaKey"), 1);
    glUniform1i(glGetUniformLocation(prog,"grayscale"),  0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p.texWalk);
    glUniform1i(glGetUniformLocation(prog,"tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// =====================================================
// MAIN
// =====================================================
int main() {
    // Inicialização do GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Mundo Invertido", nullptr, nullptr);
    if (!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE; glewInit();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    setupQuad();
    GLuint prog = makeProgram();

    glm::mat4 proj      = glm::ortho(0.f,(float)WIDTH,(float)HEIGHT,0.f,-1.f,1.f);
    glm::mat4 viewIdent = glm::mat4(1);

    // -------------------------------------------------
    // CARREGAR AS CAMADAS DOS DOIS MUNDOS
    // ratex menor = camada se move mais devagar (mais longe)
    // ratex maior = camada se move mais rápido (mais perto)
    // -------------------------------------------------
    Layer summer1[4] = {
        makeLayer("summer1-1.png", 0.1f, 0.1f),  // céu (quase parado)
        makeLayer("summer1-2.png", 0.3f, 0.3f),  // montanhas
        makeLayer("summer1-3.png", 0.6f, 0.6f),  // vaca + árvore
        makeLayer("summer1-4.png", 1.0f, 1.0f),  // chão (acompanha a câmera)
    };
    Layer graveyard1[4] = {
        makeLayer("graveyard1-1.png", 0.1f, 0.1f),  // céu noturno com estrelas
        makeLayer("graveyard1-2.png", 0.3f, 0.3f),  // nuvens
        makeLayer("graveyard1-3.png", 0.6f, 0.6f),  // igreja + árvores
        makeLayer("graveyard1-4.png", 1.0f, 1.0f),  // cemitério (chão)
    };

    // -------------------------------------------------
    // PLAYER
    // -------------------------------------------------
    Player player;
    load_texture("orange-cat-run.png", &player.texWalk);
    player.cols = 6; player.h = PLAYER_SIZE; player.w = PLAYER_SIZE;
    player.x = 100.f; player.y = PLAYER_Y_TOP; player.vy = 0.f;
    player.currentCol = 0; player.frameTime = 0; player.frameRate = 0.1;
    player.facingRight = true; player.moving = false;
    player.state = STANDING_TOP;

    // splitY começa no fundo da tela = 100% summer visível
    float splitY      = SPLIT_A;
    float splitTarget = SPLIT_A;
    bool  wantsGrave  = false;  // já entrou no mundo invertido?
    float summerTimer = 0.f;    // tempo no summer

    float  cameraX = 0.f;
    double prevTime = glfwGetTime();
    bool   spacePrev = false;   // pra detectar quando a tecla foi pressionada

    // =================================================
    // LOOP PRINCIPAL
    // =================================================
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        double now = glfwGetTime();
        float  fdt = (float)(now - prevTime);  // delta time (segundos desde o último frame)
        prevTime   = now;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);

        // --- INPUT ---
        float dirX = 0.f;
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) dirX -= 1.f;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) dirX += 1.f;
        // Pulo: só dispara no instante que a tecla é apertada (não enquanto segura)
        bool spaceNow     = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        bool spacePressed = (spaceNow && !spacePrev);
        spacePrev = spaceNow;

        // --- ANIMAÇÃO DA DIVISÃO DOS MUNDOS ---
        // splitY se move suavemente em direção ao splitTarget
        if (splitY != splitTarget) {
            float dir = (splitTarget > splitY) ? 1.f : -1.f;
            splitY   += dir * SPLIT_SPEED * fdt;
            if (dir > 0 && splitY > splitTarget) splitY = splitTarget;
            if (dir < 0 && splitY < splitTarget) splitY = splitTarget;
        }
        bool animating = (splitY != splitTarget);

        // Depois de TIME_IN_SUMMER segundos, dispara a queda automática
        if (!wantsGrave && !animating) {
            summerTimer += fdt;
            if (summerTimer >= TIME_IN_SUMMER) {
                wantsGrave  = true;
                splitTarget = SPLIT_B;  // anima a divisão pra cima
                summerTimer = 0.f;
            }
        }

        // Calcula onde a personagem fica em pé no graveyard
        float CHAO_GRAVE_Y   = splitY + (HEIGHT - splitY) * 0.08f;
        float PLAYER_Y_GRAVE = CHAO_GRAVE_Y;

        // --- MOVIMENTO HORIZONTAL ---
        player.moving = (dirX != 0.f);
        if (dirX > 0) player.facingRight = true;
        if (dirX < 0) player.facingRight = false;
        player.x += dirX * PLAYER_SPEED * fdt;
        if (player.x < 0) player.x = 0;
        cameraX = player.x;

        // --- FÍSICA E ESTADOS DA PERSONAGEM ---
        // Durante a animação da transição entre mundos, a personagem fica parada
        if (animating) {
            player.vy    = 0.f;
            player.state = wantsGrave ? STANDING_BOTTOM : STANDING_TOP;
            player.y     = wantsGrave ? PLAYER_Y_GRAVE : PLAYER_Y_TOP;
        } else {
            switch (player.state) {
                case STANDING_TOP:
                    // Em pé no summer
                    player.y  = PLAYER_Y_TOP;
                    player.vy = 0.f;
                    // Pulo: velocidade pra cima (Y negativo)
                    if (spacePressed) { player.vy = -JUMP_VEL; player.state = JUMPING_TOP; }
                    break;
                case JUMPING_TOP:
                    // Aplica gravidade (puxa pra baixo)
                    player.vy += GRAVITY * fdt;
                    player.y  += player.vy * fdt;
                    // Voltou ao chão?
                    if (player.y >= PLAYER_Y_TOP) {
                        player.y = PLAYER_Y_TOP; player.vy = 0.f;
                        player.state = STANDING_TOP;
                    }
                    break;
                case STANDING_BOTTOM:
                    // Em pé no graveyard (de cabeça pra baixo)
                    player.y  = PLAYER_Y_GRAVE;
                    player.vy = 0.f;
                    // Pulo invertido: velocidade pra baixo da tela
                    if (spacePressed) { player.vy = +JUMP_VEL; player.state = JUMPING_BOTTOM; }
                    break;
                case JUMPING_BOTTOM:
                    // Gravidade invertida: puxa pra cima
                    player.vy -= GRAVITY * fdt;
                    player.y  += player.vy * fdt;
                    if (player.vy < 0.f && player.y <= PLAYER_Y_GRAVE) {
                        player.y = PLAYER_Y_GRAVE; player.vy = 0.f;
                        player.state = STANDING_BOTTOM;
                    }
                    break;
            }
        }

        updatePlayerAnim(player, now);

        // --- CAMERA Y (parallax vertical durante o pulo) ---
        // Calcula o quanto a personagem está acima do chão
        float groundSummer = PLAYER_Y_TOP;
        float groundGrave  = splitY;
        float cameraY = 0.f;
        if (!wantsGrave) {
            cameraY = (groundSummer - player.y) * PARALLAX_Y_SOFT;
        } else {
            cameraY = (player.y - groundGrave) * PARALLAX_Y_SOFT;
        }

        // --- DIVISÃO DOS MUNDOS DURANTE O PULO NO GRAVEYARD ---
        // No mundo invertido, em vez das camadas se mexerem, a divisão dos
        // mundos sobe na tela: o summer encolhe, o graveyard ocupa mais espaço
        // Isso dá a sensação de "câmera subindo" sem mover as imagens em si
        float renderSplitY = splitY;
        if (wantsGrave && (player.state == JUMPING_BOTTOM)) {
            renderSplitY = splitY - cameraY;
            if (renderSplitY < 0.f) renderSplitY = 0.f;
        }

        // --- DESLOCAMENTO DO SUMMER ---
        // Faz o summer "subir" pra mostrar a base (grama/chão) quando estamos
        // no estado B. Durante o pulo, soma o cameraY pra continuar mostrando
        // a base mesmo quando a faixa do summer encolhe
        float effectiveSummerShift = SPLIT_A - splitY;
        if (wantsGrave && (player.state == JUMPING_BOTTOM)) {
            effectiveSummerShift += cameraY;
        }

        // =============================================
        // RENDER
        // =============================================
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog,"proj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(prog,"view"),1,GL_FALSE,glm::value_ptr(viewIdent));
        glBindVertexArray(quadVAO);

        // Define regiões da tela pra cada mundo usando glScissor
        int sciSum_y = (int)(HEIGHT - renderSplitY);
        int sciSum_h = (int)renderSplitY;
        int sciGrv_y = 0;
        int sciGrv_h = (int)(HEIGHT - renderSplitY);

        // ---- DESENHA O SUMMER (parte de cima da tela) ----
        // A base do summer fica em summerAnchor (sobe quando entra no graveyard)
        float summerAnchor = (float)HEIGHT - effectiveSummerShift;

        if (sciSum_h > 0) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, sciSum_y, WIDTH, sciSum_h);

            // Céu: estático verticalmente (não acompanha o pulo)
            drawLayer(prog, summer1[0],
                      cameraX * summer1[0].ratex, 0.0f,
                      summerAnchor, (float)HEIGHT,
                      false, false);

            // Demais camadas: parallax vertical durante o pulo
            for (int i = 1; i < 4; i++) {
                drawLayer(prog, summer1[i],
                          cameraX * summer1[i].ratex, +cameraY,
                          summerAnchor, LAYER_DRAW_H,
                          true, false);
            }
            glDisable(GL_SCISSOR_TEST);
        }

        // ---- DESENHA O GRAVEYARD ----
        if (sciGrv_h > 0) {
            float invPivot = (renderSplitY + splitY) * 0.5f;
            glm::mat4 graveView = glm::mat4(1);
            graveView = glm::translate(graveView, glm::vec3(0.f, 2.f * invPivot, 0.f));
            graveView = glm::scale(graveView, glm::vec3(1.f, -1.f, 1.f));
            glUniformMatrix4fv(glGetUniformLocation(prog,"view"),1,GL_FALSE,glm::value_ptr(graveView));

            glEnable(GL_SCISSOR_TEST);
            glScissor(0, sciGrv_y, WIDTH, sciGrv_h);

            // Todas as camadas do graveyard são estáticas verticalmente
            // (só se movem horizontalmente). O efeito do pulo aqui é só
            // a divisão dos mundos subindo

            drawLayer(prog, graveyard1[0],
                      cameraX * graveyard1[0].ratex, 0.0f,
                      splitY, (float)HEIGHT, false, false);

            drawLayer(prog, graveyard1[1],
                      cameraX * graveyard1[1].ratex, 0.0f,
                      splitY, LAYER_DRAW_H, true, false);

            for (int i = 2; i < 4; i++) {
                drawLayer(prog, graveyard1[i],
                          cameraX * graveyard1[i].ratex, 0.0f,
                          splitY, LAYER_DRAW_H, true, false);
            }

            glDisable(GL_SCISSOR_TEST);

            glUniformMatrix4fv(glGetUniformLocation(prog,"view"),1,GL_FALSE,glm::value_ptr(viewIdent));
        }

        // ---- DESENHA O PLAYER ----
        // Se está no graveyard, desenha invertido verticalmente
        bool flipped = (player.state == STANDING_BOTTOM || player.state == JUMPING_BOTTOM);
        float drawX  = (float)WIDTH * 0.5f - player.w * 0.5f;
        drawPlayer(prog, player, drawX, flipped);

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return EXIT_SUCCESS;
}
