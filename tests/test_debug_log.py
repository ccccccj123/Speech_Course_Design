import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DebugLogTests(unittest.TestCase):
    def test_debug_log_contains_audio_and_codec_fields(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        for text in (
            "debug.txt",
            "recorded_bytes",
            "left_min",
            "left_max",
            "right_min",
            "right_max",
            "mono_min",
            "mono_max",
            "left_nonzero",
            "right_nonzero",
            "ES8388 registers",
            "verify_error",
            "fw=",
            "raw_words_count",
            "0x2B",
            "i2s_state",
            "i2s_error",
            "record_start_error",
        ):
            self.assertIn(text, source)

    def test_raw_debug_words_skip_leading_silence(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("((frame[0] != 0U) || (frame[1] != 0U))", source)


if __name__ == "__main__":
    unittest.main()
