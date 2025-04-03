#pragma once
#include <glm/glm.hpp>

struct Camera {
    glm::mat4 view;
    glm::mat4 projection;
};

class RTCamera {
    public:
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 up;
    glm::vec4 right;
    float fov;
    float min;
    float max;
    float aspect_ratio;

    void set_position(const glm::vec3& pos) {
        position = glm::vec4(pos, 0.0f);
    }

    void set_direction(const glm::vec3& dir) {
        direction = glm::vec4(dir, 0.0f);
    }

    void set_up(const glm::vec3& up_vec) {
        up = glm::vec4(up_vec, 0.0f);
    }

    void set_right(const glm::vec3& right_vec) {
        right = glm::vec4(right_vec, 0.0f);
    }

    void set_fov(float fov_degrees) {
        fov = glm::radians(fov_degrees/2.0f);
    }

    void set_aspect_ratio(float ratio) {
        aspect_ratio = ratio;
    }

    void set_range(float rmin, float rmax) {
        min = rmin;
        max = rmax;
    }

};