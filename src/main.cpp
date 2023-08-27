#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include "object.h"

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
bool normal = false;

void renderScene(Shader *shader);
vector<Object *> objects;

unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};
vector<SpotLight> spotLights(2);

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 backpackPosition = glm::vec3(0.0f);
    float backpackScale = 1.0f;
    PointLight pointLight;
    ProgramState() : camera(glm::vec3(0.0f, 10.0f, -8.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    //stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    //programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//    glEnable(GL_CULL_FACE);

    // build and compile shaders
    // -------------------------
    Shader simpleShader("resources/shaders/model_lighting.vs", "resources/shaders/model_lighting.fs");
    Shader simpleDepthShader("resources/shaders/3.2.1.point_shadows_depth.vs", "resources/shaders/3.2.1.point_shadows_depth.fs", "resources/shaders/3.2.1.point_shadows_depth.gs");
    Shader skyboxShader("resources/shaders/6.1.skybox.vs", "resources/shaders/6.1.skybox.fs");
    Shader normalShader("resources/shaders/normal.vs", "resources/shaders/normal.fs");

    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    unsigned int depthCubemap;
    glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                     SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    
    // load models
    // -----------
    Object castle;
    castle.setModel(new Model("resources/objects/castle/Castle OBJ.obj"));
    castle.setScale(glm::vec3(0.25));
//    objects.push_back(&castle);

    Object henri;
    henri.setModel(new Model("resources/objects/henri/stegosaurus.obj"));
    henri.setScale(glm::vec3(0.007));
    henri.translate(glm::vec3(-14.0, 17, 400.0));
    objects.push_back(&henri);
    int len = 0, stone_wait = 0;
    glm::vec3 henri_curr = henri.getPosition(), henri_init = henri.getPosition();
    auto center = henri.getPosition() + glm::vec3(200.0f, 0.0f, 500.0f);
    float radius = 200.0f;
    float curr = 0.0f, total = 100.0f;
    float curr1 = 0.0f, total1 = 100.0f;
    float curr2 = 0.0f, total2 = 100.0f;
    float curr3 = 0.0f, total3 = 100.0f;

    Object tank;
    tank.setModel(new Model("resources/objects/tank/T34.vox.obj"));
    tank.setScale(glm::vec3(0.4));
    tank.rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-135.0f), glm::vec3(0.0, 1.0, 0.0)));
    tank.translate(glm::vec3(2, 0.1, 5));
    objects.push_back(&tank);

    Object tree_bare;
    tree_bare.setModel(new Model("resources/objects/trees/Trunk_3.obj"));
    tree_bare.setScale(glm::vec3(0.6));
    tree_bare.translate(glm::vec3(5, 0, 0));
    objects.push_back(&tree_bare);

    Object tree;
    tree.setModel(new Model("resources/objects/trees/Tree_3.obj"));
    tree.setScale(glm::vec3(0.6));
    tree.translate(glm::vec3(-7, 0, 13));
    objects.push_back(&tree);

    Object trunk;
    trunk.setModel(new Model("resources/objects/trees/Log_5.obj"));
    trunk.setScale(glm::vec3(0.6));
    trunk.rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-135.0f), glm::vec3(0.0, 1.0, 0.0)));
    trunk.translate(glm::vec3(12, 0, -7));
    objects.push_back(&trunk);

    Object rock;
    rock.setModel(new Model("resources/objects/rock/Rock1.obj"));
    rock.setScale(glm::vec3(0.6));
    rock.translate(glm::vec3(9, 0, 13));
    objects.push_back(&rock);


    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3( 0.0f, 5.0, 5.0);
    pointLight.ambient = glm::vec3(1.0);
    pointLight.diffuse = glm::vec3(3.0);
    pointLight.specular = glm::vec3(1.0);

    pointLight.constant = 1.0f;
    pointLight.linear = 0.09f;
    pointLight.quadratic = 0.032f;

    // spotlights
    glm::vec3 origin = glm::vec3(0.0f, 6.0f, -4.0f);
    glm::vec3 pos0 = glm::vec3(8.0, 3.0, 1.0);
    glm::vec3 pos1 = glm::vec3(-8.0, 3.0, 1.0);
    glm::vec3 dir1 = normalize(origin - pos1);
    glm::vec3 dir0 = normalize(origin - pos0);

    spotLights[0].position = pos0;
    spotLights[0].direction = dir0;
    spotLights[0].ambient = glm::vec3(0.0f, 0.0f, 1.0f);
    spotLights[0].diffuse = glm::vec3(1.0f);
    spotLights[0].specular = glm::vec3(0.0f, 0.5f, 0.0f);
    spotLights[0].constant = 1.0;
    spotLights[0].linear = 0.09;
    spotLights[0].quadratic = 0.032;
    spotLights[0].cutOff = glm::cos(glm::radians(45.0f));
    spotLights[0].outerCutOff = glm::cos(glm::radians(60.0f));

    spotLights[1].position = pos1;
    spotLights[1].direction = dir1;
    spotLights[1].ambient = glm::vec3(0.0f, 0.0f, 1.0f);
    spotLights[1].diffuse = glm::vec3(1.0f);
    spotLights[1].specular = glm::vec3(0.0f, 0.5f, 0.0f);
    spotLights[1].constant = 1.0;
    spotLights[1].linear = 0.09;
    spotLights[1].quadratic = 0.032;
    spotLights[1].cutOff = glm::cos(glm::radians(45.0f));
    spotLights[1].outerCutOff = glm::cos(glm::radians(60.0f));


    // load skybox
    vector<std::string> faces
    {
            FileSystem::getPath("resources/textures/skybox/right.jpg"),
            FileSystem::getPath("resources/textures/skybox/left.jpg"),
            FileSystem::getPath("resources/textures/skybox/top.jpg"),
            FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
            FileSystem::getPath("resources/textures/skybox/front.jpg"),
            FileSystem::getPath("resources/textures/skybox/back.jpg")
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);



    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        auto currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();

        // input
        // -----
        processInput(window);


        // render
        // ------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!normal) {
            // 0. create depth cubemap transformation matrices
            // -----------------------------------------------
            float near_plane = 1.0f;
            float far_plane = 25.0f;
            glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT, near_plane, far_plane);
            std::vector<glm::mat4> shadowTransforms;
            shadowTransforms.push_back(shadowProj * glm::lookAt(pointLight.position, pointLight.position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(pointLight.position, pointLight.position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(pointLight.position, pointLight.position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(pointLight.position, pointLight.position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(pointLight.position, pointLight.position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(pointLight.position, pointLight.position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

            // 1. render scene to depth cubemap
            // --------------------------------
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            simpleDepthShader.use();
            for (unsigned int i = 0; i < 6; ++i)
                simpleDepthShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
            simpleDepthShader.setFloat("far_plane", far_plane);
            simpleDepthShader.setVec3("lightPos", pointLight.position);
            simpleDepthShader.setMat4("projection", projection);
            simpleDepthShader.setMat4("view", view);
            castle.render(&simpleDepthShader);
            renderScene(&simpleDepthShader);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // 2. render scene as normal
            // -------------------------
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            simpleShader.use();
            simpleShader.setInt("depthMap", 1);
            simpleShader.setFloat("far_plane", far_plane);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);


            simpleShader.setVec3("pointLight.position", pointLight.position);
            simpleShader.setVec3("pointLight.ambient", pointLight.ambient);
            simpleShader.setVec3("pointLight.diffuse", pointLight.diffuse);
            simpleShader.setVec3("pointLight.specular", pointLight.specular);
            simpleShader.setFloat("pointLight.constant", pointLight.constant);
            simpleShader.setFloat("pointLight.linear", pointLight.linear);
            simpleShader.setFloat("pointLight.quadratic", pointLight.quadratic);
            simpleShader.setVec3("viewPosition", programState->camera.Position);
            simpleShader.setFloat("material.shininess", 32.0f);
            simpleShader.setMat4("projection", projection);
            simpleShader.setMat4("view", view);

            for (int i = 0; i < 2; i++) {
                string sl = "spotLights[" + to_string(i) + "].";
                simpleShader.setVec3 (sl + "position", spotLights[i].position);
                simpleShader.setVec3 (sl + "direction", spotLights[i].direction);
                simpleShader.setVec3 (sl + "ambient", spotLights[i].ambient);
                simpleShader.setVec3 (sl + "diffuse", spotLights[i].diffuse);
                simpleShader.setVec3 (sl + "specular", spotLights[i].specular);
                simpleShader.setFloat(sl + "constant", spotLights[i].constant);
                simpleShader.setFloat(sl + "linear", spotLights[i].linear);
                simpleShader.setFloat(sl + "quadratic", spotLights[i].quadratic);
                simpleShader.setFloat(sl + "cutOff", spotLights[i].cutOff);
                simpleShader.setFloat(sl + "outerCutOff", spotLights[i].outerCutOff);
            }


            glDisable(GL_CULL_FACE);
            castle.render(&simpleShader);
            glEnable(GL_CULL_FACE);
            renderScene(&simpleShader);
        }
        else {
            normalShader.use();
            normalShader.setVec3("pointLight.position", pointLight.position);
            normalShader.setVec3("pointLight.ambient", pointLight.ambient);
            normalShader.setVec3("pointLight.diffuse", pointLight.diffuse);
            normalShader.setVec3("pointLight.specular", pointLight.specular);
            normalShader.setFloat("pointLight.constant", pointLight.constant);
            normalShader.setFloat("pointLight.linear", pointLight.linear);
            normalShader.setFloat("pointLight.quadratic", pointLight.quadratic);
            normalShader.setVec3("viewPosition", programState->camera.Position);
            normalShader.setFloat("material.shininess", 32.0f);
            normalShader.setMat4("projection", projection);
            normalShader.setMat4("view", view);
            normalShader.setVec3("lightPos", pointLight.position);
            for (int i = 0; i < 2; i++) {
                string sl = "spotLights[" + to_string(i) + "].";
                normalShader.setVec3 (sl + "position", spotLights[i].position);
                normalShader.setVec3 (sl + "direction", spotLights[i].direction);
                normalShader.setVec3 (sl + "ambient", spotLights[i].ambient);
                normalShader.setVec3 (sl + "diffuse", spotLights[i].diffuse);
                normalShader.setVec3 (sl + "specular", spotLights[i].specular);
                normalShader.setFloat(sl + "constant", spotLights[i].constant);
                normalShader.setFloat(sl + "linear", spotLights[i].linear);
                normalShader.setFloat(sl + "quadratic", spotLights[i].quadratic);
                normalShader.setFloat(sl + "cutOff", spotLights[i].cutOff);
                normalShader.setFloat(sl + "outerCutOff", spotLights[i].outerCutOff);
                normalShader.setVec3("SpotLightPos[" + to_string(i) + "]", spotLights[i].position);
            }


            glDisable(GL_CULL_FACE);
            castle.render(&simpleShader);
            glEnable(GL_CULL_FACE);
            renderScene(&simpleShader);
        }
        

        henri:
        if (len < 500) {
            henri.translate(glm::vec3(0.0f, 0.0f, 2.0f));
            henri_curr += glm::vec3(0.0f, 0.0f, 2.0f);
            len += 2;
        }
        else if (curr < total) {
//            henri.translate(glm::vec3(2.0f, 0.0f, 2.0f));
            curr += 1.0f;
            float angle = 90.0f * curr / total;
            auto henri_pos = center + glm::vec3(-radius * cos(glm::radians(angle)), 0.0f, radius *  sin(glm::radians(angle)));
            henri_curr = henri_pos;
            auto henri_pos1 = glm::vec3(
                    -henri_pos.z * sin(glm::radians(angle)) + henri_pos.x * cos(glm::radians(angle)),
                    henri_pos.y,
                    henri_pos.z * cos(glm::radians(angle)) + henri_pos.x * sin(glm::radians(angle))
                    );

            henri.setPosition(henri_pos1);

            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));
        }
        else if (len < 800) {
            float angle = 90.0f;
            henri_curr += glm::vec3(2.0f, 0.0f, 0.0f);

            auto henri_pos1 = glm::vec3(
                    -henri_curr.z * sin(glm::radians(angle)) + henri_curr.x * cos(glm::radians(angle)),
                    henri_curr.y,
                    henri_curr.z * cos(glm::radians(angle)) + henri_curr.x * sin(glm::radians(angle))
            );

            henri.setPosition(henri_pos1);
            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));

            len += 2;
        }
        else if (curr1 < total1) {
            if (stone_wait < 50) {
                stone_wait++;
                goto end;
            }
            curr1 += 1.0f;
            float angle = 90.0f + 180.0f * curr1 / total1;
            auto henri_pos1 = glm::vec3(
                    -henri_curr.z * sin(glm::radians(angle)) + henri_curr.x * cos(glm::radians(angle)),
                    henri_curr.y,
                    henri_curr.z * cos(glm::radians(angle)) + henri_curr.x * sin(glm::radians(angle))
            );

            henri.setPosition(henri_pos1);

            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));
        }
        else if (len < 1100) {
            float angle = 270.0f;
            henri_curr -= glm::vec3(2.0f, 0.0f, 0.0f);

            auto henri_pos1 = glm::vec3(
                    -henri_curr.z * sin(glm::radians(angle)) + henri_curr.x * cos(glm::radians(angle)),
                    henri_curr.y,
                    henri_curr.z * cos(glm::radians(angle)) + henri_curr.x * sin(glm::radians(angle))
            );

            henri.setPosition(henri_pos1);
            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));

            len += 2;
        }
        else if (curr2 < total2) {
            curr2 += 1.0f;
            float angle = 270.0f - 90.0f * curr2 / total2;
            float circle_angle = 90.0f - 90.0f * curr2 / total2;

            auto henri_pos = center + glm::vec3(-radius * cos(glm::radians(circle_angle)), 0.0f, radius *  sin(glm::radians(circle_angle)));
            henri_curr = henri_pos;
            auto henri_pos1 = glm::vec3(
                    -henri_pos.z * sin(glm::radians(angle)) + henri_pos.x * cos(glm::radians(angle)),
                    henri_pos.y,
                    henri_pos.z * cos(glm::radians(angle)) + henri_pos.x * sin(glm::radians(angle))
            );

            henri.setPosition(henri_pos1);

            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));
        }
        else if (len < 1600) {
            float angle = 180.0f;
            henri_curr -= glm::vec3(0.0f, 0.0f, 2.0f);

            auto henri_pos1 = glm::vec3(
                    -henri_curr.z * sin(glm::radians(angle)) + henri_curr.x * cos(glm::radians(angle)),
                    henri_curr.y,
                    henri_curr.z * cos(glm::radians(angle)) + henri_curr.x * sin(glm::radians(angle))
            );

            henri.setPosition(henri_pos1);
            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));

            len += 2;
        }
        else if (curr3 < total3) {
            curr3 += 1.0f;
            float angle = 180.0f + 180.0f * curr3 / total3;
            auto henri_pos1 = glm::vec3(
                    -henri_curr.z * sin(glm::radians(angle)) + henri_curr.x * cos(glm::radians(angle)),
                    henri_curr.y,
                    henri_curr.z * cos(glm::radians(angle)) + henri_curr.x * sin(glm::radians(angle))
            );

            henri.setPosition(henri_pos1);

            henri.setRotation(glm::rotate(glm::mat4(1), glm::radians(angle), glm::vec3(0.0, 1.0, 0.0)));
        }
        else {
            henri.setRotation(glm::mat4(1.0f));
            henri_curr = henri_init;
            len = 0, stone_wait = 0;
            curr = 0.0f, total = 100.0f;
            curr1 = 0.0f, total1 = 100.0f;
            curr2 = 0.0f, total2 = 100.0f;
            curr3 = 0.0f, total3 = 100.0f;
            goto henri;
        }
        end:


        // draw skybox as last
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default


        if (programState->ImGuiEnabled)
            DrawImGui(programState);



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    //programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
        ImGui::DragFloat3("Backpack position", (float*)&programState->backpackPosition);
        ImGui::DragFloat("Backpack scale", &programState->backpackScale, 0.05, 0.1, 4.0);

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (key == GLFW_KEY_N && action == GLFW_PRESS)
        normal ^= true;
}

void renderScene(Shader *shader) {
    for (auto& object : objects)
        object->render(shader);
}

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
