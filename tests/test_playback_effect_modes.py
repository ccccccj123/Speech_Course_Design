import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PlaybackEffectModeTests(unittest.TestCase):
    def test_pc13_cycles_playback_effect_modes(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")
        header = (ROOT / "Core" / "Inc" / "audio_app.h").read_text(encoding="utf-8")

        self.assertIn("AudioFxMode", header)
        self.assertIn("AudioApp_GetFxMode", header)
        self.assertIn("AUDIO_FX_NORMAL", header)
        self.assertIn("AUDIO_FX_FILTER", header)
        self.assertIn("s_fx_mode", source)
        self.assertIn("cycle_fx_mode", source)
        self.assertRegex(source, re.compile(r"button_pressed\(&s_reserved_key\).*cycle_fx_mode", re.S))

    def test_playback_samples_are_processed_before_i2s_output(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("apply_playback_effects", source)
        self.assertRegex(
            source,
            re.compile(r"fill_playback_segment.*apply_playback_effects\(s_mono_buffer", re.S),
        )
        self.assertRegex(
            source,
            re.compile(r"dst\[\(i \* AUDIO_I2S_CHANNELS\) \+ 0U\] = s_effect_buffer\[i\]", re.S),
        )

    def test_all_course_extension_effects_have_paths(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        for text in (
            "AUDIO_FX_PITCH",
            "AUDIO_FX_PITCH_STEP_PERCENT",
            "AUDIO_FX_PITCH_MOD_PERIOD",
            "AUDIO_FX_ECHO",
            "AUDIO_FX_MIX",
            "AUDIO_FX_SLOW",
            "AUDIO_FX_FILTER",
            "apply_pitch_effect",
            "apply_echo_effect",
            "apply_mix_effect",
            "apply_slow_effect",
            "apply_filter_effect",
            "saturate_i16",
        ):
            self.assertIn(text, source)

    def test_pitch_effect_is_more_obvious_than_light_resampling(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("#define AUDIO_FX_PITCH_STEP_PERCENT 150U", source)
        self.assertIn("s_fx_pitch_phase", source)
        self.assertRegex(source, re.compile(r"sample\s*=\s*-\s*sample"))
        self.assertNotIn("105U", source)

    def test_effect_state_resets_when_playback_starts_or_mode_changes(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("reset_effect_state", source)
        self.assertRegex(source, re.compile(r"start_playback.*reset_effect_state\(\)", re.S))
        self.assertRegex(source, re.compile(r"cycle_fx_mode.*reset_effect_state\(\)", re.S))

    def test_extra_pc13_mode_records_second_wav_file(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")
        header = (ROOT / "Core" / "Inc" / "audio_app.h").read_text(encoding="utf-8")

        self.assertIn("AUDIO_FX_RECORD2", header)
        self.assertIn('#define MIX_FILE_NAME          "mix.wav"', source)
        self.assertIn("get_record_file_name", source)
        self.assertRegex(
            source,
            re.compile(r"start_recording.*f_open\(&s_audio_file,\s*get_record_file_name\(\)", re.S),
        )

    def test_mix_effect_reads_and_blends_second_wav_file(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("s_mix_file", source)
        self.assertIn("s_mix_buffer", source)
        self.assertRegex(
            source,
            re.compile(r"start_playback.*AUDIO_FX_MIX.*f_open\(&s_mix_file,\s*MIX_FILE_NAME,\s*FA_READ\)", re.S),
        )
        self.assertIn("ensure_mix_file_open", source)
        self.assertRegex(source, re.compile(r"apply_mix_effect.*ensure_mix_file_open\(\)", re.S))
        self.assertRegex(source, re.compile(r"apply_mix_effect.*f_read\(&s_mix_file", re.S))
        self.assertRegex(
            source,
            re.compile(r"\(int32_t\)sample\s*/\s*2\)\s*\+\s*\(\(int32_t\)mix_sample\s*/\s*2"),
        )


if __name__ == "__main__":
    unittest.main()
