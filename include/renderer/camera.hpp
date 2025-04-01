#pragma once
#include <glm/glm.hpp>

struct Camera {
    glm::mat4 view;
    glm::mat4 projection;
};

struct RTCamera {
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 up;
    glm::vec4 right;
    float fov;
    float min;
    float max;
    float aspect_ratio;
};