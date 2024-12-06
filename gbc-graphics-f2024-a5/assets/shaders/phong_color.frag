#version 460 core

// Inputs
in vec3 position;
in vec3 normal;
in vec2 tcoord;

// Outputs
out vec4 FragColor;

// Uniforms
uniform sampler2D u_tex;

// Camera + Light Para
uniform vec3 u_camPos;

// Orbit Light
uniform vec3 u_litePos;
uniform vec3 u_liteCol;
uniform float u_liteRad;

// Direction Light
uniform vec3 u_dirLitePos;
uniform float u_dirLiteRad;

// Spot Light
uniform vec3 u_spoLiteCamPos;
uniform vec3 u_spoLitePos;
uniform vec3 u_spoLiteCol;
uniform vec3 u_spoLiteDir;
uniform vec3 u_spoLiteRad;

void main()
{
	// -- Spot Light --
	vec3 spoL = normalize(position - u_spoLitePos);
	vec3 spoD = normalize(-u_spoLiteDir);
	float spoLD = dot(spoL, spoD);

	float incut = cos(radians(u_spoLiteRad * 0.5));
	float outcut = cos(radians((u_spoLiteRad* 0.5) + 0.5));
	float cut = incut - outcut;
	float spoTenz = clamp((spoLD - outcut) / cut, 0.0, 1.0);
	
	vec3 spoAmb = u_spoLiteCol * 0.25;
	vec3 spoLite = spoAmb * spoTenz;
	
	// -- Orbit Light --
	vec3 n = normalize(normal);
	vec3 l = normalize(u_litePos - position);
	vec3 v = normalize(u_camPos - position);
	vec3 r = reflect(-l, n);

	float orbNL = max(dot(n, l), 0.0);
	float orbVR = max(dot(v, r), 0.0);

	float orbDis = length(u_litePos - position);
	float orbDim = clamp(u_liteRad / orbDis, 0.0, 1.0);

	vec3 orbAmb = u_liteCol * 0.25;
	vec3 orbDfue = u_liteCol * orbNL;
	vec3 orbSpur = u_liteCol * pow(orbVR, 64);

	vec3 orbLite(orbAmb + orbDfue + orbSpur) * orbDim;

	// -- Direction Light --
	vec3 dirL = normalize(u_dirLitePos - position);
	float dirNL = max(dot(n, dirL), 0.0);

	float dirDis = length(u_dirLitePos - position);
	float dirDim = clamp(u_dirLiteRad / dirDis, 0.0, 1.0);

	vec3 dirAmb = u_liteCol * 0.25;
	vec3 dirDfue = u_liteCol * dirNL;
	
	vec3 dirLite = (dirAmb + dirDfue) * dirDim;

	// -- Combine Lighting --
	vec3 texCol = texture(u_tex, tcoord).rgb;
	vec3 allLites = (orbLite + dirLite + spotLite) * texCol;

	// Final Fragment Color
	FragColor = vec4(allLites, 1.0);
}
