#version 440

uniform sampler2D font_texture;

in vec4 Color;

layout(location = 0) out vec4 color;

void main()
{
    color = Color;
}
