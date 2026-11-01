#include "DistrhoPlugin.hpp"

#include "AbsentoxDSP.hpp"

START_NAMESPACE_DISTRHO

class AbsentoxPlugin : public Plugin {
public:
    AbsentoxPlugin() : Plugin(absentox::P_COUNT, 0, 0) { engine.setup(getSampleRate()); }

protected:
    const char* getLabel() const override { return "ABSENToX"; }
    const char* getDescription() const override { return "A small dream-pop and shoegaze synth."; }
    const char* getMaker() const override { return "absentlo"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1, 1, 0); }

    void initParameter(uint32_t index, Parameter& parameter) override {
        const absentox::ParamInfo& k = absentox::kParams[index];
        parameter.hints = kParameterIsAutomatable;
        if (k.log) parameter.hints |= kParameterIsLogarithmic;
        if (index >= absentox::P_OFF_OSC) parameter.hints |= kParameterIsBoolean | kParameterIsInteger;
        parameter.name = k.name;
        // numbered, so saved projects keep working if a name ever changes
        parameter.symbol = String("p") + String(index + 1);
        parameter.ranges.min = k.min;
        parameter.ranges.max = k.max;
        parameter.ranges.def = k.def;
    }

    float getParameterValue(uint32_t index) const override { return engine.get((int)index); }
    void setParameterValue(uint32_t index, float value) override { engine.set((int)index, value); }

    // no reset on activate: CLAP hosts can restart a plugin mid-render, which would cut off every note and tail
    void sampleRateChanged(double newSampleRate) override { engine.setup(newSampleRate); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* events, uint32_t count) override {
        uint32_t done = 0;
        for (uint32_t i = 0; i < count; ++i) {
            const MidiEvent& e = events[i];
            if (e.size > MidiEvent::kDataSize) continue;
            // a host could hand us an event past the end of the block
            const uint32_t at = e.frame < frames ? e.frame : frames;
            if (at > done) {
                engine.run(outputs[0] + done, outputs[1] + done, at - done, nullptr, 0);
                done = at;
            }
            engine.midi({0, e.data[0], e.size > 1 ? e.data[1] : (uint8_t)0, e.size > 2 ? e.data[2] : (uint8_t)0});
        }
        if (frames > done) engine.run(outputs[0] + done, outputs[1] + done, frames - done, nullptr, 0);
    }

private:
    absentox::Engine engine;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AbsentoxPlugin)
};

Plugin* createPlugin() { return new AbsentoxPlugin(); }

END_NAMESPACE_DISTRHO
