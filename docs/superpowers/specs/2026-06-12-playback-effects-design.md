# Playback Effects Design

## Goal

Add the course-design extension features as real-time playback effects without changing the recorded `record.wav` file.

## User Interaction

- `PA0` keeps toggling recording. In normal effect modes it records `record.wav`; in the extra `REC2` mode it records `mix.wav`.
- `PE8` keeps toggling playback.
- `PC13` cycles the mode selector.
- The OLED shows the current playback effect as `FX:n`. The extra second-recording target is shown as `REC2`.

## Effect Modes

The playback path cycles through these modes:

- `0 Normal`: unmodified playback.
- `1 Pitch`: simple pitch/voice change by resampling each playback block.
- `2 Echo`: original signal plus delayed feedback.
- `3 Mix`: reads `record.wav` and `mix.wav` as two independent WAV sources and blends them with 1/2 + 1/2 weighting.
- `4 Slow`: speech remains recognizable while duration is increased by repeating/interpolating samples.
- `5 Filter`: one-pole digital low-pass filter.
- `REC2`: not a playback effect. It selects `mix.wav` as the next recording target so the user can record the second source for `FX:3`.

## Architecture

The existing `fill_playback_segment()` function is the single point that reads mono WAV samples and writes stereo I2S TX samples. The effect processor will run inside this path, after SD read and before the samples are copied into the I2S buffer. Most effects process `record.wav` only.

For `FX:3`, playback opens `mix.wav` as a second optional source. If `mix.wav` exists, each output sample is `record / 2 + mix / 2` with saturation. If `mix.wav` is missing or ends early, the missing second source is treated as silence so playback can continue.

Effect state such as echo delay buffers and slow-playback phase is reset when playback starts and when the effect mode changes. Saturating arithmetic is used to avoid wraparound distortion.

## Testing

Static tests will verify:

- PC13 is used to cycle effect modes.
- `AudioApp_GetFxMode()` exposes the current mode.
- `fill_playback_segment()` calls the effect processor before writing stereo output.
- OLED renders `FX:%d`.
- OLED renders `REC2` for the second recording target.
- The effect code includes pitch, echo, dual-file mix, slow, and filter paths with saturation.
