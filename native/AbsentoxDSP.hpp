// The ABSENToX sound engine: a port of absentox.jsfx, with no dependencies.
// Keep both in step. tests/smoke.py runs the same checks on each and compares their levels.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace absentox {

enum Param {
    P_WAVE, P_DETUNE, P_SUB, P_NOISE, P_WOW_RATE, P_WOW_DEPTH, P_CUTOFF, P_RESO, P_FENV,
    P_ATK, P_DEC, P_SUS, P_REL, P_DRIVE, P_CH_RATE, P_CH_MIX, P_DL_TIME, P_DL_FB, P_DL_MIX,
    P_RV_SIZE, P_RV_DECAY, P_RV_TONE, P_RV_SHIM, P_RV_MIX, P_EQ_LOW, P_EQ_MID, P_EQ_HIGH, P_OUT,
    // switches are stored as "off" to match the JSFX version: 0 is on
    P_OFF_OSC, P_OFF_WOW, P_OFF_FLT, P_OFF_ENV, P_OFF_DRV, P_OFF_CHO, P_OFF_DLY, P_OFF_REV,
    P_OFF_EQ, P_OFF_OUT,
    P_COUNT
};

struct ParamInfo { const char* name; float min, max, def; bool log; };

static const ParamInfo kParams[P_COUNT] = {
    {"Wave", 0, 1, 0, false},
    {"Detune", 0, 50, 14, false},
    {"Sub", 0, 1, 0.25f, false},
    {"Noise", 0, 1, 0.04f, false},
    {"Wow Rate", 0.05f, 8, 0.4f, true},
    {"Wow Depth", 0, 100, 10, false},
    {"Cutoff", 40, 18000, 2500, true},
    {"Resonance", 0, 1, 0.1f, false},
    {"Filter Env", -1, 1, 0.25f, false},
    {"Attack", 1, 8000, 350, true},
    {"Decay", 5, 8000, 1200, true},
    {"Sustain", 0, 1, 0.75f, false},
    {"Release", 5, 12000, 1800, true},
    {"Drive", 0, 1, 0.2f, false},
    {"Chorus Rate", 0.05f, 6, 0.6f, true},
    {"Chorus Mix", 0, 1, 0.35f, false},
    {"Delay Time", 20, 1500, 420, true},
    {"Delay Feedback", 0, 0.9f, 0.35f, false},
    {"Delay Mix", 0, 1, 0.18f, false},
    {"Reverb Size", 0.4f, 2, 1.2f, false},
    {"Reverb Decay", 0.5f, 30, 6, true},
    {"Reverb Tone", 0, 1, 0.55f, false},
    {"Reverb Shimmer", 0, 1, 0.1f, false},
    {"Reverb Mix", 0, 1, 0.45f, false},
    {"EQ Low", -12, 12, 0, false},
    {"EQ Mid", -12, 12, 0, false},
    {"EQ High", -12, 12, 0, false},
    {"Output", -36, 6, 0, false},
    {"OSC Off", 0, 1, 0, false},
    {"WOW Off", 0, 1, 0, false},
    {"FILTER Off", 0, 1, 0, false},
    {"ENVELOPE Off", 0, 1, 0, false},
    {"DRIVE Off", 0, 1, 0, false},
    {"CHORUS Off", 0, 1, 0, false},
    {"DELAY Off", 0, 1, 0, false},
    {"REVERB Off", 0, 1, 0, false},
    {"EQ Off", 0, 1, 0, false},
    {"OUT Off", 0, 1, 0, false},
};

struct Midi { uint32_t frame; uint8_t status, d1, d2; };

// reverb delay line lengths, in seconds at Size 1
static const double RLEN[8] = {0.0383, 0.0363, 0.0465, 0.0510, 0.0544, 0.0593, 0.0658, 0.0717};

class Engine {
public:
    Engine() { for (int i = 0; i < P_COUNT; ++i) p[i] = kParams[i].def; }

    float get(int i) const { return p[i]; }

    // Hosts are outside our control, and delay lengths depend on these staying in range.
    void set(int i, float v) {
        if (i < 0 || i >= P_COUNT) return;
        const ParamInfo& k = kParams[i];
        p[i] = v >= k.min ? (v <= k.max ? v : k.max) : k.min;
    }

    // Allocates, so call it outside the audio thread, before the first run().
    void setup(double sampleRate) {
        sr = sampleRate;
        chn = (int)std::ceil(0.03 * sr);
        chl.assign(chn, 0); chr.assign(chn, 0);
        dn = (int)std::ceil(1.6 * sr);
        dbl.assign(dn, 0); dbr.assign(dn, 0);
        const double as = sr / 44100;
        const int apLen[4] = {142, 379, 151, 353};
        for (int i = 0; i < 4; ++i) { ap[i].b.assign((int)std::ceil(apLen[i] * as), 0); ap[i].w = 0; }
        rn = (int)std::ceil(0.15 * sr) + 16; // longest line at Size 2, plus modulation
        rbuf.assign(8 * rn, 0);
        rmod = 0.00018 * sr;
        shw = (int)std::ceil(0.08 * sr); shn = shw + 2;
        shb.assign(shn, 0);
        reset();
    }

    // Silences everything without allocating.
    void reset() {
        std::fill(chl.begin(), chl.end(), 0.0); std::fill(chr.begin(), chr.end(), 0.0);
        std::fill(dbl.begin(), dbl.end(), 0.0); std::fill(dbr.begin(), dbr.end(), 0.0);
        for (AllPass& a : ap) { std::fill(a.b.begin(), a.b.end(), 0.0); a.w = 0; }
        std::fill(rbuf.begin(), rbuf.end(), 0.0);
        std::fill(shb.begin(), shb.end(), 0.0);
        for (double& z : rz) z = 0;
        for (Voice& v : voices) v = Voice();
        for (double& s : eqs) s = 0;
        dlpL = dlpR = 0;
        cw = dw = rw = sw = 0;
        cp = sp = rl = wp1 = wp2 = wow = 0;
        pb = 0; ctl = 0; act = 0; duck = 0;
        dcur = p[P_DL_TIME] * 0.001 * sr;
        rsz = p[P_RV_SIZE];
        fade = 1 / (0.005 * sr);
    }

    // Events must be sorted by frame.
    void run(float* outL, float* outR, uint32_t frames, const Midi* events, uint32_t count) {
        block();
        uint32_t next = 0;
        for (uint32_t f = 0; f < frames; ++f) {
            switches();
            while (next < count && events[next].frame <= f) midi(events[next++]);
            sample(outL[f], outR[f]);
        }
        while (next < count) midi(events[next++]);
    }

    void midi(const Midi& m) {
        const int st = m.status & 0xF0;
        if (st == 0x90 && m.d2 > 0) noteOn(m.d1, m.d2);
        else if (st == 0x80 || st == 0x90) noteOff(m.d1);
        else if (st == 0xB0 && (m.d1 == 123 || m.d1 == 120)) noteOff(-1);
        else if (st == 0xE0) { pb = (m.d2 * 128 + m.d1 - 8192) / 4096.0; ctl = 0; } // +/- 2 semitones
    }

private:
    static constexpr int NV = 6;
    static constexpr double TWO_PI = 6.283185307179586;
    static constexpr double PI = 3.141592653589793;
    // keeps feedback paths out of denormal numbers, which are very slow on many CPUs
    static constexpr double DENORMAL_GUARD = 1e-15;

    struct Voice {
        int state = 0; // 0 off, 1 attack, 2 decay/sustain, 3 release
        int note = 0;
        double vel = 0, env = 0;
        double pc = 0, pa = 0, pb = 0, ps = 0; // phases: centre, left, right, sub
        double ic = 0, ia = 0, ib = 0, is = 0; // phase increments
        double l1 = 0, l2 = 0, r1 = 0, r2 = 0; // filter state
        double a1 = 0, a2 = 0, a3 = 0;         // filter coefficients
    };
    struct AllPass { std::vector<double> b; int w = 0; };

    float p[P_COUNT];
    double sr = 44100;
    Voice voices[NV];

    std::vector<double> chl, chr, dbl, dbr, rbuf, shb;
    AllPass ap[4];
    int chn = 0, dn = 0, rn = 0, shw = 0, shn = 0;
    int cw = 0, dw = 0, rw = 0, sw = 0;
    double rmod = 0, rsz = 1, dcur = 0, dlpL = 0, dlpR = 0;
    double cp = 0, sp = 0, rl = 0, wp1 = 0, wp2 = 0, wow = 0;
    double rg[8] = {}, rz[8] = {}, rx[8] = {};
    double eqc[15] = {}, eqs[12] = {};
    double pb = 0, duck = 0, fade = 0;
    int ctl = 0, mask = 0, act = 0;
    uint32_t rng = 0x9E3779B9u;

    // values derived once per block
    double atkInc = 0, decCo = 0, relCo = 0, susV = 0, detRatio = 1, fk = 2, drvG = 1, drvOut = 1;
    double chInc = 0, dlTarget = 0, rvDamp = 0, outG = 1;

    double rnd() { // xorshift: rand() is not safe on the audio thread
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return rng / 4294967296.0;
    }

    static double blep(double t, double dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1; }
        if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
        return 0;
    }

    // saw minus a half-cycle-shifted saw is a square, so Wave morphs saw -> square
    double osc(double ph, double dt) const {
        double s = 2 * ph - 1 - blep(ph, dt);
        if (p[P_WAVE] > 0) {
            double q = ph + 0.5; if (q >= 1) q -= 1;
            s -= p[P_WAVE] * (2 * q - 1 - blep(q, dt));
        }
        return s;
    }

    static double tap(const double* b, int n, int w, double d) {
        double pos = w - d; if (pos < 0) pos += n;
        int i = (int)pos; const double f = pos - i;
        if (i >= n) i -= n; // pos can round up to exactly n
        return b[i] * (1 - f) + b[i + 1 >= n ? 0 : i + 1] * f;
    }

    static double allpass(double x, AllPass& a) {
        const double d = a.b[a.w];
        const double u = x + 0.6 * d;
        a.b[a.w] = u;
        if (++a.w >= (int)a.b.size()) a.w = 0;
        return d - 0.6 * u;
    }

    static double bq(double x, const double* c, double* s) {
        const double y = c[0] * x + s[0];
        s[0] = c[1] * x - c[3] * y + s[1];
        s[1] = c[2] * x - c[4] * y;
        return y;
    }

    // RBJ cookbook. type: 0 low shelf, 1 peak, 2 high shelf
    void eqSet(double* c, int type, double f, double db) const {
        const double a = std::pow(10.0, db / 40), w = TWO_PI * f / sr, cs = std::cos(w);
        const double al = std::sin(w) / std::sqrt(2.0), sq = 2 * std::sqrt(a) * al;
        double a0;
        if (type == 0) {
            a0 = (a + 1) + (a - 1) * cs + sq;
            c[0] = a * ((a + 1) - (a - 1) * cs + sq) / a0;
            c[1] = 2 * a * ((a - 1) - (a + 1) * cs) / a0;
            c[2] = a * ((a + 1) - (a - 1) * cs - sq) / a0;
            c[3] = -2 * ((a - 1) + (a + 1) * cs) / a0;
            c[4] = ((a + 1) + (a - 1) * cs - sq) / a0;
        } else if (type == 1) {
            a0 = 1 + al / a;
            c[0] = (1 + al * a) / a0;
            c[1] = -2 * cs / a0;
            c[2] = (1 - al * a) / a0;
            c[3] = c[1];
            c[4] = (1 - al / a) / a0;
        } else {
            a0 = (a + 1) - (a - 1) * cs + sq;
            c[0] = a * ((a + 1) + (a - 1) * cs + sq) / a0;
            c[1] = -2 * a * ((a - 1) + (a + 1) * cs) / a0;
            c[2] = a * ((a + 1) + (a - 1) * cs - sq) / a0;
            c[3] = 2 * ((a - 1) - (a + 1) * cs) / a0;
            c[4] = ((a + 1) - (a - 1) * cs - sq) / a0;
        }
    }

    void noteOn(int n, int vel) {
        // same note first, then a free voice, then the quietest (released before held)
        Voice* best = voices;
        double low = 1e9;
        for (Voice& v : voices) {
            const double score = v.state == 0 ? -1 : v.note == n ? -2 : v.env + (v.state < 3 ? 1 : 0);
            if (score < low) { low = score; best = &v; }
        }
        // a stolen voice keeps its level and filter state so it does not click
        if (best->state == 0) {
            best->env = best->l1 = best->l2 = best->r1 = best->r2 = 0;
            // one shared start phase: unrelated phases would change the level of every note at low Detune
            best->pc = best->pa = best->pb = rnd(); best->ps = 0;
        }
        best->state = 1; best->note = n; best->vel = 0.35 + 0.65 * vel / 127;
        ctl = 0; // compute this voice's pitch and filter before its first sample
    }

    void noteOff(int n) {
        for (Voice& v : voices)
            if (v.state && v.state < 3 && (n < 0 || v.note == n)) v.state = 3;
    }

    // everything is derived here, so host automation needs no separate update path
    void block() {
        const bool offEnv = p[P_OFF_ENV] > 0.5f;
        // Envelope off is a plain gate: short fades only, so notes still do not click
        atkInc = 1 / ((offEnv ? 3 : p[P_ATK]) * 0.001 * sr);
        decCo = std::exp(-6.9 / ((offEnv ? 5 : p[P_DEC]) * 0.001 * sr));
        relCo = std::exp(-6.9 / ((offEnv ? 5 : p[P_REL]) * 0.001 * sr));
        susV = offEnv ? 1 : p[P_SUS];
        detRatio = std::pow(2.0, p[P_DETUNE] / 1200.0);
        fk = 2 - 1.96 * p[P_RESO];
        drvG = 1 + p[P_DRIVE] * p[P_DRIVE] * 40.0;
        drvOut = 1 / (1.0 + p[P_DRIVE]);
        chInc = p[P_CH_RATE] / sr;
        dlTarget = p[P_DL_TIME] * 0.001 * sr;
        rvDamp = 1 - std::exp(-TWO_PI * 800 * std::pow(2.0, p[P_RV_TONE] * 4.3) / sr);
        for (int i = 0; i < 8; ++i) rg[i] = std::pow(10.0, -3 * RLEN[i] * p[P_RV_SIZE] / p[P_RV_DECAY]);
        // one bit per active section. A Mix at 0 counts as off: it costs no CPU and its memory gets cleared
        mask = (p[P_OFF_OSC] < 0.5f ? 1 : 0) | (p[P_OFF_WOW] < 0.5f ? 2 : 0) | (p[P_OFF_FLT] < 0.5f ? 4 : 0)
             | (!offEnv ? 8 : 0) | (p[P_OFF_DRV] < 0.5f ? 16 : 0)
             | (p[P_OFF_CHO] < 0.5f && p[P_CH_MIX] > 0 ? 32 : 0)
             | (p[P_OFF_DLY] < 0.5f && p[P_DL_MIX] > 0 ? 64 : 0)
             | (p[P_OFF_REV] < 0.5f && p[P_RV_MIX] > 0 ? 128 : 0)
             | (p[P_OFF_EQ] < 0.5f && (p[P_EQ_LOW] != 0 || p[P_EQ_MID] != 0 || p[P_EQ_HIGH] != 0) ? 256 : 0)
             | (p[P_OFF_OUT] < 0.5f ? 512 : 0);
        if (mask & 256) {
            eqSet(eqc, 0, 200, p[P_EQ_LOW]);
            eqSet(eqc + 5, 1, 1000, p[P_EQ_MID]);
            eqSet(eqc + 10, 2, 5000, p[P_EQ_HIGH]);
        }
        outG = std::pow(10.0, p[P_OUT] / 20.0);
    }

    // section switches apply only while the output is faded out, so toggling never clicks
    void switches() {
        if (mask == act) { duck = std::min(1.0, duck + fade); return; }
        duck = std::max(0.0, duck - fade);
        if (duck > 0) return;
        // a section coming back starts empty, or it would replay the sound it froze with
        if ((mask & 32) && !(act & 32)) {
            std::fill(chl.begin(), chl.end(), 0.0); std::fill(chr.begin(), chr.end(), 0.0);
        }
        if ((mask & 64) && !(act & 64)) {
            std::fill(dbl.begin(), dbl.end(), 0.0); std::fill(dbr.begin(), dbr.end(), 0.0);
            dlpL = dlpR = 0;
        }
        if ((mask & 128) && !(act & 128)) {
            for (AllPass& a : ap) std::fill(a.b.begin(), a.b.end(), 0.0);
            std::fill(rbuf.begin(), rbuf.end(), 0.0);
            std::fill(shb.begin(), shb.end(), 0.0);
            for (double& z : rz) z = 0;
        }
        act = mask;
        ctl = 0;
    }

    // pitch and filter update every 32 samples, to stay cheap
    void control() {
        ctl = 31;
        wp1 += 32 * p[P_WOW_RATE] / sr; if (wp1 >= 1) wp1 -= 1;
        wp2 += 32 * p[P_WOW_RATE] * 0.37 / sr; if (wp2 >= 1) wp2 -= 1;
        // two unrelated sines, so the wobble never settles into an obvious cycle
        wow = (act & 2) ? p[P_WOW_DEPTH] * 0.01 * (std::sin(TWO_PI * wp1) + 0.4 * std::sin(TWO_PI * wp2)) / 1.4 : 0;
        for (Voice& v : voices) {
            if (!v.state) continue;
            const double inc = 440 * std::pow(2.0, (v.note - 69 + pb + wow) / 12) / sr;
            v.ic = inc; v.ia = inc * detRatio; v.ib = inc / detRatio; v.is = inc * 0.5;
            if (act & 4) {
                const double fc = std::max(20.0, std::min(p[P_CUTOFF] * std::pow(2.0, p[P_FENV] * 5 * v.env), sr * 0.45));
                const double fg = std::tan(PI * fc / sr);
                const double fa = 1 / (1 + fg * (fg + fk));
                v.a1 = fa; v.a2 = fg * fa; v.a3 = fg * fg * fa;
            }
        }
    }

    void sample(float& outL, float& outR) {
        if (--ctl < 0) control();

        double sl = 0, sr_ = 0;
        const double nzl = (rnd() * 2 - 1) * p[P_NOISE];
        const double nzr = (rnd() * 2 - 1) * p[P_NOISE];
        for (Voice& v : voices) {
            if (!v.state) continue;
            if (v.state == 1) {
                v.env += atkInc;
                if (v.env >= 1) { v.env = 1; v.state = 2; }
            } else if (v.state == 2) {
                v.env = susV + (v.env - susV) * decCo;
            } else {
                v.env *= relCo;
                if (v.env < 0.0001) { v.env = 0; v.state = 0; }
            }
            if (!(act & 1)) continue;

            v.pc += v.ic; if (v.pc >= 1) v.pc -= 1;
            v.pa += v.ia; if (v.pa >= 1) v.pa -= 1;
            v.pb += v.ib; if (v.pb >= 1) v.pb -= 1;
            v.ps += v.is; if (v.ps >= 1) v.ps -= 1;

            const double oc = osc(v.pc, v.ic);
            const double os = p[P_SUB] > 0 ? p[P_SUB] * std::sin(TWO_PI * v.ps) : 0;
            double xl = (oc + osc(v.pa, v.ia)) * 0.5 + os + nzl;
            double xr = (oc + osc(v.pb, v.ib)) * 0.5 + os + nzr;

            if (act & 4) {
                // TPT state-variable lowpass, stable at any cutoff and resonance
                double t3 = xl - v.l2, t1 = v.a1 * v.l1 + v.a2 * t3;
                xl = v.l2 + v.a2 * v.l1 + v.a3 * t3;
                v.l1 = 2 * t1 - v.l1; v.l2 = 2 * xl - v.l2;
                t3 = xr - v.r2; t1 = v.a1 * v.r1 + v.a2 * t3;
                xr = v.r2 + v.a2 * v.r1 + v.a3 * t3;
                v.r1 = 2 * t1 - v.r1; v.r2 = 2 * xr - v.r2;
            }
            const double g = v.env * v.vel * 0.3;
            sl += xl * g;
            sr_ += xr * g;
        }

        // shortcut: no oversampling, so high notes alias at full Fuzz; oversample if it sounds harsh
        if (act & 16) {
            double x = sl * drvG; sl = x / (1 + std::fabs(x)) * drvOut;
            x = sr_ * drvG; sr_ = x / (1 + std::fabs(x)) * drvOut;
        }

        if (act & 32) {
            chl[cw] = sl; chr[cw] = sr_;
            cp += chInc; if (cp >= 1) cp -= 1;
            const double wl = tap(chl.data(), chn, cw, (0.013 + 0.005 * std::sin(TWO_PI * cp)) * sr);
            const double wr = tap(chr.data(), chn, cw, (0.013 + 0.005 * std::cos(TWO_PI * cp)) * sr);
            sl += (wl - sl) * p[P_CH_MIX];
            sr_ += (wr - sr_) * p[P_CH_MIX];
            if (++cw >= chn) cw = 0;
        }

        // delay: crossed feedback, darkened on every repeat
        if (act & 64) {
            dcur += (dlTarget - dcur) * 0.0002; // glide on time changes, like tape, instead of clicking
            const double wl = tap(dbl.data(), dn, dw, dcur);
            const double wr = tap(dbr.data(), dn, dw, dcur);
            dlpL += (wl - dlpL) * 0.35;
            dlpR += (wr - dlpR) * 0.35;
            dbl[dw] = sl + dlpR * p[P_DL_FB] + DENORMAL_GUARD;
            dbr[dw] = sr_ + dlpL * p[P_DL_FB] + DENORMAL_GUARD;
            if (++dw >= dn) dw = 0;
            sl += wl * p[P_DL_MIX];
            sr_ += wr * p[P_DL_MIX];
        }

        // reverb: 8-line feedback delay network, fed by the signal plus an octave-up copy
        if (act & 128) {
            double il = sl, ir = sr_;
            // shortcut: one octave only, no endless climb; inside the reverb loop it ran away, so that needs a limiter
            if (p[P_RV_SHIM] > 0) {
                shb[sw] = (sl + sr_) * 0.5;
                sp += 1.0 / shw; if (sp >= 1) sp -= 1;
                double sq = sp + 0.5; if (sq >= 1) sq -= 1;
                int d1 = sw - (int)((1 - sp) * shw) - 1; if (d1 < 0) d1 += shn;
                int d2 = sw - (int)((1 - sq) * shw) - 1; if (d2 < 0) d2 += shn;
                const double sh = (shb[d1] * (1 - std::fabs(2 * sp - 1)) + shb[d2] * (1 - std::fabs(2 * sq - 1))) * p[P_RV_SHIM];
                il += sh; ir += sh;
                if (++sw >= shn) sw = 0;
            }
            il = allpass(allpass(il, ap[0]), ap[1]) + DENORMAL_GUARD;
            ir = allpass(allpass(ir, ap[2]), ap[3]) + DENORMAL_GUARD;

            rsz += (p[P_RV_SIZE] - rsz) * 0.0002;
            rl += 0.31 / sr; if (rl >= 1) rl -= 1;
            const double ma = rmod * std::sin(TWO_PI * rl);
            const double mb = rmod * std::cos(TWO_PI * rl);
            double rsum = 0;
            for (int i = 0; i < 8; ++i) {
                const double y = tap(rbuf.data() + i * rn, rn, rw, RLEN[i] * sr * rsz + ((i & 1) ? ma : mb));
                rz[i] += (y - rz[i]) * rvDamp;
                rx[i] = rz[i] * rg[i];
                rsum += rx[i];
            }
            rsum *= 0.25; // Householder feedback: x - (2/N) * sum(x)
            for (int i = 0; i < 8; ++i) rbuf[i * rn + rw] = rx[i] - rsum + ((i & 1) ? ir : il);
            if (++rw >= rn) rw = 0;

            const double wl = (rx[0] + rx[2] + rx[4] + rx[6]) * 0.3;
            const double wr = (rx[1] + rx[3] + rx[5] + rx[7]) * 0.3;
            sl += (wl - sl) * p[P_RV_MIX];
            sr_ += (wr - sr_) * p[P_RV_MIX];
        }

        if (act & 256) {
            sl = bq(bq(bq(sl + DENORMAL_GUARD, eqc, eqs), eqc + 5, eqs + 2), eqc + 10, eqs + 4);
            sr_ = bq(bq(bq(sr_ + DENORMAL_GUARD, eqc, eqs + 6), eqc + 5, eqs + 8), eqc + 10, eqs + 10);
        }

        // hard ceiling: the last line of defence for speakers and ears
        const double g = (act & 512) ? outG * duck : 0;
        outL = (float)std::max(-1.0, std::min(1.0, sl * g));
        outR = (float)std::max(-1.0, std::min(1.0, sr_ * g));
    }
};

} // namespace absentox
