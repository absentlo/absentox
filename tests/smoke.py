#!/usr/bin/env python3
"""Checks the audio of both versions of ABSENToX.
  python3 tests/smoke.py          the JSFX version, rendered through REAPER
  python3 tests/smoke.py native   the C++ engine, plus a comparison against the JSFX version
REAPER needs to be on PATH with a display. It uses a throwaway config, not your REAPER settings."""
import atexit, cmath, math, os, re, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLIDERS = 38
NATIVE = len(sys.argv) > 1 and sys.argv[1] == "native"
RENDER_BIN = None
WAVE, DETUNE, SUB, NOISE, WOW_DEPTH = 1, 2, 3, 4, 6
CUTOFF, RESO, FENV, ATK, REL = 7, 8, 9, 10, 13
DRIVE, CH_MIX, DL_FB, DL_MIX = 14, 16, 18, 19
RV_DECAY, RV_SHIM, RV_MIX, EQ_LOW, OUT = 21, 23, 24, 25, 28
OFF_OSC, OFF_FLT, OFF_ENV, OFF_DRV, OFF_DLY, OFF_REV, OFF_OUT = 29, 31, 32, 33, 35, 36, 38
FX_OFF = {SUB: 0, NOISE: 0, WOW_DEPTH: 0, DRIVE: 0, CH_MIX: 0, DL_MIX: 0, RV_MIX: 0, ATK: 1}


def project(path, out, params, notes, seconds, rate, switch=None):
    """notes: (start_beat, length_beats, midi_note). 120 BPM, 960 ticks per beat.
    switch: (slider, [(seconds, value), ...]) automates one slider with square steps."""
    src = open(os.path.join(ROOT, "absentox.jsfx")).read()
    defaults = {int(n): v for n, v in re.findall(r"^slider(\d+):\w+=(-?[\d.]+)", src, re.M)}
    assert len(defaults) == SLIDERS, f"expected {SLIDERS} sliders, found {len(defaults)}"
    vals = " ".join(str(params.get(i, defaults.get(i, "-"))) for i in range(1, 65))
    events, last = [], 0
    for tick, status, note, vel in sorted(
        [(int(s * 960), 0x90, n, 110) for s, _, n in notes]
        + [(int((s + l) * 960), 0x80, n, 0) for s, l, n in notes]
    ):
        events.append(f"E {tick - last} {status:02x} {note:02x} {vel:02x}")
        last = tick
    body = "\n".join(events)
    env = ""
    if switch:
        points = "\n".join(f"        PT {at} {v} 1" for at, v in switch[1])
        env = f"<PARMENV {switch[0] - 1} 0 1 0\n        ACT 1 -1\n        VIS 1 1 1\n        ARM 0\n        DEFSHAPE 1 -1 -1\n{points}\n      >"
    with open(path, "w") as f:
        f.write(f"""<REAPER_PROJECT 0.1 "7.0" 0
  TEMPO 120 4 4
  RENDER_FILE "{out}"
  RENDER_PATTERN ""
  RENDER_FMT 0 2 {rate}
  RENDER_RANGE 1 0 {seconds} 18 1000
  RENDER_1X 0
  RENDER_RESAMPLE 3 0 1
  RENDER_STEMS 0
  RENDER_DITHER 0
  SAMPLERATE {rate} 0 0
  <RENDER_CFG
    ZXZhdyABAA==
  >
  <TRACK
    NAME absentox
    NCHAN 2
    MAINSEND 1 0
    <FXCHAIN
      SHOW 0
      LASTSEL 0
      DOCKED 0
      BYPASS 0 0 0
      <JS absentox.jsfx ""
        {vals}
      >
      FLOATPOS 0 0 0 0
      {env}
      WAK 0 0
    >
    <ITEM
      POSITION 0
      LENGTH {seconds}
      LOOP 0
      <SOURCE MIDI
        HASDATA 1 960 QN
{body}
        E 960 b0 7b 00
      >
    >
  >
>
""")


def render(tmp, name, params, notes, seconds, rate=44100, switch=None, native=None):
    native = NATIVE if native is None else native
    rpp, out = os.path.join(tmp, name + ".rpp"), os.path.join(tmp, name + ".wav")
    if native:
        subprocess.run([
            RENDER_BIN, out, str(seconds), str(rate),
            ",".join(f"{k}={v}" for k, v in params.items()),
            ",".join(f"{s}:{l}:{n}" for s, l, n in notes),
            f"{switch[0]}|" + ",".join(f"{at}={v}" for at, v in switch[1]) if switch else "",
        ], check=True, timeout=120)
    else:
        project(rpp, out, params, notes, seconds, rate, switch)
        subprocess.run(
            ["reaper", "-cfgfile", os.path.join(tmp, "cfg", "reaper.ini"), "-newinst", "-nosplash",
             "-ignoreerrors", "-renderproject", rpp],
            timeout=180, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
    if not os.path.exists(out):
        sys.exit(f"FAIL {name}: REAPER produced no file")
    # the wave module cannot read float WAVs, and float is what shows overs past 1.0
    with open(out, "rb") as f:
        buf = f.read()
    pos, raw, fmt = 12, None, None
    while pos + 8 <= len(buf):
        cid, size = buf[pos:pos + 4], struct.unpack("<I", buf[pos + 4:pos + 8])[0]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", buf[pos + 8:pos + 24])
        elif cid == b"data":
            raw = buf[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
    tag, ch, rate, _, _, bits = fmt
    assert (tag, ch, bits) == (3, 2, 32), f"{name}: expected 32-bit float stereo, got {fmt}"
    data = struct.unpack(f"<{len(raw) // 4}f", raw)
    return data[0::2], data[1::2], rate


def rms(x):
    return math.sqrt(sum(s * s for s in x) / max(1, len(x)))


def peak(x):
    return max((abs(s) for s in x), default=0)


def level_at(x, rate, hz):
    return abs(sum(s * cmath.exp(-2j * math.pi * hz * i / rate) for i, s in enumerate(x))) / len(x)


def check(ok, msg):
    print(("ok   " if ok else "FAIL ") + msg)
    return ok


def main():
    tmp = tempfile.mkdtemp(prefix="absentox-")
    atexit.register(shutil.rmtree, tmp, True)
    os.makedirs(os.path.join(tmp, "cfg", "Effects"))
    shutil.copy(os.path.join(ROOT, "absentox.jsfx"), os.path.join(tmp, "cfg", "Effects"))
    open(os.path.join(tmp, "cfg", "reaper.ini"), "w").close()
    good = True

    if NATIVE:
        global RENDER_BIN
        RENDER_BIN = os.path.join(tmp, "render")
        subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-o", RENDER_BIN,
                        os.path.join(ROOT, "tests", "render.cpp")], check=True)
        # the two versions must agree on every control
        src = open(os.path.join(ROOT, "absentox.jsfx")).read()
        jsfx = re.findall(r"^slider(\d+):\w+=(-?[\d.]+)<(-?[\d.]+),(-?[\d.]+)", src, re.M)
        cpp = subprocess.run([RENDER_BIN, "--params"], check=True, capture_output=True, text=True).stdout.split("\n")
        same = len(jsfx) == SLIDERS and all(
            all(abs(float(x) - float(y)) < 1e-4 for x, y in zip(cpp[i].split()[1:], (lo, hi, df)))
            for i, (_, df, lo, hi) in enumerate(jsfx)
        )
        good &= check(same, "native controls match the JSFX ranges and defaults")

    # defaults: a chord makes stereo sound and stays under the ceiling
    l, r, sr = render(tmp, "default", {}, [(0, 4, 48), (0, 4, 55), (0, 4, 64)], 8)
    good &= check(rms(l) > 0.01, f"default is audible (rms {rms(l):.3f})")
    good &= check(peak(l) <= 1 and peak(r) <= 1, f"default within ceiling (peak {peak(l):.3f})")
    side = rms([a - b for a, b in zip(l, r)])
    good &= check(side > 0.005, f"default is stereo (side rms {side:.3f})")

    # dry A4 is in tune, and the note stops after release
    dry = {**FX_OFF, DETUNE: 0, CUTOFF: 18000, FENV: 0, REL: 50, OUT: -6}
    l, r, sr = render(tmp, "dry", dry, [(0, 2, 69)], 3)
    win = l[int(0.2 * sr):int(0.7 * sr)]
    on, off = level_at(win, sr, 440), max(level_at(win, sr, 415), level_at(win, sr, 466))
    good &= check(on > 10 * off, f"A4 sounds at 440 Hz ({on:.4f} vs {off:.4f} a semitone away)")
    good &= check(rms(l[int(1.5 * sr):]) < 1e-4, "voice is silent after release")

    # 8 notes on 6 voices: stealing must not hang or blow up
    l, r, sr = render(tmp, "steal", {**FX_OFF, REL: 50}, [(0, 2, 40 + i) for i in range(8)], 3)
    good &= check(rms(l[:sr]) > 0.01, "8-note chord still sounds")
    good &= check(rms(l[int(1.5 * sr):]) < 1e-4, "no stuck voices after stealing")

    # every feedback path at maximum: the tail must fall, not grow
    stress = {WAVE: 1, DETUNE: 50, SUB: 1, NOISE: 1, RESO: 1, CUTOFF: 18000, DRIVE: 1, CH_MIX: 1,
              DL_FB: 0.9, DL_MIX: 1, RV_DECAY: 30, RV_SHIM: 1, RV_MIX: 1, EQ_LOW: 12, OUT: -36}  # low output, so the ceiling cannot hide growth
    l, r, sr = render(tmp, "stress", stress, [(0, 4, 36), (0, 4, 60)], 30)
    a, b = rms(l[2 * sr:4 * sr]), rms(l[28 * sr:30 * sr])
    good &= check(all(math.isfinite(s) for s in l), "stress output is finite")
    good &= check(b < a, f"stress tail decays (rms {a:.3f} -> {b:.3f})")

    # buffers are sized from srate, so a high rate must still work
    l, r, sr = render(tmp, "hirate", {}, [(0, 2, 60)], 3, rate=96000)
    good &= check(sr == 96000 and rms(l) > 0.01, f"renders at {sr} Hz (rms {rms(l):.3f})")

    # section switches
    chord = [(0, 4, 48), (0, 4, 55), (0, 4, 64)]
    for name, slider in (("OSC", OFF_OSC), ("OUT", OFF_OUT)):
        l, r, sr = render(tmp, "off" + name, {slider: 1}, chord, 3)
        good &= check(peak(l) < 1e-4, f"{name} off is silent (peak {peak(l):.6f})")

    dark = {**FX_OFF, CUTOFF: 200, FENV: 0}
    on = rms(render(tmp, "flton", dark, [(0, 2, 60)], 2)[0])
    off = rms(render(tmp, "fltoff", {**dark, OFF_FLT: 1}, [(0, 2, 60)], 2)[0])
    good &= check(off > 2 * on, f"FILTER off lets the sound through (rms {on:.3f} -> {off:.3f})")

    slow = {**FX_OFF, ATK: 8000}
    head = lambda x, rate: rms(x[int(0.05 * rate):int(0.2 * rate)])
    l, r, sr = render(tmp, "envon", slow, [(0, 2, 60)], 2)
    on = head(l, sr)
    l, r, sr = render(tmp, "envoff", {**slow, OFF_ENV: 1}, [(0, 2, 60)], 2)
    good &= check(head(l, sr) > 5 * on, f"ENVELOPE off opens at once (rms {on:.3f} -> {head(l, sr):.3f})")
    good &= check(rms(l[int(1.2 * sr):]) < 1e-4, "ENVELOPE off still stops the note")

    long_tail = {**FX_OFF, RV_MIX: 1, RV_DECAY: 30, REL: 50}
    l, r, sr = render(tmp, "revoff", {**long_tail, OFF_REV: 1}, [(0, 2, 60)], 3)
    good &= check(rms(l[int(1.5 * sr):]) < 1e-4, "REVERB off leaves no tail")

    # a project saved before the switches existed must load with every section on
    old = {i: "-" for i in range(29, 39)}
    a = rms(render(tmp, "legacy", old, chord, 3)[0])
    b = rms(render(tmp, "current", {}, chord, 3)[0])
    good &= check(a > 0.5 * b, f"older projects still sound (rms {a:.3f} vs {b:.3f})")

    # switching a reverb back on must not replay the tail it froze with
    l, r, sr = render(tmp, "stale", long_tail, [(0, 2, 60)], 6, switch=(OFF_REV, [(0, 0), (2, 1), (4, 0)]))
    held = rms(l[int(1.5 * sr):int(1.9 * sr)])
    back = rms(l[int(4.1 * sr):int(5 * sr)])
    good &= check(held > 0.001 and back < 1e-4, f"REVERB back on starts empty (tail {held:.4f}, after {back:.6f})")

    # switching mid-note must fade, not click
    tone = {**FX_OFF, CUTOFF: 400, FENV: 0, OUT: -6}
    l, r, sr = render(tmp, "click", tone, [(0, 4, 48)], 2, switch=(OFF_DRV, [(0, 0), (1, 1)]))
    step = lambda x: max(abs(x[i + 1] - x[i]) for i in range(len(x) - 1))
    before, around = step(l[int(0.6 * sr):int(0.9 * sr)]), step(l[int(0.98 * sr):int(1.03 * sr)])
    good &= check(around < 3 * before, f"toggling mid-note does not click (step {before:.4f} -> {around:.4f})")

    if NATIVE:
        # phases start at random, so compare levels over time rather than samples
        patches = {
            "default": ({}, chord),
            "dry": ({**FX_OFF, DETUNE: 0, CUTOFF: 18000, FENV: 0}, [(0, 4, 57)]),
            "full": ({WAVE: 0.5, DRIVE: 0.8, DL_MIX: 0.5, RV_SHIM: 1, EQ_LOW: 6}, chord),
        }
        for name, (params, notes) in patches.items():
            a, _, sr = render(tmp, "cmpn" + name, params, notes, 4, native=True)
            b, _, _ = render(tmp, "cmpj" + name, params, notes, 4, native=False)
            # short windows swing by over 25% between two runs of the same version, so use the whole note
            x, y = rms(a[:4 * sr]), rms(b[:4 * sr])
            diff = abs(x - y) / y
            good &= check(diff < 0.15, f"native sounds as loud as JSFX, patch {name} ({x:.3f} vs {y:.3f}, {diff * 100:.1f}%)")

    print("PASS" if good else "FAILED")
    sys.exit(0 if good else 1)


if __name__ == "__main__":
    main()
