import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class OledStateTextTests(unittest.TestCase):
    def test_oled_ui_contains_required_state_labels(self):
        source = (ROOT / "Core" / "Src" / "oled_ui.c").read_text(encoding="utf-8")

        for label in ("REC:", "PLAY:", "ON", "STOP", "ERROR"):
            self.assertIn(label, source)

        self.assertRegex(source, re.compile(r"AUDIO_APP_RECORDING.*REC:.*ON", re.S))
        self.assertRegex(source, re.compile(r"AUDIO_APP_PLAYING.*PLAY:.*ON", re.S))
        self.assertRegex(source, re.compile(r"AUDIO_APP_IDLE.*REC:.*STOP", re.S))
        self.assertRegex(source, re.compile(r"AUDIO_APP_IDLE.*PLAY:.*STOP", re.S))

    def test_error_screen_shows_file_and_hal_codes(self):
        source = (ROOT / "Core" / "Src" / "oled_ui.c").read_text(encoding="utf-8")

        self.assertIn("AudioApp_GetLastFileResult", source)
        self.assertIn("AudioApp_GetLastHalResult", source)
        self.assertIn("SD:%d", source)
        self.assertIn("H:%d", source)
        self.assertIn("snprintf", source)

    def test_oled_font_supports_numeric_error_codes(self):
        source = (ROOT / "Core" / "Src" / "oled_ui.c").read_text(encoding="utf-8")

        for digit in range(10):
            self.assertIn(f"glyph_{digit}", source)

    def test_oled_shows_current_effect_mode(self):
        source = (ROOT / "Core" / "Src" / "oled_ui.c").read_text(encoding="utf-8")

        self.assertIn("AudioApp_GetFxMode", source)
        self.assertIn("FX:%d", source)
        self.assertIn("glyph_f", source)
        self.assertIn("glyph_x", source)

    def test_oled_shows_second_recording_mode(self):
        source = (ROOT / "Core" / "Src" / "oled_ui.c").read_text(encoding="utf-8")

        self.assertIn("AUDIO_FX_RECORD2", source)
        self.assertIn("REC2", source)


if __name__ == "__main__":
    unittest.main()
