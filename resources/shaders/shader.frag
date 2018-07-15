#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragViewSpacePos;
layout(location = 1) in vec2 fragUv;
layout(location = 2) in mat3 fragTbn;

layout(binding = 1) uniform sampler2D colorMap;
layout(binding = 2) uniform sampler2D normalMap;

layout(push_constant) uniform ModelInfos {
	mat4 model;
	float shininess;
} object;

layout(location = 0) out vec4 outColor;

void main() {
	// Base color.
	vec3 albedo = texture(colorMap, fragUv).rgb;
	
	// Compute normal in view space.
	vec3 n = normalize(2.0 * texture(normalMap, fragUv).rgb - 1.0);
	n = normalize(fragTbn * n);
	// Light dir.
	vec3 l = vec3(normalize(vec3(1.0,1.0,1.0)));
	
	// Phong lighting.
	// Ambient term.
	vec3 color = 0.1 * albedo;
	float shadowFactor = 1.0;
	// Diffuse term.
	float diffuse = max(0.0, dot(n, l));
	color += shadowFactor * diffuse * albedo;
	// Specular term.
	if(diffuse > 0.0){
		vec3 v = normalize(-fragViewSpacePos);
		vec3 r = reflect(-l, n);
		float specular = pow(max(dot(r, v), 0.0), object.shininess);
		color += shadowFactor*specular;
	}
	outColor = vec4(color,1.0);
	
}
