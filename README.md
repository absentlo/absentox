# ABSENToX

A small dream-pop and shoegaze synth, made for low-spec PCs. Use it in [REAPER](https://www.reaper.fm/), or in any DAW that loads VST3 or CLAP.

![ABSENToX](docs/screenshot.png)

## Install

Pick one. They sound and look the same.

| Version | Status | Windows | Linux | Best for |
|---|---|---|---|---|
| **REAPER (JSFX)** | Stable | [Download](https://github.com/absentlo/absentox/releases/latest/download/ABSENToX-Windows.zip) | [Download](https://github.com/absentlo/absentox/releases/latest/download/ABSENToX-Linux.tar.gz) | REAPER. The lightest install: one small text file of about 22 KB, with nothing to compile. |
| **VST3 + CLAP** | Beta | [Download](https://github.com/absentlo/absentox/releases/latest/download/ABSENToX-VST3-CLAP-Windows.zip) | [Download](https://github.com/absentlo/absentox/releases/latest/download/ABSENToX-VST3-CLAP-Linux.tar.gz) | Other DAWs. New, so expect rough edges, and please report any problems. |

**Windows:** unzip it, then run `install.bat`. For VST3 + CLAP, right-click it and choose **Run as administrator**.

**Linux:** extract it and run `./install.sh`.

Then restart your DAW, or rescan your plugins. On a portable REAPER, pass its folder to the REAPER installer, or copy `absentox.jsfx` into its `Effects` folder.

## Play

**In any DAW:** add ABSENToX to an instrument track as VST3 or CLAP, then play or draw some MIDI notes.

**In REAPER:**

1. On a track, open the FX window, click **Add** and search for `ABSENToX`.
2. Arm the track and turn on monitoring.
3. Open **View > Virtual MIDI Keyboard** and hold a chord.

The default sound fades in slowly.

## Controls

- **Drag up or down:** turn a knob.
- **Ctrl + drag:** turn it slowly.
- **Right-click:** reset a knob.
- **Circle at the top right of a section:** switch it on (filled) or off (hollow). Your settings stay.
- **Save a sound:** use your DAW's preset menu. In REAPER, that's the **+** next to the preset list.

## Sections

| Section | Knobs | When OFF |
|---|---|---|
| **OSC** | Wave (saw to square), Detune (width), Sub (octave down), Noise | No new sound; tails keep ringing |
| **WOW** | Rate, Depth: slow pitch drift, like a tremolo arm | Pitch is steady |
| **FILTER** | Cutoff (brightness), Reso (emphasis), Env (how much a note opens or closes it) | Fully open |
| **ENVELOPE** | Attack, Decay, Sustain, Release | Notes start and stop at once |
| **DRIVE** | Fuzz | Clean |
| **CHORUS** | Rate, Mix | Dry |
| **DELAY** | Time, Feedback, Mix | Dry |
| **REVERB** | Size, Decay (up to 30 s), Tone, Shimmer (octave up), Mix | Dry |
| **EQ** | Low 200 Hz, Mid 1 kHz, High 5 kHz | Flat |
| **OUT** | Level | Muted |

## Good to know

- **No sound?** Check that the track is receiving MIDI, and that the **OSC** and **OUT** circles are filled.
- **Save CPU:** turn off what you don't use. **REVERB** costs the most.
- **Limits:** 6 notes at once, pitch bend but no sustain pedal, and delay time doesn't follow tempo.

## Developers

```sh
git clone --recurse-submodules https://github.com/absentlo/absentox.git
make -C native
```

The plugins land in `native/build/`. `absentox.jsfx` and `native/AbsentoxDSP.hpp` are the same synth, so change both. `python3 tests/smoke.py` checks the REAPER version, and `python3 tests/smoke.py native` checks the C++ engine and compares the two.

Every push builds all four downloads, which you can get from its run in the **Actions** tab. Pushing a tag like `v1.1.0` also publishes them as a release.

## License

[MIT](LICENSE). The VST3 and CLAP versions are built with [DPF](https://github.com/DISTRHO/DPF); see [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES).
