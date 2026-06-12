import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class AudioRecordMixTests(unittest.TestCase):
    def test_recording_uses_one_mic_channel_by_default(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("AUDIO_RECORD_INPUT_CHANNEL", source)
        self.assertIn("#define AUDIO_RECORD_INPUT_CHANNEL AUDIO_RECORD_CHANNEL_LEFT", source)
        self.assertIn("select_i2s_record_sample", source)
        self.assertRegex(source, re.compile(r"int16_t\s+left"))
        self.assertRegex(source, re.compile(r"int16_t\s+right"))
        self.assertNotRegex(source, re.compile(r"left\s*\+\s*right"))
        self.assertNotIn("mix_i2s_stereo_to_mono", source)

    def test_recording_applies_saturating_digital_gain(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("AUDIO_RECORD_DIGITAL_GAIN", source)
        self.assertIn("#define AUDIO_RECORD_DIGITAL_GAIN  3U", source)
        self.assertIn("apply_record_gain", source)
        self.assertRegex(
            source,
            re.compile(r"\(int32_t\)\(int16_t\)sample\s*\*\s*AUDIO_RECORD_DIGITAL_GAIN"),
        )
        self.assertIn("32767", source)
        self.assertIn("-32768", source)

    def test_recording_flushes_i2s_before_starting_dma(self):
        source = (ROOT / "Core" / "Src" / "audio_app.c").read_text(encoding="utf-8")

        self.assertIn("prepare_i2s_for_start", source)
        self.assertRegex(
            source,
            re.compile(r"start_recording.*prepare_i2s_for_start\(\).*HAL_I2SEx_TransmitReceive_DMA", re.S),
        )
        self.assertIn("__HAL_I2SEXT_FLUSH_RX_DR(&hi2s2)", source)


if __name__ == "__main__":
    unittest.main()
