import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class OledSoftI2cPinTests(unittest.TestCase):
    def test_oled_uses_pd10_pe13_soft_i2c(self):
        source = (ROOT / "Core" / "Src" / "oled_ui.c").read_text(encoding="utf-8")

        self.assertIn("OLED_SCL_PORT        GPIOD", source)
        self.assertIn("OLED_SCL_PIN         GPIO_PIN_10", source)
        self.assertIn("OLED_SDA_PORT        GPIOE", source)
        self.assertIn("OLED_SDA_PIN         GPIO_PIN_13", source)
        self.assertIn("oled_soft_i2c_write", source)
        self.assertNotIn("HAL_I2C_Master_Transmit(&hi2c1", source)


if __name__ == "__main__":
    unittest.main()
