#include "DistrhoUI.hpp"

#include "AbsentoxDSP.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

using namespace absentox;

namespace {

struct KnobDef { int param; const char* label; const char* fmt; };
struct SectionDef { const char* name; int off; float extraWidth; int count; KnobDef knobs[5]; };

// one knob is too narrow for a title and its switch, so DRIVE gets half a knob of extra width
const SectionDef kRows[3][4] = {
    {{"OSC", P_OFF_OSC, 0, 4, {{P_WAVE, "Wave", "%.2f"}, {P_DETUNE, "Detune", "%.1f ct"}, {P_SUB, "Sub", "%.2f"}, {P_NOISE, "Noise", "%.2f"}}},
     {"WOW", P_OFF_WOW, 0, 2, {{P_WOW_RATE, "Rate", "%.2f Hz"}, {P_WOW_DEPTH, "Depth", "%.1f ct"}}},
     {"FILTER", P_OFF_FLT, 0, 3, {{P_CUTOFF, "Cutoff", "%.0f Hz"}, {P_RESO, "Reso", "%.2f"}, {P_FENV, "Env", "%+.2f"}}},
     {nullptr, 0, 0, 0, {}}},
    {{"ENVELOPE", P_OFF_ENV, 0, 4, {{P_ATK, "Attack", "%.0f ms"}, {P_DEC, "Decay", "%.0f ms"}, {P_SUS, "Sustain", "%.2f"}, {P_REL, "Release", "%.0f ms"}}},
     {"DRIVE", P_OFF_DRV, 0.5f, 1, {{P_DRIVE, "Fuzz", "%.2f"}}},
     {"CHORUS", P_OFF_CHO, 0, 2, {{P_CH_RATE, "Rate", "%.2f Hz"}, {P_CH_MIX, "Mix", "%.2f"}}},
     {"DELAY", P_OFF_DLY, 0, 3, {{P_DL_TIME, "Time", "%.0f ms"}, {P_DL_FB, "Feedback", "%.2f"}, {P_DL_MIX, "Mix", "%.2f"}}}},
    {{"REVERB", P_OFF_REV, 0, 5, {{P_RV_SIZE, "Size", "%.2f"}, {P_RV_DECAY, "Decay", "%.1f s"}, {P_RV_TONE, "Tone", "%.2f"}, {P_RV_SHIM, "Shimmer", "%.2f"}, {P_RV_MIX, "Mix", "%.2f"}}},
     {"EQ", P_OFF_EQ, 0, 3, {{P_EQ_LOW, "Low 200", "%+.1f dB"}, {P_EQ_MID, "Mid 1k", "%+.1f dB"}, {P_EQ_HIGH, "High 5k", "%+.1f dB"}}},
     {"OUT", P_OFF_OUT, 0, 1, {{P_OUT, "Level", "%.1f dB"}}},
     {nullptr, 0, 0, 0, {}}},
};

const float W = DISTRHO_UI_DEFAULT_WIDTH, H = DISTRHO_UI_DEFAULT_HEIGHT;
const float MARGIN = 3, TOP = 28, HEADER = 20, RADIUS = 17, ARC_START = -2.356f, ARC_RANGE = 4.712f;
const float ROW_H = (H - TOP - MARGIN) / 3;

float toNormal(int param, float v) {
    const ParamInfo& k = kParams[param];
    return k.log ? std::log(v / k.min) / std::log(k.max / k.min) : (v - k.min) / (k.max - k.min);
}

float fromNormal(int param, float n) {
    const ParamInfo& k = kParams[param];
    return k.log ? k.min * std::pow(k.max / k.min, n) : k.min + (k.max - k.min) * n;
}

} // namespace

class AbsentoxUI : public UI {
public:
    AbsentoxUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true) {
        for (int i = 0; i < P_COUNT; ++i) values[i] = kParams[i].def;
        loadSharedResources();
        layout();
    }

protected:
    void parameterChanged(uint32_t index, float value) override {
        if (index >= P_COUNT) return;
        values[index] = value;
        repaint();
    }

    void onNanoDisplay() override {
        beginPath(); rect(0, 0, W, H); fillColor(PAPER); fill();
        fontFaceId(findFont(NANOVG_DEJAVU_SANS_TTF));

        fontSize(16); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(INK);
        bold(8, 14, "ABSENToX");
        fontSize(11); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(SOFT);
        text(W - 7, 14, "Ctrl: fine    Right-click: reset", nullptr);

        for (const Section& s : sections) {
            const bool on = values[s.def->off] < 0.5f;
            const Color& ink = on ? INK : SOFT;
            beginPath(); rect(s.x, s.y, s.w, ROW_H - MARGIN); fillColor(CARD); fill();

            fontSize(11.5f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(ink);
            bold(s.x + 5, s.y + HEADER / 2, s.def->name);

            // a filled circle is on, a hollow one is off
            beginPath(); circle(s.dotX, s.dotY, on ? 5 : 4.25f);
            if (on) { fillColor(ink); fill(); }
            else { strokeColor(ink); strokeWidth(1.5f); stroke(); }

            fontSize(11.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            for (int i = 0; i < s.def->count; ++i) knob(s.def->knobs[i], s.knobX[i], s.y + HEADER + (ROW_H - MARGIN - HEADER) / 2 - 1, ink);
        }
    }

    bool onMouse(const MouseEvent& ev) override {
        if (ev.button != kMouseButtonLeft && ev.button != kMouseButtonRight) return false;
        if (!ev.press) {
            if (drag >= 0 && ev.button == kMouseButtonLeft) { editParameter(drag, false); drag = -1; return true; }
            return false;
        }
        const float x = ev.pos.getX(), y = ev.pos.getY();
        for (const Section& s : sections) {
            if (x < s.x || x >= s.x + s.w || y < s.y || y >= s.y + ROW_H - MARGIN) continue;
            if (y < s.y + HEADER) {
                if (ev.button != kMouseButtonLeft || std::fabs(x - s.dotX) > 9 || std::fabs(y - s.dotY) > 9) return false;
                send(s.def->off, values[s.def->off] < 0.5f ? 1.0f : 0.0f);
                return true;
            }
            const int i = (int)((x - s.knobLeft) / s.knobW);
            if (i < 0 || i >= s.def->count) return false;
            const int param = s.def->knobs[i].param;
            if (ev.button == kMouseButtonRight) {
                send(param, kParams[param].def);
            } else {
                drag = param; dragN = toNormal(param, values[param]); dragY = y;
                editParameter(param, true);
            }
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        if (drag < 0) return false;
        const float y = ev.pos.getY();
        // knobs turn with vertical drags: 200 px is the full range, 2000 px with Ctrl
        dragN = std::max(0.0f, std::min(1.0f, dragN + (dragY - y) / ((ev.mod & kModifierControl) ? 2000.0f : 200.0f)));
        dragY = y;
        values[drag] = fromNormal(drag, dragN);
        setParameterValue(drag, values[drag]);
        repaint();
        return true;
    }

private:
    struct Section {
        const SectionDef* def;
        float x, y, w, dotX, dotY, knobLeft, knobW, knobX[5];
    };

    const Color PAPER = Color(0.95f, 0.94f, 0.92f), CARD = Color(0.90f, 0.89f, 0.86f);
    const Color INK = Color(0.27f, 0.27f, 0.27f), SOFT = Color(0.62f, 0.61f, 0.59f);

    float values[P_COUNT];
    Section sections[10];
    int drag = -1;
    float dragN = 0, dragY = 0;

    void layout() {
        int n = 0;
        for (int row = 0; row < 3; ++row) {
            float units = 0; int groups = 0;
            for (const SectionDef& d : kRows[row]) if (d.name) { units += d.count + d.extraWidth; ++groups; }
            const float cell = (W - MARGIN * (groups + 1)) / units;
            float x = 0;
            for (const SectionDef& d : kRows[row]) {
                if (!d.name) continue;
                x += MARGIN;
                Section& s = sections[n++];
                s.def = &d;
                s.x = x; s.y = TOP + row * ROW_H; s.w = (d.count + d.extraWidth) * cell;
                s.dotX = s.x + s.w - 10; s.dotY = s.y + HEADER / 2;
                s.knobLeft = x; s.knobW = s.w / d.count;
                for (int i = 0; i < d.count; ++i) s.knobX[i] = x + s.knobW * (i + 0.5f);
                x += s.w;
            }
        }
    }

    void bold(float x, float y, const char* s) {
        // the bundled font has no bold weight
        text(x, y, s, nullptr);
        text(x + 0.4f, y, s, nullptr);
    }

    void send(int param, float v) {
        values[param] = v;
        editParameter(param, true);
        setParameterValue(param, v);
        editParameter(param, false);
        repaint();
    }

    void knob(const KnobDef& k, float cx, float cy, const Color& ink) {
        const ParamInfo& info = kParams[k.param];
        const float v = values[k.param];
        const float a = ARC_START + ARC_RANGE * toNormal(k.param, v);
        // symmetric ranges (EQ, Env) grow from the top
        const float a0 = info.min == -info.max ? 0 : ARC_START;
        const float from = std::min(a0, a) - 1.5707963f, to = std::max(a0, a) - 1.5707963f;

        strokeColor(ink); strokeWidth(3); lineCap(ROUND);
        beginPath();
        arc(cx, cy, RADIUS - 1.5f, from, to + 0.001f, CW);
        stroke();

        const float cap = RADIUS * 0.7f;
        beginPath(); circle(cx, cy, cap); fillColor(ink); fill();
        strokeColor(PAPER); strokeWidth(1.6f); lineCap(ROUND);
        beginPath();
        moveTo(cx + std::sin(a) * cap * 0.3f, cy - std::cos(a) * cap * 0.3f);
        lineTo(cx + std::sin(a) * cap * 0.95f, cy - std::cos(a) * cap * 0.95f);
        stroke();

        fillColor(ink);
        text(cx, cy - RADIUS - 10, k.label, nullptr);
        char value[32];
        std::snprintf(value, sizeof(value), k.fmt, v);
        text(cx, cy + RADIUS + 10, value, nullptr);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbsentoxUI)
};

UI* createUI() { return new AbsentoxUI(); }

END_NAMESPACE_DISTRHO
