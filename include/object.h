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
public:
    Object();
    ~Object() = default;

    void setPosition(glm::vec3 p);
    void setRotation(glm::mat4 r);
    void setScale(glm::vec3 s);
    void setModel(Model* m);

    glm::vec3 getPosition();

    void translate(glm::vec3 t);
    void rotate(glm::mat4 r);
    void render(Shader *sh);
};

Object::Object() {
    position = glm::vec3(0);
    rotation = glm::mat4(1.0f);
    scale = glm::vec3(1);
    model = nullptr;
};

void Object::setPosition(glm::vec3 p) {
    position = p;
}
void Object::setRotation(glm::mat4 r) {
    rotation = r;
}
void Object::setScale(glm::vec3 s) {
    scale = s;
}
void Object::setModel(Model *m) {
    model = m;
    model->SetShaderTextureNamePrefix("material.");
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
void Object::render(Shader *sh) {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::scale(modelMatrix, scale);
    modelMatrix *= rotation;
    modelMatrix = glm::translate(modelMatrix, position);

    sh->setMat4("model", modelMatrix);
    model->Draw(*sh);
}

#endif //PROJECT_BASE_OBJECT_H
