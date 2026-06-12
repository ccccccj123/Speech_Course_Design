import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class SdBusWidthTests(unittest.TestCase):
    def test_sd_init_does_not_require_four_bit_bus(self):
        source = (ROOT / "FATFS" / "Target" / "bsp_driver_sd.c").read_text(encoding="utf-8")

        self.assertNotIn("SDIO_BUS_WIDE_4B", source)


if __name__ == "__main__":
    unittest.main()
