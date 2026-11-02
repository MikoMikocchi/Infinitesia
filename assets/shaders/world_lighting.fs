#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(-0.45, 0.82, 0.35));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float skyFill = max(normal.y, 0.0) * 0.12;
    float lighting = 0.48 + diffuse * 0.42 + skyFill;

    finalColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
