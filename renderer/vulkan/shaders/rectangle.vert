#version 450

layout(location = 0) in vec4 instanceBounds;
layout(location = 1) in vec4 instanceColor;
layout(location = 2) in vec4 instanceParameters;
layout(location = 3) in vec4 instanceTextureCoordinates;

layout(push_constant) uniform Viewport {
    vec2 size;
} viewport;

layout(location = 0) out vec4 fragmentColor;
layout(location = 1) out vec2 fragmentPosition;
layout(location = 2) out vec2 rectangleSize;
layout(location = 3) out float cornerRadius;
layout(location = 4) out vec2 textureCoordinates;
layout(location = 5) out float usesTexture;

const vec2 vertices[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

void main() {
    vec2 localPosition = vertices[gl_VertexIndex];
    vec2 pixelPosition = instanceBounds.xy + localPosition * instanceBounds.zw;
    vec2 normalizedPosition = vec2(
        pixelPosition.x / viewport.size.x * 2.0 - 1.0,
        pixelPosition.y / viewport.size.y * 2.0 - 1.0
    );

    gl_Position = vec4(normalizedPosition, 0.0, 1.0);
    fragmentColor = instanceColor;
    fragmentPosition = localPosition * instanceBounds.zw;
    rectangleSize = instanceBounds.zw;
    cornerRadius = instanceParameters.x;
    textureCoordinates = instanceTextureCoordinates.xy +
        localPosition * instanceTextureCoordinates.zw;
    usesTexture = instanceParameters.y;
}
