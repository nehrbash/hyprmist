float hash(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}
float noise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x),
                   mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
                   mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}
float fbm(vec3 p) {
    float n = 0.0, amplitude = 0.54;
    for (int i = 0; i < 5; ++i) {
        n += amplitude * noise(p);
        p = p * 2.03 + vec3(7.1, 3.7, 11.8);
        amplitude *= 0.48;
    }
    return n;
}
vec3 centers[12];
vec3 inverseScales[12];
float billowMass[12];
void prepareBillows(vec3 extent, vec3 offset, float t) {
    for (int i = 0; i < 12; ++i) {
        float id = float(i);
        float h = hash(offset + id * 3.7);
        float j = hash(offset + id * 8.9 + 17.0);
        float angle = id * 2.399963;
        float radius = sqrt((id + 0.5) / 12.0);
        vec3 center = vec3(cos(angle) * extent.x * radius,
                           sin(angle) * extent.y * radius,
                           (h - 0.5) * 0.9);
        center.xy += vec2(sin(t * 0.8 + h * 6.28), cos(t * 0.65 + j * 6.28)) * 0.20;
        float release = smoothstep(0.40 + h * 0.18, 0.92 + j * 0.08, progress);
        center.xy += vec2(cos(j * 6.28), sin(j * 6.28)) * release * (0.45 + h * 0.65);
        vec3 scale = vec3(0.78 + h * 0.42, 0.65 + j * 0.38, 0.60 + h * 0.3);
        scale *= 1.0 + release * 0.26;
        centers[i] = center;
        inverseScales[i] = 1.0 / scale;
        billowMass[i] = 1.0 - smoothstep(0.52 + h * 0.16, 0.80 + j * 0.18, progress);
    }
}
float densityAt(vec3 p, vec3 extent, vec3 offset, float t) {
    vec3 flow = vec3(t * 0.16, -t * 0.10, t * 0.17);
    float billow = fbm(p * 3.1 + offset + flow);
    float surfaceDetail = (billow - 0.52) * 1.15;
    float density = 0.0;
    for (int i = 0; i < 12; ++i) {
        float sphere = 1.0 - length((p - centers[i]) * inverseScales[i]);
        float lobe = smoothstep(-0.08, 0.18, sphere + surfaceDetail) * billowMass[i];
        density = 1.0 - (1.0 - density) * (1.0 - lobe);
    }
    vec3 back = abs(p - vec3(0.0, 0.0, 0.65)) - vec3(max(extent.xy - 0.42, vec2(0.1)), 0.22);
    float backDistance = length(max(back, vec3(0.0))) + min(max(back.x, max(back.y, back.z)), 0.0) - 0.23;
    float backing = 1.0 - smoothstep(-0.18, 0.48, backDistance + (0.52 - billow) * 0.65);
    float arrival = 0.32 + 0.36 * noise(vec3(p.xy * 0.55, 5.0) + offset);
    float backingMass = 1.0 - smoothstep(0.035, 0.18, progress - arrival);
    return max(density, backing * backingMass * 0.90);
}
vec4 renderSmoke(vec2 point, vec4 windowRect) {
    vec2 center = windowRect.xy + windowRect.zw * 0.5;
    float unit = clamp(windowRect.w / 3.0, 100.0, 180.0);
    vec2 xy = (point - center) / unit;
    vec3 offset = vec3(mod(seed, 97.0), mod(seed * 0.37, 89.0), mod(seed * 0.13, 79.0));
    float t = progress * 2.4;
    float expansion = 1.0 + smoothstep(0.35, 1.0, progress) * 0.12;
    vec3 extent = vec3(windowRect.zw / (unit * 2.0), 0.75);
    prepareBillows(extent, offset, t);
    float arrival = 0.32 + 0.36 * noise(vec3(xy * 0.55, 5.0) + offset);
    float phase = progress - arrival;
    float envelope = smoothstep(0.0, 0.16, progress) * (1.0 - smoothstep(0.80, 1.0, progress));
    float life = exp(-pow(phase / (phase < 0.0 ? 0.24 : 0.20), 2.0)) * envelope;
    float extinction = 11.0 * life / (expansion * expansion);
    vec3 lightDirection = normalize(vec3(-0.55, -0.83, -0.18));
    vec3 radiance = vec3(0);
    float transmittance = 1.0;
    float jitter = hash(vec3(floor(point), 0.0));
    for (int i = 0; i < 64; ++i) {
        vec3 p = vec3(xy / expansion, -1.5 + (float(i) + jitter) * 0.046875);
        float density = densityAt(p, extent, offset, t);
        if (density > 0.005) {
            float occlusion = densityAt(p + lightDirection * 0.18, extent, offset, t) * 0.18
                            + densityAt(p + lightDirection * 0.48, extent, offset, t) * 0.30
                            + densityAt(p + lightDirection * 0.95, extent, offset, t) * 0.47;
            float sunlight = exp(-occlusion * 5.0 * sqrt(life));
            float region = noise(p * 0.65 + offset + 19.0);
            vec3 tint = mix(accentA.rgb, accentB.rgb, smoothstep(0.25, 0.46, region));
            tint = mix(tint, accentC.rgb, smoothstep(0.56, 0.75, region));
            float lightLuminance = dot(lightColor.rgb, vec3(0.2126, 0.7152, 0.0722));
            vec3 neutralLight = mix(vec3(lightLuminance), lightColor.rgb, 0.12);
            float accentRegion = smoothstep(0.46, 0.68, noise(p * 1.3 + offset + 41.0));
            float rim = smoothstep(0.25, 0.65, sunlight) * (1.0 - density * 0.7);
            float accent = accentRegion * rim;
            vec3 scattered = mix(neutralLight * 0.72, tint, min(accent * 1.2, 1.0));
            vec3 light = shadowColor.rgb * 0.48 + scattered * sunlight * 0.95;
            light += tint * accent * sunlight * 0.35;
            float opacity = 1.0 - exp(-density * extinction * 0.046875);
            radiance += transmittance * opacity * light;
            transmittance *= 1.0 - opacity;
            if (transmittance < 0.005) break;
        }
    }
    float alpha = 1.0 - transmittance;
    vec2 outside = max(abs(point - center) - windowRect.zw * 0.5, vec2(0));
    // Only force full coverage where the window's visibility changes.
    float cover = (1.0 - smoothstep(0.025, 0.10, abs(phase)))
                * (1.0 - smoothstep(0.0, 20.0, length(outside))) * envelope;
    float coveredAlpha = max(alpha, cover);
    vec3 color = radiance / max(alpha, 0.001);
    return vec4(color * coveredAlpha, coveredAlpha);
}

float windowVisibility(vec2 point, vec4 windowRect) {
    float unit = clamp(windowRect.w / 3.0, 100.0, 180.0);
    vec2 xy = (point - windowRect.xy - windowRect.zw * 0.5) / unit;
    vec3 offset = vec3(mod(seed, 97.0), mod(seed * 0.37, 89.0), mod(seed * 0.13, 79.0));
    float arrival = 0.32 + 0.36 * noise(vec3(xy * 0.55, 5.0) + offset);
    float passed = smoothstep(-0.015, 0.015, progress - arrival);
    return incoming != 0 ? passed : 1.0 - passed;
}
