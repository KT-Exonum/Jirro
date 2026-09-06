// MeshSource fragment shader - Full PBR with IBL
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vWorldPos;
layout(location = 2) in vec3 vNormal;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vBitangent;
layout(location = 5) in vec4 vColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D albedoMap;
layout(set = 0, binding = 1) uniform sampler2D normalMap;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughnessMap;
layout(set = 0, binding = 3) uniform sampler2D emissiveMap;
layout(set = 0, binding = 4) uniform sampler2D occlusionMap;

layout(set = 0, binding = 5) uniform samplerCube irradianceMap;   // IBL diffuse
layout(set = 0, binding = 6) uniform samplerCube prefilterMap;    // IBL specular
layout(set = 0, binding = 7) uniform sampler2D brdfLUT;           // BRDF integration

layout(set = 0, binding = 8) uniform MeshUniforms {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 normalMatrix;
    vec3 cameraPos;
    float time;
    bool hasVertexColors;
    bool hasTangents;
    bool skinned;
    int maxJoints;
    
    // Material
    vec3 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float emissiveFactor;
    float alphaCutoff;
    float alphaMode; // 0=OPAQUE, 1=MASK, 2=BLEND
    
    // Material override
    vec3 overrideBaseColor;
    float overrideMetallic;
    float overrideRoughness;
    float overrideEmissive;
    bool useOverride;
    
    // Lights
    int lightCount;
    vec3 lightPositions[8];
    vec3 lightColors[8];
    float lightIntensities[8];
    float lightRadii[8];
    
    // IBL
    float iblIntensity;
    vec3 environmentColor;
    
    vec2 resolution;
} uniforms;

const float PI = 3.14159265359;

// Distribution (GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// Geometry (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0) / 2.0;
    float k = r * r / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel (Schlick)
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Tone mapping (ACES)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// sRGB to linear
vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

// Linear to sRGB
vec3 linearToSrgb(vec3 c) {
    return pow(c, vec3(1.0 / 2.2));
}

// Sample IBL
vec3 sampleIBL(vec3 N, vec3 V, float roughness, vec3 F0) {
    if (uniforms.iblIntensity <= 0.0) return vec3(0.0);
    
    // Diffuse IBL
    vec3 diffuse = texture(irradianceMap, N).rgb * uniforms.environmentColor;
    
    // Specular IBL (split-sum approximation)
    vec3 R = reflect(-V, N);
    vec3 prefilteredColor = texture(prefilterMap, R).rgb;
    
    // BRDF LUT
    vec2 brdfUV = vec2(max(dot(N, V), 0.0), roughness);
    vec2 brdf = texture(brdfLUT, brdfUV).rg;
    vec3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);
    
    return (diffuse + specular) * uniforms.iblIntensity;
}

void main() {
    // Normal
    vec3 N = normalize(vNormal);
    
    // Tangent space normal mapping
    if (uniforms.hasTangents) {
        vec3 T = normalize(vTangent);
        vec3 B = normalize(vBitangent);
        mat3 TBN = mat3(T, B, N);
        vec3 mapNormal = texture(normalMap, vTexCoord).rgb * 2.0 - 1.0;
        N = normalize(TBN * mapNormal);
    }
    
    // View direction
    vec3 V = normalize(uniforms.cameraPos - vWorldPos);
    
    // Material properties
    vec3 albedo = texture(albedoMap, vTexCoord).rgb;
    float metallic = texture(metallicRoughnessMap, vTexCoord).b;
    float roughness = texture(metallicRoughnessMap, vTexCoord).g;
    float ao = texture(occlusionMap, vTexCoord).r;
    
    // Apply vertex colors
    if (uniforms.hasVertexColors) {
        albedo *= vColor.rgb;
        ao *= vColor.a;
    }
    
    // Apply material override
    if (uniforms.useOverride) {
        albedo = uniforms.overrideBaseColor;
        metallic = uniforms.overrideMetallic;
        roughness = uniforms.overrideRoughness;
    }
    
    // sRGB to linear
    albedo = srgbToLinear(albedo);
    
    // Metallic workflow
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Emissive
    vec3 emissive = texture(emissiveMap, vTexCoord).rgb * uniforms.emissiveFactor;
    if (uniforms.useOverride) {
        emissive = vec3(uniforms.overrideEmissive);
    }
    emissive = srgbToLinear(emissive);
    
    // Lighting
    vec3 Lo = vec3(0.0);
    
    // Analytic lights
    for (int i = 0; i < uniforms.lightCount; i++) {
        vec3 L = normalize(uniforms.lightPositions[i] - vWorldPos);
        vec3 H = normalize(V + L);
        
        float distance = length(uniforms.lightPositions[i] - vWorldPos);
        float attenuation = 1.0 / (distance * distance + 0.001);
        attenuation *= smoothstep(uniforms.lightRadii[i], 0.0, distance);
        
        vec3 radiance = uniforms.lightColors[i] * uniforms.lightIntensities[i] * attenuation;
        
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            vec3 H = normalize(V + L);
            float NdotH = max(dot(N, H), 0.0);
            float NdotV = max(dot(N, V), 0.0);
            float NdotL = max(dot(N, L), 0.0);
            
            vec3 F = FresnelSchlick(NdotH, F0);
            float G = GeometrySmith(N, V, L, roughness);
            float D = DistributionGGX(N, H, roughness);
            
            vec3 numerator = D * G * F;
            float denominator = 4.0 * NdotV * NdotL + 0.0001;
            vec3 specular = numerator / denominator;
            
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;
            
            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }
    
    // IBL
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    Lo += sampleIBL(N, V, roughness, F0) * ao;
    
    // Ambient occlusion
    Lo *= ao;
    
    // Add emissive
    Lo += emissive;
    
    // Tone mapping
    Lo = ACESFilm(Lo);
    
    // Gamma correction
    Lo = linearToSrgb(Lo);
    
    // Alpha
    float alpha = 1.0;
    if (uniforms.alphaMode == 1) { // MASK
        alpha = (albedo.a > uniforms.alphaCutoff) ? 1.0 : 0.0;
    } else if (uniforms.alphaMode == 2) { // BLEND
        alpha = albedo.a;
    }
    
    outColor = vec4(Lo, alpha);
}