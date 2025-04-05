struct Ray {
    vec3 origin;
    vec3 direction;
};

struct RayPayload {
    vec4 color;
    uint depth;
    bool hit;
    float t;
    uint sample_id;
};