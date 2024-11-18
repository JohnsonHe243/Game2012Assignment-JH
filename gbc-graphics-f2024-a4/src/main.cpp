#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Mesh.h"
#include "Math.h"
#include <stb_image.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;
constexpr float SCREEN_ASPECT = SCREEN_WIDTH / (float)SCREEN_HEIGHT;

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void error_callback(int error, const char* description);

GLuint CreateShader(GLint type, const char* path);
GLuint CreateProgram(GLuint vs, GLuint fs);

std::array<int, GLFW_KEY_LAST> gKeysCurr{}, gKeysPrev{};
bool IsKeyDown(int key);
bool IsKeyUp(int key);
bool IsKeyPressed(int key);

void Print(Matrix m);

enum Projection : int
{
    ORTHO,  // Orthographic, 2D
    PERSP   // Perspective,  3D
};

struct Pixel
{
    stbi_uc r = 255;
    stbi_uc g = 255;    
    stbi_uc b = 255;
    stbi_uc a = 255;
};

int main(void)
{
    glfwSetErrorCallback(error_callback);
    assert(glfwInit() == GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
#ifdef NDEBUG
#else
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graphics 1", NULL, NULL);
    glfwMakeContextCurrent(window);
    assert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress));
    glfwSetKeyCallback(window, key_callback);

#ifdef NDEBUG
#else
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugOutput, nullptr);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Vertex shaders:
    GLuint vs = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/default.vert");
    GLuint vsPoints = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/points.vert");
    GLuint vsLines = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/lines.vert");
    GLuint vsVertexPositionColor = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/vertex_color.vert");
    GLuint vsColorBufferColor = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/buffer_color.vert");
    
    // Fragment shaders:
    GLuint fsLines = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/lines.frag");
    GLuint fsUniformColor = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/uniform_color.frag");
    GLuint fsVertexColor = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/vertex_color.frag");
    GLuint fsTcoords = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/tcoord_color.frag");
    GLuint fsNormals = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/normal_color.frag");
    GLuint fsTexture = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/texture_color.frag");
    GLuint fsTextureMix = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/texture_color_mix.frag");

    // Shader programs:
    GLuint shaderUniformColor = CreateProgram(vs, fsUniformColor);
    GLuint shaderVertexPositionColor = CreateProgram(vsVertexPositionColor, fsVertexColor);
    GLuint shaderVertexBufferColor = CreateProgram(vsColorBufferColor, fsVertexColor);
    GLuint shaderPoints = CreateProgram(vsPoints, fsVertexColor);
    GLuint shaderLines = CreateProgram(vsLines, fsLines);
    GLuint shaderTcoords = CreateProgram(vs, fsTcoords);
    GLuint shaderNormals = CreateProgram(vs, fsNormals);
    GLuint shaderTexture = CreateProgram(vs, fsTexture);
    GLuint shaderTextureMix = CreateProgram(vs, fsTextureMix);

    // Define vertices for a simple trapezoid in normalized coordinates
    GLfloat vertices[] = {
        // Top side
        -0.3f,  0.3f, 0.0f,   // Top Left
         0.3f,  0.3f, 0.0f,   // Top Right
         // Bottom side
         -0.5f, -0.3f, 0.0f,   // Bottom Left
          0.5f, -0.3f, 0.0f    // Bottom Right
    };

    int texGradientWidth = 256;
    int texGradientHeight = 256;
    Pixel* pixelsGradient = (Pixel*)malloc(texGradientWidth * texGradientHeight * sizeof(Pixel));
    for (int y = 0; y < texGradientHeight; y++)
    {
        for (int x = 0; x < texGradientWidth; x++)
        {
            float u = x / (float)texGradientWidth;
            float v = y / (float)texGradientHeight;       
        
            Pixel pixel;
            pixel.r = u * 255.0f;
            pixel.g = v * 255.0f;
            pixel.b = 255;
            pixel.a = 255;

            pixelsGradient[y * texGradientWidth + x] = pixel;

        }
    }

    // obj file defiens tcoords as 0 = bottom, 1 = top, but OpenGL defines as 0 = top, 1 = bottom.
    // Flips image vertically.
    stbi_set_flip_vertically_on_load(true);

    // Step 1: Load image from disk to CPU
    int texHeadWidth = 0;
    int texHeadHeight = 0;
    int texHeadChannels = 0;
    stbi_uc* pixelsHead = stbi_load("./assets/textures/head.png", &texHeadWidth, &texHeadHeight, &texHeadChannels, 0);

    // Step 2: Upload image from CPU to GPU
    GLuint texHead = GL_NONE;
    glGenTextures(1, &texHead);
    glBindTexture(GL_TEXTURE_2D, texHead);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texHeadWidth, texHeadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsHead);

    int texCT4Width = 0;
    int texCT4Height = 0;
    int texCT4Channels = 0;
    stbi_uc* pixelsCT4 = stbi_load("./assets/textures/ct4_grey.png", &texCT4Width, &texCT4Height, &texCT4Channels, 0);

    GLuint texCT4 = GL_NONE;
    glGenTextures(1, &texCT4);
    glBindTexture(GL_TEXTURE_2D, texCT4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texCT4Width, texCT4Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsCT4);

    int texDiceWidth = 0;
    int texDiceHeight = 0;
    int texDiceChannels = 0;
    stbi_uc* pixelsDice = stbi_load("./assets/textures/dice.png", &texDiceWidth, &texDiceHeight, &texDiceChannels, 0);

    GLuint texDice = GL_NONE;
    glGenTextures(1, &texDice);
    glBindTexture(GL_TEXTURE_2D, texDice);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texDiceWidth, texDiceHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsDice);
    free(pixelsDice);
    pixelsDice = nullptr;

    GLuint texGradient = GL_NONE;
    glGenTextures(1, &texGradient);
    glBindTexture(GL_TEXTURE_2D, texGradient);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texGradientWidth, texGradientHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsGradient);
    free(pixelsHead);
    pixelsHead = nullptr;

   
    int object = 0;
    printf("Object %i\n", object + 1);
    Projection projection = PERSP;
    Vector3 camPos{ 0.0f, 0.0f, 5.0f };
    float fov = 90.0f * DEG2RAD;
    float left = -1.0f;
    float right = 1.0f;
    float top = 1.0f;
    float bottom = -1.0f;
    float near = 1.0f; // 1.0 for testing purposes. Usually 0.1f or 0.01f
    float far = 10.0f;

    // Whether we render the imgui demo widgets
    bool imguiDemo = false;
    bool texToggle = false;
    bool camToggle = true;

    Mesh shapeMesh, objMesh, ct4Mesh, cubeMesh;
    CreateMesh(&shapeMesh, PLANE);
    CreateMesh(&objMesh, "assets/meshes/head.obj");
    CreateMesh(&ct4Mesh, "assets/meshes/ct4.obj");
    CreateMesh(&cubeMesh, "assets/meshes/cube.obj");

    float Pitch = 0.0f;
    float Yaw = 0.0f;
    Vector3 objectPosition = V3_ZERO;
    float objectSpeed = 30.0f;

    // Render looks weird cause this isn't enabled, but its causing unexpected problems which I'll fix soon!
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    float timePrev = glfwGetTime();
    float timeCurr = glfwGetTime();
    float dt = 0.0f;
    double pmx = 0.0, pmy = 0.0, mx = 0.0, my = 0.0;
    // Positions of our triangle's vertices (CCW winding-order)
    Vector3 positions[] =
    {
        0.5f, -0.5f, 0.0f,  // vertex 1 (bottom-right)
        0.0f, 0.5f, 0.0f,   // vertex 2 (top-middle)
        -0.5f, -0.5f, 0.0f  // vertex 3 (bottom-left)
    };

    // Colours of our triangle's vertices (xyz = rgb)
    Vector3 colours[] =
    {
        1.0f, 0.0f, 0.0f,   // vertex 1
        0.0f, 1.0f, 0.0f,   // vertex 2
        0.0f, 0.0f, 1.0f    // vertex 3
    };

    Vector2 curr[4]
    {
        { -0.3f,  0.3f },   // top-left
        {  0.3f,  0.3f },   // top-right
        {  0.3f, -0.3f },   // bot-right
        { -0.3f, -0.3f }    // bot-left
    };

    GLuint vao, pbo, cbo;       // pbo = "position buffer object", "cbo = color buffer object"
    glGenVertexArrays(1, &vao); // Allocate a vao handle
    glBindVertexArray(vao);     // Bind = "associate all bound buffer object with the current array object"

    // Create position buffer:
    glGenBuffers(1, &pbo);              // Allocate a vbo handle
    glBindBuffer(GL_ARRAY_BUFFER, pbo); // Associate this buffer with the bound vertex array
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(Vector3), positions, GL_STATIC_DRAW);  // Upload the buffer
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), 0);          // Describe the buffer
    glEnableVertexAttribArray(0);

    // Create color buffer:
    glGenBuffers(1, &cbo);              // Allocate a vbo handle
    glBindBuffer(GL_ARRAY_BUFFER, cbo); // Associate this buffer with the bound vertex array
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(Vector3), colours, GL_STATIC_DRAW);    // Upload the buffer
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), 0);          // Describe the buffer
    glEnableVertexAttribArray(1);

    GLuint vaoLines, pboLines, cboLines;
    glGenVertexArrays(1, &vaoLines);
    glBindVertexArray(vaoLines);

    glGenBuffers(1, &pboLines);
    glBindBuffer(GL_ARRAY_BUFFER, pboLines);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vector2), curr, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vector2), nullptr);
    glEnableVertexAttribArray(0);

    GLint u_color = glGetUniformLocation(shaderUniformColor, "u_color");
    GLint u_intensity = glGetUniformLocation(shaderUniformColor, "u_intensity");


    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        float time = glfwGetTime();
        timePrev = time;

        pmx = mx; pmy = my;
        glfwGetCursorPos(window, &mx, &my);
        mx = mx;
        Vector3 mouseDelta = { mx - pmx, my - pmy };
        //printf("x: &f, y: &f\n", mouseDelta.x, mouseDelta.y);
        //mouseDelta = Normalize(mouseDelta)

        // Change object when space is pressed
        if (IsKeyPressed(GLFW_KEY_TAB))
        {
            ++object %= 5;
            printf("Object %i\n", object + 1);
        }
        if (IsKeyPressed(GLFW_KEY_I))
            imguiDemo = !imguiDemo;
        if (IsKeyPressed(GLFW_KEY_T))
            texToggle = !texToggle;
        if (IsKeyPressed(GLFW_KEY_C))
        {
            camToggle = !camToggle;
            if (camToggle)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

        //Matrix objectRotation = ToMatrix(FromEuler(0.0f, 0.0f, 0.0f));
        //Matrix objectTranslation = Translate(objectPosition);
        //Matrix objectMatrix = objectRotation * objectTranslation;
        
        float camMove = objectSpeed * dt;
        float mouseScale = 0.5f;
        Pitch += mouseDelta.y * mouseScale;
        Yaw += mouseDelta.x * mouseScale;
        Pitch = fmax(fmin(Pitch, 89.0f), -89.0f);  // Clamp tighter to avoid near-alignment


        Vector3 camForward = 
        {
            cosf(Pitch * DEG2RAD) * cosf(Yaw * DEG2RAD),
            sinf(Pitch * DEG2RAD),
            cosf(Pitch * DEG2RAD) * sinf(Yaw * DEG2RAD)
        };        
        camForward = Normalize(camForward);

        Vector3 camRight = Normalize(Cross(V3_UP, camForward));
        Vector3 camUp = Cross(camForward, camRight);

        if (!camToggle)
        {
            camMove = 0.0f;
            mouseScale = 0.0f;
        }

        
        if (IsKeyDown(GLFW_KEY_W))
        {
            camPos += camForward * camMove;

        }        
        if (IsKeyDown(GLFW_KEY_S))
        {
            camPos -= camForward * camMove;
        }
        if (IsKeyDown(GLFW_KEY_D))
        {
            camPos -= camRight * camMove;
        }
        if (IsKeyDown(GLFW_KEY_A))
        {
            camPos += camRight * camMove;
        }        
        if (IsKeyDown(GLFW_KEY_SPACE))
        {
            camPos.y -= camMove;
        }
        if (IsKeyDown(GLFW_KEY_LEFT_SHIFT))
        {
            camPos.y += camMove;
        }



        // View-Matrix is the inverse of the camera matrix.
        // ex: when we move cam left, obj moves right.
        // ex: when we move cam up, obj moves down.
        // ex: when we rotate cam left, obj move right.
        // ex: when we rotate cam up, obj move down.
        Matrix matrixScale;
        Matrix world = MatrixIdentity();
        Matrix view = LookAt(camPos, camPos + camForward, camUp);
        Matrix proj = projection == ORTHO ? Ortho(left, right, bottom, top, near, far) : Perspective(fov, SCREEN_ASPECT, near, far);
        Matrix mvp;
        GLint u_mvp = GL_NONE;
        GLint u_tex = GL_NONE;
        GLint u_textureSlots[2];
        GLuint shaderProgram = GL_NONE;
        GLuint texture = texToggle ? texGradient : texHead;
        GLint u_t = GL_NONE;

        Matrix rotationY = RotateY(100.0f * time * DEG2RAD);
        //Matrix rotationX = RotateX(objectYaw * DEG2RAD);
/*        Matrix rotationY = RotateY(Yaw * DEG2RAD);
        Matrix rotationX = RotateX(Pitch * DEG2RAD);
        view = view * rotationX * rotationY; *///* Translate(-camPos.x,-camPos.y,-camPos.z);
        // world = objectMatrix;
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Interpolation parameter (0 means fully A, 1 means fully B)

        float a = cosf(time) * 0.5f + 0.5f;

        // Interpolate scale
        Vector3 sA = V3_ONE;
        Vector3 sB = V3_ONE * 10.0f;
        Vector3 sC = Lerp(sA, sB, a);

        // Interpolate rotation (slerp = "spherical lerp" because we rotate in a circle) 
        Quaternion rA = QuaternionIdentity();
        Quaternion rB = FromEuler(0.0f, 0.0f, 90.0f * DEG2RAD);
        Quaternion rC = Slerp(rA, rB, a);

        // Interpolate translation
        Vector3 tA = { -10.0f, 0.0f, 0.0f };
        Vector3 tB = { 10.0f, 0.0f, 0.0f };
        Vector3 tC = Lerp(tA, tB, a);

        // Interpolate color
        Vector3 cA = V3_UP;
        Vector3 cB = V3_FORWARD;
        Vector3 cC = Lerp(cA, cB, a);

        Matrix s = Scale(sC);
        Matrix r = ToMatrix(rC);
        Matrix t = Translate(tC);

        switch (object + 1)
        {
        case 1:
            shaderProgram = shaderNormals;
            glUseProgram(shaderProgram);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            DrawMesh(objMesh);
            break;

        case 2:
            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texHead);
            DrawMesh(objMesh);
            break;

        case 3:
            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            view = rotationY * view;
            world = world * rotationY * Translate(+2.5f,0.0f,0.0f);
            matrixScale = Scale(0.2f, 0.2f, 0.2f);
            mvp = matrixScale * world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texCT4);
            DrawMesh(ct4Mesh);
            break;

        case 4:
            view = LookAt({ 0.0f, 0.0f, 5.0f }, V3_ZERO, V3_UP);
            proj = Ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 10.0f);

            world = s * r * t;
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");

            shaderProgram = shaderLines;
            glUseProgram(shaderProgram);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_a"), a);
            glLineWidth(3.0f);
            glBindVertexArray(vaoLines);

            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(Vector2), curr);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            break;

        case 5:
            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            matrixScale = Scale(3.0f, 3.0f, 3.0f);
            mvp = matrixScale * world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texDice);
            DrawMesh(cubeMesh);
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (imguiDemo)
            ImGui::ShowDemoWindow();
        else
        {
            ImGui::SliderFloat3("Camera Position", &camPos.x, -10.0f, 10.0f);

            ImGui::RadioButton("Orthographic", (int*)&projection, 0); ImGui::SameLine();
            ImGui::RadioButton("Perspective", (int*)&projection, 1);

            ImGui::SliderFloat("Near", &near, -10.0f, 10.0f);
            ImGui::SliderFloat("Far", &far, -10.0f, 10.0f);
            if (projection == ORTHO)
            {
                ImGui::SliderFloat("Left", &left, -1.0f, -10.0f);
                ImGui::SliderFloat("Right", &right, 1.0f, 10.0f);
                ImGui::SliderFloat("Top", &top, 1.0f, 10.0f);
                ImGui::SliderFloat("Bottom", &bottom, -1.0f, -10.0f);
            }
            else if (projection == PERSP)
            {
                ImGui::SliderAngle("FoV", &fov, 10.0f, 90.0f);
            }
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        timeCurr = glfwGetTime();
        dt = timeCurr - timePrev;

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll and process events */
        memcpy(gKeysPrev.data(), gKeysCurr.data(), GLFW_KEY_LAST * sizeof(int));
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Disable repeat events so keys are either up or down
    if (action == GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    gKeysCurr[key] = action;

    // Key logging
    //const char* name = glfwGetKeyName(key, scancode);
    //if (action == GLFW_PRESS)
    //    printf("%s\n", name);
}

void error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

// Compile a shader
GLuint CreateShader(GLint type, const char* path)
{
    GLuint shader = GL_NONE;
    try
    {
        // Load text file
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(path);

        // Interpret the file as a giant string
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();

        // Verify shader type matches shader file extension
        const char* ext = strrchr(path, '.');
        switch (type)
        {
        case GL_VERTEX_SHADER:
            assert(strcmp(ext, ".vert") == 0);
            break;

        case GL_FRAGMENT_SHADER:
            assert(strcmp(ext, ".frag") == 0);
            break;
        default:
            assert(false, "Invalid shader type");
            break;
        }

        // Compile text as a shader
        std::string str = stream.str();
        const char* src = str.c_str();
        shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        // Check for compilation errors
        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cout << "Shader failed to compile: \n" << infoLog << std::endl;
        }
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "Shader (" << path << ") not found: " << e.what() << std::endl;
        assert(false);
    }

    return shader;
}

// Combine two compiled shaders into a program that can run on the GPU
GLuint CreateProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        program = GL_NONE;
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    return program;
}

// Graphics debug callback
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
    // ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    } std::cout << std::endl;

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    } std::cout << std::endl;

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    } std::cout << std::endl;
    std::cout << std::endl;
}

bool IsKeyDown(int key)
{
    return gKeysCurr[key] == GLFW_PRESS;
}

bool IsKeyUp(int key)
{
    return gKeysCurr[key] == GLFW_RELEASE;
}

bool IsKeyPressed(int key)
{
    return gKeysPrev[key] == GLFW_PRESS && gKeysCurr[key] == GLFW_RELEASE;
}

void Print(Matrix m)
{
    printf("%f %f %f %f\n", m.m0, m.m4, m.m8, m.m12);
    printf("%f %f %f %f\n", m.m1, m.m5, m.m9, m.m13);
    printf("%f %f %f %f\n", m.m2, m.m6, m.m10, m.m14);
    printf("%f %f %f %f\n\n", m.m3, m.m7, m.m11, m.m15);
}

