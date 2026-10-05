#version 330 core 

out vec4 FragColor;
in vec3 FragPos;

uniform float time;

void main()
{
    float dist = length(FragPos);
    float normalizedDist = dist / 10.0;
    vec3 nearColor = vec3(1.0, 1.0, 1.0);
    vec3 farColor = vec3(1.0, 1.0, 1.0);
    vec3 color = mix(nearColor, farColor, normalizedDist);
    FragColor = vec4(color, 1.0);
}
