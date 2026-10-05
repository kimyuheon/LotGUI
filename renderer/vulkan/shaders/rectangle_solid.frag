#version 450

layout(location = 0) in vec4 fragmentColor;
layout(location = 1) in vec2 fragmentPosition;
layout(location = 2) in vec2 rectangleSize;
layout(location = 3) in float cornerRadius;
layout(location = 0) out vec4 outputColor;

void main() {
    vec2 halfSize = rectangleSize * 0.5;
    float radius = clamp(cornerRadius, 0.0, min(halfSize.x, halfSize.y));
    vec2 offset = abs(fragmentPosition - halfSize) - (halfSize - vec2(radius));
    float distanceToEdge = length(max(offset, vec2(0.0)))
        + min(max(offset.x, offset.y), 0.0) - radius;
    float antialiasWidth = max(fwidth(distanceToEdge), 0.75);
    float coverage = 1.0 - smoothstep(
        -antialiasWidth, antialiasWidth, distanceToEdge);
    outputColor = vec4(fragmentColor.rgb, fragmentColor.a * coverage);
}
