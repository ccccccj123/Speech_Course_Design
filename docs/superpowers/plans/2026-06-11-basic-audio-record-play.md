# Basic Audio Record Play Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the basic course-design firmware path: PA0 toggles WAV recording to TF card, PE8 toggles playback, and PC13 is reserved.

**Architecture:** Add focused modules for ES8388 register setup, WAV header writing, and the audio application state machine. Keep CubeMX-generated files limited to includes and USER CODE sections, and add new modules to the Keil project.

**Tech Stack:** STM32F407 HAL, I2C1, I2S2 full-duplex DMA, SDIO/FatFs, Keil MDK-ARM.

---

### Task 1: WAV Header

**Files:**
- Create: `Audio_Record_Play/Core/Inc/wav.h`
- Create: `Audio_Record_Play/Core/Src/wav.c`
- Test: `Audio_Record_Play/tests/test_wav_header.py`

- [x] **Step 1: Write the failing test**

Run: `python Audio_Record_Play/tests/test_wav_header.py`

Expected: FAIL because `Core/Src/wav.c` does not exist.

- [x] **Step 2: Implement WAV header helpers**

Create functions that write a 44-byte PCM WAV header and later update RIFF/data sizes.

- [x] **Step 3: Run test**

Run: `python Audio_Record_Play/tests/test_wav_header.py`

Expected: PASS.

### Task 2: ES8388 Codec Setup

**Files:**
- Create: `Audio_Record_Play/Core/Inc/es8388.h`
- Create: `Audio_Record_Play/Core/Src/es8388.c`

- [x] **Step 1: Add I2C register write/read helpers**

Use `0x10 << 1` as the HAL address and return `HAL_StatusTypeDef`.

- [x] **Step 2: Add a basic record/play codec init sequence**

Configure ES8388 for I2S Philips, 16-bit, ADC from board microphone, DAC output enabled, moderate volume.

### Task 3: Audio App State Machine

**Files:**
- Create: `Audio_Record_Play/Core/Inc/audio_app.h`
- Create: `Audio_Record_Play/Core/Src/audio_app.c`
- Modify: `Audio_Record_Play/Core/Src/main.c`

- [x] **Step 1: Add PA0/PE8 debounce**

PA0 toggles recording, PE8 toggles playback, PC13 is read but does not trigger behavior.

- [x] **Step 2: Add recording path**

Mount SD, open `record.wav`, start I2S full-duplex DMA with silent TX and RX buffer, write completed half-buffers from the main loop, cap at 60 seconds, update WAV header on stop.

- [x] **Step 3: Add playback path**

Open `record.wav`, parse/skip the 44-byte header, fill DMA TX half-buffers from the file, transmit over I2S, and stop at EOF.

### Task 4: Keil Integration

**Files:**
- Modify: `Audio_Record_Play/MDK-ARM/Audio_Record_Play.uvprojx`

- [x] **Step 1: Add new C files to Application/User/Core**

Add `es8388.c`, `wav.c`, and `audio_app.c`.

### Task 5: Verification

**Files:**
- Inspect all modified files.

- [x] **Step 1: Run host WAV test**

Run: `python Audio_Record_Play/tests/test_wav_header.py`

Expected: PASS.

- [x] **Step 2: Static reference check**

Search for new module includes, callbacks, and Keil project entries.
