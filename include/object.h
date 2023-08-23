#ifndef PROJECT_BASE_OBJECT_H
#define PROJECT_BASE_OBJECT_H

#include <glm/glm.hpp>

#include <learnopengl/model.h>
#include <learnopengl/shader.h>

#include <iostream>
#include <map>
#include <string>


class Object {
private:
    glm::vec3 position;
    glm::mat4 rotation;
    glm::vec3 scale;
    Model* model;
    Shader* shader;
public:
    Object();
    ~Object() = default;

    void setScale(glm::vec3 scale);
    void setModel(Model* model);
    void setShader(Shader* shader);

    glm::vec3 getPosition();

    void translate(glm::vec3 t);
    void rotate(glm::mat4 r);
    void render();
};

Object::Object() {
    position = glm::vec3(0);
    rotation = glm::mat4(1.0f);
    scale = glm::vec3(1);
    model = nullptr;
    shader = nullptr;
};

void Object::setScale(glm::vec3 s) {
    scale = s;
}
void Object::setModel(Model *m) {
    model = m;
    model->SetShaderTextureNamePrefix("material.");
}
void Object::setShader(Shader *s) {
    shader = s;
}

glm::vec3 Object::getPosition() {
    return position;
}

void Object::translate(glm::vec3 t) {
    position += t;
}
void Object::rotate(glm::mat4 r) {
    rotation *= r;
}
void Object::render() {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix *= rotation;
    modelMatrix = glm::scale(modelMatrix, scale);
    modelMatrix = glm::translate(modelMatrix, getPosition());

    shader->setMat4("model", modelMatrix);
    model->Draw(*shader);
}

#endif //PROJECT_BASE_OBJECT_H
