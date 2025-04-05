// PBR model adapted from Google Filament Documentation: https://google.github.io/filament/Filament.md.html#materialsystem/standardmodel
#define PI 3.14159265358979323846


// struct RayPayload {
//   vec3 rayOrigin;
//   vec3 rayDirection;
//   int level;
//   vec4 color;
//   vec4 contribution;
//   bool missed;
//   int index;
//   bool light_ray;
// };

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

vec3 BRDF_Filament(vec3 n, vec3 l, vec3 v, float roughness, float metalness, vec3 f0, vec3 base_color, vec3 light_color) {
    vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    // perceptually linear roughness to roughness (see parameterization)
    float alpha = roughness * roughness;

    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, alpha);

    // specular BRDF
    vec3 Fr = (D * V) * F;

    // diffuse BRDF
    vec3 diffuse = base_color * (1.0 - metalness);
    vec3 Fd = diffuse * Fd_Lambert();

    // apply lighting
    return (Fd + Fr) * light_color * NoL;
}

// http://www.jcgt.org/published/0009/03/02/
vec3 random_pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    return vec3(v) * (1.0/float(0xffffffffu));
}

// https://schuttejoe.github.io/post/ggximportancesamplingpart1/
vec3 sampleGGX(vec2 Xi, float roughness, vec3 N) {
    float a = roughness * roughness;
    
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;
    
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

vec3 importanceSampleGGX(vec3 N, vec3 V, float roughness, vec2 Xi, vec3 F0, out vec3 contribution) {
    vec3 H = sampleGGX(Xi, roughness, N);
    
    vec3 L = reflect(-V, H);
    
    float NoL = dot(N, L);
    float NoH = dot(N, H);
    float VoH = dot(V, H);
    
    if (NoL > 0.0 && NoH > 0.0) {
        // vec3 F0 = vec3(0.04); 
        vec3 F = F_Schlick(VoH, F0);
        float NoV = max(dot(N, V), 0.0001);
        
        float G = V_SmithGGXCorrelated(NoV, NoL, roughness * roughness);
        
        float D = D_GGX(NoH, roughness);
        float weight = (VoH * G * D) / (NoV * NoH);
        
        contribution = F * weight;
    } else {
        contribution = vec3(0.0);
    }
    
    return L;
}