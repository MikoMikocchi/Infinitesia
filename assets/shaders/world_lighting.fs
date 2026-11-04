#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in float fragWorldY;
in vec3 fragWorldPosition;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
// (world height of the top of the viewed level, 1 / cell size, darkening per level, minimum brightness);
// a darkening of 0 turns the depth shading off.
uniform vec4 depthShadeA = vec4(0.0, 1.0, 0.0, 1.0);
// (desaturation per level, maximum desaturation)
uniform vec4 depthShadeB = vec4(0.0);

// Lighting style; the values are set from presentation/rendering/LightingStyle.h.
uniform vec3 lightDirection = vec3(-0.45, 0.82, 0.35);   // towards the light
uniform vec3 sunColor = vec3(0.62, 0.57, 0.48);
uniform vec3 ambientSky = vec3(0.46, 0.50, 0.60);
uniform vec3 ambientGround = vec3(0.26, 0.25, 0.24);
uniform vec3 fogColor = vec3(0.62, 0.72, 0.84);
// (distance where the fog starts, distance where it is strongest, strongest amount, desaturation at full fog)
uniform vec4 fogParams = vec4(1.0e9, 2.0e9, 0.0, 0.0);
uniform vec3 cameraPosition = vec3(0.0);

out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);

    // Warm sun from one side, cool ambient from the sky above and a duller one from the ground:
    // the top of a block is the brightest, the sides differ by how they face the sun.
    float diffuse = max(dot(normal, normalize(lightDirection)), 0.0);
    vec3 ambient = mix(ambientGround, ambientSky, normal.y * 0.5 + 0.5);
    vec3 lighting = min(ambient + sunColor * diffuse, vec3(1.15));

    vec3 lit = baseColor.rgb * lighting;

    float levelsBelow = max((depthShadeA.x - fragWorldY) * depthShadeA.y, 0.0);
    float brightness = max(1.0 - levelsBelow * depthShadeA.z, depthShadeA.w);
    float drained = min(levelsBelow * depthShadeB.x, depthShadeB.y);
    float grey = dot(lit, vec3(0.299, 0.587, 0.114));
    lit = mix(lit, vec3(grey), drained) * brightness;

    // Distance haze: past the fog start colours drain a little and drift towards the sky colour.
    float fog = clamp((distance(fragWorldPosition, cameraPosition) - fogParams.x) /
                      max(fogParams.y - fogParams.x, 0.0001), 0.0, 1.0);
    fog = fog * fog * (3.0 - 2.0 * fog);
    float litGrey = dot(lit, vec3(0.299, 0.587, 0.114));
    lit = mix(lit, vec3(litGrey), fog * fogParams.w);
    lit = mix(lit, fogColor, fog * fogParams.z);

    finalColor = vec4(lit, baseColor.a);
}
