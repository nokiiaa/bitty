#version 440

uniform mat4 transform;
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 foreground;
layout(location = 3) in vec4 background;

out vec2 UV;
out vec4 Foreground;
out vec4 Background;

void main()
{
    gl_Position = transform * position;
    UV = uv;
    Foreground = foreground;
    Background = background;
}
