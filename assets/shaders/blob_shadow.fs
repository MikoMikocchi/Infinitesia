#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float distanceFromCenter = dot(centered, centered);
    float softness = 1.0 - smoothstep(0.12, 1.0, distanceFromCenter);
    float alpha = softness * baseColor.a * 0.58;

    finalColor = vec4(vec3(0.035, 0.045, 0.055), alpha);
}
