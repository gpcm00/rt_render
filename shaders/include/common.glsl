
vec3 decode_sRGB(vec3 color) {
    return pow(color, vec3(2.2));
}