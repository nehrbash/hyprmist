#version 440
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float progress;
    float seed;
    float incoming;
    vec2 targetSize;
    vec4 windowRect;
    vec4 shadowColor;
    vec4 accentA;
    vec4 accentB;
    vec4 accentC;
    vec4 lightColor;
};
layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D smokeVolume;

#include "../shaders/volume.glsl"
void main() {
    vec2 uv = qt_TexCoord0;
    float visibility = windowVisibility(uv * targetSize, windowRect);
    vec4 smoke = texture(smokeVolume, uv);
    vec4 window = texture(source, uv) * visibility;
    fragColor = (smoke + window * (1.0 - smoke.a)) * qt_Opacity;
}
