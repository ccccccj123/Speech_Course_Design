# Playback Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add PC13-selectable real-time playback effects for the course-design extension requirements.

**Architecture:** Keep all playback DSP inside `Core/Src/audio_app.c` so the Keil project does not need a new source file. Expose the current mode through `AudioApp_GetFxMode()` and render it in `Core/Src/oled_ui.c`.

**Tech Stack:** STM32 HAL, FatFs, ES8388, I2S DMA, Python static tests.

---

### Task 1: Add Effect Mode State And PC13 Switching

**Files:**
- Modify: `Core/Inc/audio_app.h`
- Modify: `Core/Src/audio_app.c`
- Test: `tests/test_playback_effect_modes.py`

- [ ] Write a failing static test that checks for `AudioFxMode`, `AudioApp_GetFxMode`, `cycle_fx_mode`, and PC13 calling the mode cycle.
- [ ] Run `python tests/test_playback_effect_modes.py` and confirm failure.
- [ ] Add the enum, getter, state variable, and PC13 switching implementation.
- [ ] Run the targeted test and confirm pass.

### Task 2: Add Playback DSP Processing

**Files:**
- Modify: `Core/Src/audio_app.c`
- Test: `tests/test_playback_effect_modes.py`

- [ ] Add failing tests for `apply_playback_effects`, pitch, echo, mix, slow, filter, and saturation.
- [ ] Run the test and confirm failure.
- [ ] Implement the minimal effect processor and reset state when playback starts or the mode changes.
- [ ] Run the targeted test and confirm pass.

### Task 3: Show Effect Mode On OLED

**Files:**
- Modify: `Core/Src/oled_ui.c`
- Test: `tests/test_oled_state_text.py`

- [ ] Add failing tests that check `FX:%d`, `AudioApp_GetFxMode`, and glyphs for `F` and `X`.
- [ ] Run the OLED test and confirm failure.
- [ ] Render `FX:n` on the OLED and add missing glyphs.
- [ ] Run the OLED test and confirm pass.

### Task 4: Full Verification

**Files:**
- Existing tests only.

- [ ] Run `python -m unittest discover -s tests`.
- [ ] Check whether a local Keil command-line build is available; if not, report that firmware compile must be done in Keil.
