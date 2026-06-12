import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ES8388PlaybackOutputTests(unittest.TestCase):
    def test_dac_output_paths_are_enabled_for_headphone_and_speaker(self):
        source = (ROOT / "Core" / "Src" / "es8388.c").read_text(encoding="utf-8")

        for reg in ("0x2E", "0x2F", "0x30", "0x31"):
            self.assertIn(f"{{{reg}, 0x21}}", source)
            self.assertIn(f"verify_reg({reg}, 0x21)", source)

    def test_dac_is_unmuted_and_routed_to_output_mixers(self):
        source = (ROOT / "Core" / "Src" / "es8388.c").read_text(encoding="utf-8")

        self.assertIn("{0x19, 0x00}", source)
        self.assertIn("{0x27, 0xB8}", source)
        self.assertIn("{0x2A, 0xB8}", source)
        self.assertIn("verify_reg(0x19, 0x00)", source)
        self.assertIn("verify_reg(0x27, 0xB8)", source)
        self.assertIn("verify_reg(0x2A, 0xB8)", source)


if __name__ == "__main__":
    unittest.main()
