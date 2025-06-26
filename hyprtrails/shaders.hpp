#pragma once

#include <string>

inline const std::string QUADTRAIL = R"#(
#version 300 es
precision mediump float;
uniform mat3 proj;
uniform vec4 color;
in vec2 pos;
in vec2 texcoord;
in vec4 colors;
out vec4 v_color;
out vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_color = color;
    v_texcoord = texcoord;
})#";

inline const std::string FRAGTRAIL = R"#(
#version 300 es
precision mediump float;
in vec4 v_color;
in vec2 v_texcoord;

uniform vec4 window;

layout(location = 0) out vec4 fragColor;

float distToRect(vec4 rect) {
    float dx = max(rect[0] - v_texcoord[0], max(0.0, v_texcoord[0] - rect[2]));
    float dy = max(rect[1] - v_texcoord[1], max(0.0, v_texcoord[1] - rect[3]));
    return sqrt(dx*dx + dy*dy);
}

float alphaForShot(vec4 shot, float threshold) {

    float dist = distToRect(shot);

    if (dist > threshold)
        return 0.0;

    if (dist <= 0.0)
        return 0.0;

    return 1.0 - (dist * (1.0 / threshold));
}

void main() {

    vec4 pixColor = v_color;
    float a = v_color[3]; // clamp(alphaForShot(window, 0.5), 0.0, 1.0); // todo

    pixColor.rgb *= a;

    fragColor = pixColor;
})#";
