#version 440

uniform sampler2D font_texture;
uniform int cell_width;

in vec2 UV;
in vec4 Foreground;
in vec4 Background;

layout(location = 0) out vec4 color;

void main()
{
    vec2 size = textureSize(font_texture, 0);
    vec2 offset = vec2(1 / size.x, 0);
    vec4 sample1 = texture(font_texture, UV);
    color = mix(Background, Foreground, sample1);
}
