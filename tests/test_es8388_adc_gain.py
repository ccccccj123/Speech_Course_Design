import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ES8388AdcGainTests(unittest.TestCase):
    def test_adc_pga_gain_is_moderate_after_adc_power_fix(self):
        source = (ROOT / "Core" / "Src" / "es8388.c").read_text(encoding="utf-8")

        self.assertIn("{0x09, 0x66}", source)
        self.assertIn("verify_reg(0x09, 0x66)", source)
        self.assertNotIn("{0x09, 0xBB}", source)

    def test_adc_input_select_matches_onboard_lin1_rin1_microphone(self):
        source = (ROOT / "Core" / "Src" / "es8388.c").read_text(encoding="utf-8")

        self.assertIn("{0x0A, 0x00}", source)
        self.assertIn("verify_reg(0x0A, 0x00)", source)
        self.assertNotIn("{0x0A, 0xF0}", source)

    def test_adc_dac_share_lrck_for_i2s_clock_alignment(self):
        source = (ROOT / "Core" / "Src" / "es8388.c").read_text(encoding="utf-8")

        self.assertIn("{0x2B, 0x80}", source)
        self.assertIn("verify_reg(0x2B, 0x80)", source)

    def test_adc_power_uses_external_mic_bias_from_board(self):
        source = (ROOT / "Core" / "Src" / "es8388.c").read_text(encoding="utf-8")

        self.assertIn("{0x03, 0x09}", source)
        self.assertIn("verify_reg(0x03, 0x09)", source)
        self.assertNotIn("{0x03, 0x00}", source)


if __name__ == "__main__":
    unittest.main()
