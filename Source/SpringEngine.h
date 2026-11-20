// PTZR Spring - motor de reverb de mola (DSP puro, sem dependência do JUCE)
// Modelo: laços de feedback com cadeia de all-pass dispersivos (efeito "chirp"/boing da mola),
// damping dentro do laço, difusão opcional, low cut, drive de fita, tone e pre-delay.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace ptzr
{
constexpr double kTwoPi = 6.283185307179586476925286766559;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const double xd = x;
        const double y = b0 * xd + z1;
        z1 = b1 * xd - a1 * y + z2;
        z2 = b2 * xd - a2 * y;
        return (float) y;
    }
    void reset() noexcept { z1 = z2 = 0; }
    void copyCoefs (const Biquad& o) noexcept { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void setLowpass (double fs, double f, double q) noexcept
    {
        f = std::min (f, fs * 0.45);
        const double w = kTwoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setHighpass (double fs, double f, double q) noexcept
    {
        f = std::min (f, fs * 0.45);
        const double w = kTwoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setLowShelf (double fs, double f, double gainDb, double q = 0.707) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * f / fs, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), beta = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
             (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
    }
    void setHighShelf (double fs, double f, double gainDb, double q = 0.707) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * f / fs, c = std::cos (w);
        const double al = std::sin (w) / (2 * q), beta = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
             (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
    }
};

struct OnePoleLP
{
    float z = 0, c = 1;
    void setCutoff (double fs, double fc) noexcept
    {
        fc = std::min (fc, fs * 0.45);
        c = (float) (1.0 - std::exp (-kTwoPi * fc / fs));
    }
    inline float process (float x) noexcept { z += c * (x - z); return z; }
    void reset() noexcept { z = 0; }
};

//==============================================================================
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void write (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d >= 1 (amostras). Chamar read ANTES de write no mesmo instante.
    inline float read (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float s0 = buf[(size_t) ((w - i) & mask)];
        const float s1 = buf[(size_t) ((w - i - 1) & mask)];
        return s0 + f * (s1 - s0);
    }
};

struct SchroederAP
{
    DelayLine d;
    float g = 0.f, len = 100.f;
    void prepare (int maxS) { d.prepare (maxS); }
    void clear() { d.clear(); }
    inline float process (float x) noexcept
    {
        const float wd = d.read (len);
        const float w = x + g * wd;
        d.write (w);
        return -g * w + wd;
    }
};

//==============================================================================
// Um "trecho de mola": delay + cadeia de all-pass dispersivos + damping + bloqueio de DC, em feedback.
struct SpringLoop
{
    static constexpr int kMaxStages = 40;
    DelayLine delay;
    float z[kMaxStages] {};
    float lp = 0, hpX = 0, hpY = 0;
    float a = -0.6f, g = 0.9f, lpC = 0.5f, lenSamples = 1500.f, modDepth = 0.f;
    int stages = 28;
    double phase = 0, phaseInc = 0;

    void prepare (int maxSamples) { delay.prepare (maxSamples); clear(); }
    void clear()
    {
        delay.clear();
        for (auto& v : z) v = 0;
        lp = hpX = hpY = 0;
    }

    inline float tick (float in) noexcept
    {
        const float lfo = (float) std::sin (phase);
        phase += phaseInc;
        if (phase > kTwoPi) phase -= kTwoPi;

        float y = delay.read (std::max (2.0f, lenSamples + modDepth * lfo));

        for (int i = 0; i < stages; ++i)          // dispersão: graves atrasam mais que agudos => "chirp" de mola
        {
            const float o = a * y + z[i];
            z[i] = y - a * o;
            y = o;
        }
        lp += lpC * (y - lp);                      // damping
        y = lp;
        const float hy = y - hpX + 0.995f * hpY;   // bloqueio de DC / sub-graves no laço
        hpX = y; hpY = hy; y = hy;

        delay.write (in + y * g);
        return y;
    }
};

//==============================================================================
class SpringReverbEngine
{
public:
    struct Params
    {
        float mix = 0.35f;        // 0..1
        float decaySec = 2.8f;    // 0.5..10
        float tone = 0.5f;        // 0..1 (0 = dark, 1 = bright)
        float preDelayMs = 25.f;  // 0..200
        float drive = 0.3f;       // 0..1
        float damping = 0.6f;     // 0..1
        float lowCutHz = 120.f;   // 20..1000
        int   type = 1;           // 0 vintage, 1 classic, 2 modern
    };

    void prepare (double sampleRate, int initialType)
    {
        sr = sampleRate;
        const int pdMax = (int) (sr * 0.2) + 16;
        const double k = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            preDelay[c].prepare (pdMax);
            for (int j = 0; j < 2; ++j) spring[c][j].prepare ((int) (sr * 0.12) + 16);
            for (int j = 0; j < 4; ++j) diff[c][j].prepare ((int) (700 * k) + 16);
        }
        static const float dl[4] = { 113.f, 337.f, 191.f, 419.f };
        static const float dr[4] = { 127.f, 311.f, 211.f, 439.f };
        for (int j = 0; j < 4; ++j) { diff[0][j].len = dl[j] * (float) k; diff[1][j].len = dr[j] * (float) k; }

        fadeSamples = std::max (1, (int) (sr * 0.015));
        activeType = pendingType = std::clamp (initialType, 0, 2);
        applyType (activeType);
        fadeState = 0; fadeGain = 1.f;
        snapNext = true;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            preDelay[c].clear(); hpA[c].reset(); hpB[c].reset(); inLp[c].reset(); satLp[c].reset();
            ls[c].reset(); hs[c].reset(); tlp[c].reset();
            for (auto& s : spring[c]) s.clear();
            for (auto& d : diff[c]) d.clear();
        }
    }

    void setParams (const Params& p)
    {
        target = p;
        if (snapNext) { cur = p; snapNext = false; lastDry = std::cos (p.mix * 1.5707963f); lastWet = std::sin (p.mix * 1.5707963f); }
        if (p.type != activeType && fadeState == 0) { pendingType = p.type; fadeState = 1; }
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        int pos = 0;
        while (pos < n)
        {
            const int len = std::min (kChunk, n - pos);
            smooth (len);
            updateCoefs();

            const float dryT = std::cos (cur.mix * 1.5707963f);
            const float wetT = std::sin (cur.mix * 1.5707963f) * 1.9f;
            const float dry0 = lastDry, wet0 = lastWet;

            for (int i = 0; i < len; ++i)
            {
                const float t = (float) (i + 1) / (float) len;
                const float dryG = dry0 + (dryT - dry0) * t;
                const float wetG = wet0 + (wetT - wet0) * t;

                const float x[2] = { inL[pos + i], inR[pos + i] };
                float s[2];

                // --- caminho molhado, entrada ---
                for (int c = 0; c < 2; ++c)
                {
                    const float pd = preDelay[c].read (pdSamples);
                    preDelay[c].write (x[c]);
                    float w = hpB[c].process (hpA[c].process (pd));          // low cut (24 dB/oit)
                    w = inLp[c].process (w);                                 // limitação de banda da entrada (tipo de mola)
                    s[c] = saturate (w, c);                                  // drive de fita
                }

                // --- molas (2 trechos por canal, com leve crossfeed L<->R) ---
                float wet[2];
                for (int c = 0; c < 2; ++c)
                {
                    const float in = s[c] * (1.f - cross) + s[1 - c] * cross;
                    float o = 0.5f * (spring[c][0].tick (in * inGain[c][0]) + spring[c][1].tick (in * inGain[c][1]));
                    if (diffAmt > 0.001f)
                        for (int j = 0; j < 4; ++j) o = diff[c][j].process (o);
                    o = hs[c].process (ls[c].process (tlp[c].process (o)));  // tone
                    wet[c] = o;
                }

                // --- troca de tipo sem clique (fade-out -> reinicia -> fade-in) ---
                if (fadeState == 1)
                {
                    fadeGain -= 1.f / (float) fadeSamples;
                    if (fadeGain <= 0.f)
                    {
                        fadeGain = 0.f;
                        activeType = pendingType;
                        applyType (activeType);
                        for (int c = 0; c < 2; ++c) { for (auto& sp : spring[c]) sp.clear(); for (auto& d : diff[c]) d.clear(); }
                        fadeState = 2;
                    }
                }
                else if (fadeState == 2)
                {
                    fadeGain += 1.f / (float) fadeSamples;
                    if (fadeGain >= 1.f) { fadeGain = 1.f; fadeState = 0; }
                }

                const float wg = wetG * fadeGain;
                outL[pos + i] = x[0] * dryG + wet[0] * wg;
                outR[pos + i] = x[1] * dryG + wet[1] * wg;
            }
            lastDry = dryT; lastWet = wetT;
            pos += len;
        }
    }

private:
    static constexpr int kChunk = 16;

    struct TypeCfg
    {
        float a; int stages; float lenMs[4];
        float modDepthMs, modRateHz, lpScale, inLpHz, diffusion, crossfeed;
    };
    static const TypeCfg& cfgFor (int t)
    {
        static const TypeCfg k[3] = {
            // VINTAGE: mais dispersão (boing forte), mais escuro, leve wobble
            { -0.72f, 36, { 38.2f, 53.7f, 41.9f, 58.3f }, 0.10f, 0.55f, 0.60f,  5500.f, 0.00f, 0.30f },
            // CLASSIC: equilibrado, mais aberto
            { -0.62f, 28, { 31.7f, 43.9f, 34.3f, 47.3f }, 0.04f, 0.80f, 1.00f, 10000.f, 0.28f, 0.20f },
            // MODERN: pouca dispersão, difuso, largo e brilhante (espacial/digital)
            { -0.45f, 16, { 47.3f, 61.9f, 52.1f, 67.7f }, 0.16f, 0.35f, 1.45f, 16000.f, 0.58f, 0.05f }
        };
        return k[std::clamp (t, 0, 2)];
    }

    void applyType (int t)
    {
        const auto& c = cfgFor (t);
        cross = c.crossfeed;
        diffAmt = c.diffusion;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int j = 0; j < 2; ++j)
            {
                auto& s = spring[ch][j];
                s.a = c.a; s.stages = c.stages;
                s.lenSamples = (float) (c.lenMs[ch * 2 + j] * 0.001 * sr);
                s.modDepth = (float) (c.modDepthMs * 0.001 * sr);
                s.phaseInc = kTwoPi * c.modRateHz * (1.0 + 0.13 * j + 0.07 * ch) / sr;
                s.phase = kTwoPi * (0.25 * ch + 0.5 * j);
            }
            inLp[ch].setCutoff (sr, c.inLpHz);
            for (auto& d : diff[ch]) d.g = 0.f;
        }
        for (int ch = 0; ch < 2; ++ch)
            for (int j = 0; j < 4; ++j) diff[ch][j].g = c.diffusion;
    }

    void smooth (int len)
    {
        const float k = 1.f - std::exp (-(float) len / (float) (sr * 0.04));
        auto sm = [k] (float& c, float t) { c += (t - c) * k; };
        sm (cur.mix, target.mix); sm (cur.decaySec, target.decaySec); sm (cur.tone, target.tone);
        sm (cur.preDelayMs, target.preDelayMs); sm (cur.drive, target.drive);
        sm (cur.damping, target.damping); sm (cur.lowCutHz, target.lowCutHz);
    }

    void updateCoefs()
    {
        const auto& t = cfgFor (activeType);

        // low cut: Butterworth 4ª ordem
        const double fc = std::clamp ((double) cur.lowCutHz, 20.0, 1000.0);
        for (int c = 0; c < 2; ++c) { hpA[c].setHighpass (sr, fc, 0.5412); hpB[c].setHighpass (sr, fc, 1.3066); }

        // pre-delay
        pdSamples = std::max (1.0f, (float) (cur.preDelayMs * 0.001 * sr));

        // drive
        driveGain = 1.f + 4.f * cur.drive;
        driveBias = 0.12f * cur.drive;
        driveMakeup = 1.f + 0.30f * cur.drive;
        driveLpMix = 0.55f * cur.drive;
        for (int c = 0; c < 2; ++c) satLp[c].setCutoff (sr, 8500.0);

        // damping dentro do laço (0% = aberto ~18 kHz, 100% = escuro ~1.4 kHz)
        const double dampHz = 18000.0 * std::pow (0.08, (double) cur.damping) * t.lpScale;
        const float lpC = (float) (1.0 - std::exp (-kTwoPi * std::min (dampHz, sr * 0.45) / sr));

        // feedback a partir do RT60 pedido
        const float decay = std::max (0.2f, cur.decaySec) * 1.2f;   // compensa perda por damping/DC-block
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 2; ++j)
            {
                auto& s = spring[c][j];
                const double Teff = (double) s.lenSamples / sr + (double) s.stages * 0.32 / sr;
                float g = (float) std::pow (10.0, -3.0 * Teff / (double) decay);
                g = std::min (g, 0.9925f);
                s.g = g; s.lpC = lpC;
                inGain[c][j] = std::sqrt (std::max (0.f, 1.f - g * g)) * kInTrim;
            }

        // tone: tilt (shelves) + lowpass no lado escuro
        const float tl = (cur.tone - 0.5f) * 2.f;
        Biquad tmpL, tmpH, tmpP;
        tmpL.setLowShelf (sr, 250.0, -4.0 * tl);
        tmpH.setHighShelf (sr, 3500.0, 9.0 * tl);
        const double lpHz = cur.tone >= 0.5f ? 20000.0 : 20000.0 * std::pow (0.12, (0.5 - (double) cur.tone) * 2.0);
        tmpP.setLowpass (sr, lpHz, 0.707);
        for (int c = 0; c < 2; ++c) { ls[c].copyCoefs (tmpL); hs[c].copyCoefs (tmpH); tlp[c].copyCoefs (tmpP); }
    }

    inline float saturate (float x, int c) noexcept
    {
        // tanh com leve bias (harmônicas pares, "cor" de fita) e leve perda de agudos proporcional ao drive
        const float g = driveGain;
        float y = (std::tanh (g * (x + driveBias)) - std::tanh (g * driveBias)) / g;
        y *= driveMakeup;
        const float l = satLp[c].process (y);
        return y + (l - y) * driveLpMix;
    }

    static constexpr float kInTrim = 1.9f;

    double sr = 44100.0;
    Params cur, target;
    bool snapNext = true;
    float lastDry = 0.8f, lastWet = 0.5f;

    DelayLine preDelay[2];
    Biquad hpA[2], hpB[2], ls[2], hs[2], tlp[2];
    OnePoleLP inLp[2], satLp[2];
    SpringLoop spring[2][2];
    SchroederAP diff[2][4];

    float pdSamples = 1000.f, driveGain = 2.2f, driveBias = 0.f, driveMakeup = 1.f, driveLpMix = 0.f;
    float inGain[2][2] {};
    float cross = 0.2f, diffAmt = 0.3f;

    int activeType = 1, pendingType = 1, fadeState = 0, fadeSamples = 600;
    float fadeGain = 1.f;
};
} // namespace ptzr
