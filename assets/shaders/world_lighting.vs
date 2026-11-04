#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out float fragWorldY;
out vec3 fragWorldPosition;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    // Terrain meshes are built in world space; objects switch the depth shading off.
    fragWorldY = vertexPosition.y;
    // Terrain is drawn with an identity model matrix; objects carry theirs, so fog and the
    // normals below work in world space for both.
    fragWorldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
