import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _extract_hex_initializer(source: str) -> bytes:
    match = re.search(r"(?:const\s+)?uint8_t\s+\w+\[44\]\s*=\s*\{(?P<body>.*?)\};", source, re.S)
    assert match, "wav.c should build a 44-byte WAV header initializer"
    values = re.findall(r"0x[0-9A-Fa-f]{2}|\d+U?", match.group("body"))
    assert len(values) == 44
    return bytes(int(value.rstrip("U"), 0) for value in values)


class WavHeaderTests(unittest.TestCase):
    def test_wav_header_contains_pcm_riff_layout(self):
        wav_c = (ROOT / "Core" / "Src" / "wav.c").read_text(encoding="utf-8")
        header = _extract_hex_initializer(wav_c)

        self.assertEqual(header[0:4], b"RIFF")
        self.assertEqual(header[8:12], b"WAVE")
        self.assertEqual(header[12:16], b"fmt ")
        self.assertEqual(header[36:40], b"data")
        self.assertEqual(int.from_bytes(header[16:20], "little"), 16)
        self.assertEqual(int.from_bytes(header[20:22], "little"), 1)
        self.assertEqual(int.from_bytes(header[22:24], "little"), 1)
        self.assertEqual(int.from_bytes(header[24:28], "little"), 16000)
        self.assertEqual(int.from_bytes(header[28:32], "little"), 32000)
        self.assertEqual(int.from_bytes(header[32:34], "little"), 2)
        self.assertEqual(int.from_bytes(header[34:36], "little"), 16)


if __name__ == "__main__":
    unittest.main()
