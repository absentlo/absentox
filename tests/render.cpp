// Renders the native engine to a float WAV for tests/smoke.py.
//   render --params
//   render out.wav seconds rate "slider=value,..." "beat:length:note,..." "slider|seconds=value,..."
// Sliders are numbered from 1, as in absentox.jsfx. "-" keeps a slider's default.
#include "../native/AbsentoxDSP.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

using namespace absentox;

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream in(s);
    for (std::string item; std::getline(in, item, sep);) if (!item.empty()) out.push_back(item);
    return out;
}

static void u16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void u32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--params") {
        for (int i = 0; i < P_COUNT; ++i)
            printf("%d %g %g %g\n", i + 1, kParams[i].min, kParams[i].max, kParams[i].def);
        return 0;
    }
    if (argc != 7) { fputs("usage: see the top of render.cpp\n", stderr); return 2; }

    const double seconds = atof(argv[2]);
    const int rate = atoi(argv[3]);
    auto engine = new Engine();

    for (const std::string& item : split(argv[4], ',')) {
        const auto kv = split(item, '=');
        if (kv.size() == 2 && kv[1] != "-") engine->set(atoi(kv[0].c_str()) - 1, (float)atof(kv[1].c_str()));
    }
    engine->setup(rate);

    // 120 BPM, as in the REAPER test project
    std::vector<Midi> events;
    uint32_t lastOff = 0;
    for (const std::string& item : split(argv[5], ',')) {
        const auto n = split(item, ':');
        const uint32_t on = (uint32_t)(atof(n[0].c_str()) * 0.5 * rate);
        const uint32_t off = (uint32_t)((atof(n[0].c_str()) + atof(n[1].c_str())) * 0.5 * rate);
        const uint8_t note = (uint8_t)atoi(n[2].c_str());
        events.push_back({on, 0x90, note, 110});
        events.push_back({off, 0x80, note, 0});
        lastOff = std::max(lastOff, off);
    }
    events.push_back({lastOff + (uint32_t)(0.5 * rate), 0xB0, 123, 0});
    std::stable_sort(events.begin(), events.end(), [](const Midi& a, const Midi& b) { return a.frame < b.frame; });

    int switchSlider = 0;
    std::vector<std::pair<uint32_t, float>> steps;
    const auto sw = split(argv[6], '|');
    if (sw.size() == 2) {
        switchSlider = atoi(sw[0].c_str());
        for (const std::string& item : split(sw[1], ',')) {
            const auto kv = split(item, '=');
            steps.push_back({(uint32_t)(atof(kv[0].c_str()) * rate), (float)atof(kv[1].c_str())});
        }
    }

    // one second of tail, as in the REAPER test project
    const uint32_t total = (uint32_t)((seconds + 1) * rate);
    std::vector<float> stereo(total * 2);
    std::vector<float> l(512), r(512);
    std::vector<Midi> chunk;
    size_t nextEvent = 0, nextStep = 0;
    for (uint32_t pos = 0; pos < total; pos += 512) {
        const uint32_t n = std::min<uint32_t>(512, total - pos);
        while (nextStep < steps.size() && steps[nextStep].first <= pos)
            engine->set(switchSlider - 1, steps[nextStep++].second);
        chunk.clear();
        while (nextEvent < events.size() && events[nextEvent].frame < pos + n) {
            Midi m = events[nextEvent++];
            m.frame -= pos;
            chunk.push_back(m);
        }
        engine->run(l.data(), r.data(), n, chunk.data(), (uint32_t)chunk.size());
        for (uint32_t i = 0; i < n; ++i) { stereo[(pos + i) * 2] = l[i]; stereo[(pos + i) * 2 + 1] = r[i]; }
    }
    delete engine;

    FILE* f = fopen(argv[1], "wb");
    if (!f) { perror(argv[1]); return 1; }
    const uint32_t bytes = total * 8;
    fwrite("RIFF", 1, 4, f); u32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    u32(f, 16); u16(f, 3); u16(f, 2); u32(f, rate); u32(f, rate * 8); u16(f, 8); u16(f, 32);
    fwrite("data", 1, 4, f); u32(f, bytes);
    fwrite(stereo.data(), 4, stereo.size(), f);
    return fclose(f) == 0 ? 0 : 1;
}
