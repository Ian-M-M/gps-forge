# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from controls_sim import encode_command


class ControlsCommandTests(unittest.TestCase):
    def test_encoder_and_button(self):
        self.assertEqual(encode_command("right", (240, 240))[0], ["E 1"])
        self.assertEqual(encode_command("encoder -3", (240, 240))[0], ["E -3"])
        self.assertEqual(encode_command("button click", (240, 240))[0],
                         ["B 1", "B 0"])

    def test_touch_state_and_bounds(self):
        messages, last = encode_command("touch 160 200", (240, 240))
        self.assertEqual(messages, ["T 1 160 200"])
        self.assertEqual(encode_command("touch up", last)[0], ["T 0 160 200"])
        with self.assertRaisesRegex(ValueError, "0..479"):
            encode_command("touch 480 0", last)


if __name__ == "__main__":
    unittest.main()
