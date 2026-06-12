# Playback Effects Design

## Goal

Add the course-design extension features as real-time playback effects without changing the recorded `record.wav` file.

## User Interaction

- `PA0` keeps toggling recording.
- `PE8` keeps toggling playback.
- `PC13` cycles the playback effect mode.
- The OLED shows the current mode as `FX:n`, where `0` is normal playback and later values select effects.

## Effect Modes

The playback path cycles through these modes:

- `0 Normal`: unmodified playback.
- `1 Pitch`: simple pitch/voice change by resampling each playback block.
- `2 Echo`: original signal plus delayed feedback.
- `3 Mix`: original signal plus two delayed voices for a multi-signal layered effect.
- `4 Slow`: speech remains recognizable while duration is increased by repeating/interpolating samples.
- `5 Filter`: one-pole digital low-pass filter.

## Architecture

The existing `fill_playback_segment()` function is the single point that reads mono WAV samples and writes stereo I2S TX samples. The effect processor will run inside this path, after SD read and before the samples are copied into the I2S buffer. This keeps recording untouched and keeps SD file format simple.

Effect state such as echo delay buffers and slow-playback phase is reset when playback starts and when the effect mode changes. Saturating arithmetic is used to avoid wraparound distortion.

## Testing

Static tests will verify:

- PC13 is used to cycle effect modes.
- `AudioApp_GetFxMode()` exposes the current mode.
- `fill_playback_segment()` calls the effect processor before writing stereo output.
- OLED renders `FX:%d`.
- The effect code includes pitch, echo, mix, slow, and filter paths with saturation.
