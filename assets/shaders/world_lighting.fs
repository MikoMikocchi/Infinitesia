#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in float fragWorldY;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
// (world height of the top of the viewed level, 1 / cell size, darkening per level, minimum brightness);
// a darkening of 0 turns the depth shading off.
uniform vec4 depthShadeA = vec4(0.0, 1.0, 0.0, 1.0);
// (desaturation per level, maximum desaturation)
uniform vec4 depthShadeB = vec4(0.0);

out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(-0.45, 0.82, 0.35));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float skyFill = max(normal.y, 0.0) * 0.12;
    float lighting = 0.48 + diffuse * 0.42 + skyFill;

    vec3 lit = baseColor.rgb * lighting;

    float levelsBelow = max((depthShadeA.x - fragWorldY) * depthShadeA.y, 0.0);
    float brightness = max(1.0 - levelsBelow * depthShadeA.z, depthShadeA.w);
    float drained = min(levelsBelow * depthShadeB.x, depthShadeB.y);
    float grey = dot(lit, vec3(0.299, 0.587, 0.114));
    lit = mix(lit, vec3(grey), drained) * brightness;

    finalColor = vec4(lit, baseColor.a);
}
