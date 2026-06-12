import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class StopRecordingResilienceTests(unittest.TestCase):
    def test_intentional_i2s_stop_suppresses_error_callback(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("s_i2s_stop_in_progress", source)
        self.assertRegex(
            source,
            re.compile(
                r"HAL_I2S_ErrorCallback.*s_i2s_stop_in_progress\s*!=\s*0U.*return;",
                re.S,
            ),
        )

    def test_stop_recording_logs_file_close_stages(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        for text in (
            "stop_hal_result",
            "wav_result",
            "sync_result",
            "close_result",
        ):
            self.assertIn(text, source)


if __name__ == "__main__":
    unittest.main()
