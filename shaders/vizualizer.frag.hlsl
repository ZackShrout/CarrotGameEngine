struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

struct VisualizerParams
{
    uint  frameCount;
    float time;
    float rms;
    float3 bands;     // x = bass, y = mids, z = treble
};

#if defined(VULKAN)
[[vk::push_constant]]
VisualizerParams g_vis;
#else
ConstantBuffer<VisualizerParams> g_vis : register(b0);
#endif

float hash(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float a = hash(i);
    float b = hash(i + float2(1,0));
    float c = hash(i + float2(0,1));
    float d = hash(i + float2(1,1));

    float2 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(a, b, u.x),
                lerp(c, d, u.x), u.y);
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    float2 centered = uv * 2.0 - 1.0; // [-1,1] space

    float t      = g_vis.time;
    float loud   = saturate(g_vis.rms);        // 0..1
    float3 band  = saturate(g_vis.bands);      // 0..1-ish

    // Radial distance and angle
    float r = length(centered);
    float a = atan2(centered.y, centered.x);

    // Base “plasma” pattern
    float n = noise(uv * 3.0 + float2(0.0, t * 0.25));
    float wave = sin(r * 10.0 - t * (3.0 + band.x * 4.0))   // bass drives radial speed
               + cos(a * 6.0 + t * (1.0 + band.y * 2.0));   // mids drive angular variation

    wave += n * 2.0;

    // Normalize wave to 0..1
    float v = 0.5 + 0.25 * wave;

    // Color palette:
    // - hue-ish shift with angle/time
    // - saturation/brightness modulated by loudness & treble
    float hue = a / 6.2831853 + 0.5 + t * 0.05;    // wrap angle to [0,1], slowly spin
    hue = frac(hue);

    float sat = 0.4 + 0.5 * loud;
    float val = v * (0.5 + 0.5 * band.z);          // treble boosts brightness

    // Quick-and-dirty HSV -> RGB
    float3 rgb;
    float h6 = hue * 6.0;
    float c  = val * sat;
    float x  = c * (1.0 - abs(fmod(h6, 2.0) - 1.0));
    float m  = val - c;

    if      (h6 < 1.0) rgb = float3(c, x, 0);
    else if (h6 < 2.0) rgb = float3(x, c, 0);
    else if (h6 < 3.0) rgb = float3(0, c, x);
    else if (h6 < 4.0) rgb = float3(0, x, c);
    else if (h6 < 5.0) rgb = float3(x, 0, c);
    else               rgb = float3(c, 0, x);

    rgb += m;

    // Add a soft vignette so the edges fall off a bit
    float vignette = smoothstep(1.2, 0.4, r);
    rgb *= vignette;

    return float4(rgb, 1.0);
}