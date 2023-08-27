#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

#define N_SPOTLIGHTS 2

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;
out mat3 TBN;
out vec3 TangentLightPos;
out vec3 TangentSpotLightPos[N_SPOTLIGHTS];
out vec3 TangentFragPos;
out vec3 TangentViewPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec3 lightPos;
uniform vec3 SpotLightPos[N_SPOTLIGHTS];
uniform vec3 viewPos;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    mat3 TBN = transpose(mat3(T, B, N));
    TangentLightPos = TBN * lightPos;
    for (int i = 0; i < N_SPOTLIGHTS; i++)
        TangentSpotLightPos[i] = TBN * SpotLightPos[i];
    TangentViewPos  = TBN * viewPos;
    TangentFragPos  = TBN * FragPos;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}