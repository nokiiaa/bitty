#version 440

uniform mat4 transform;
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

out vec4 Color;

void main()
{
    gl_Position = transform * position;
    Color = color;
}
