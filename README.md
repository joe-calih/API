# Slate Comp

Slate Comp is an original stereo compressor plug-in with a compact analog-inspired control panel. It is not affiliated with or a recreation of another manufacturer's product or interface.

The project is licensed under GNU AGPLv3; see [LICENSE](./LICENSE). JUCE is separately licensed by its authors under AGPLv3 or a commercial JUCE licence.

## Features

- VST3 and standalone Windows targets, built from the same audio engine.
- Threshold, attack, ratio, release, variable release, output, knee, thrust, stereo link, wet/dry mix, and trim controls.
- Selectable detector filtering, feed-forward/feedback character, analog saturation, manual makeup, and input/output/gain-reduction metering.
- Five built-in starting presets: Default, Vocal Leveling, Drum Bus, Mix Bus, and Peak Catcher.
- Host state save/restore for DAW projects.

The compressor uses a smoothed peak detector, soft- or hard-knee transfer curve, linked stereo detection, and per-sample gain smoothing. Presets are starting points; adjust them to suit the source and gain staging.

## Build on Windows

Install Visual Studio 2022 with the **Desktop development with C++** workload and CMake 3.22 or newer. Then build from a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

JUCE 8.0.8 is fetched from its public repository during CMake configuration. Build products are placed under `build/SlateComp_artefacts/Release/`:

- `VST3/Slate Comp.vst3` — copy the bundle into a VST3 scan folder, then rescan plug-ins in FL Studio.
- `Standalone/Slate Comp.exe` — standalone application.

## GitHub Actions

Pushing to `main` or starting the **Windows plugin build** workflow manually builds both targets on a Windows runner and publishes them as a downloadable workflow artifact.
