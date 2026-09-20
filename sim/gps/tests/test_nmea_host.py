# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

import subprocess
import tempfile
import unittest
from pathlib import Path


class NmeaParserHostTests(unittest.TestCase):
    def test_stream_and_corrupt_input(self):
        root = Path(__file__).resolve().parents[3]
        component = root / "firmware/components/gps"
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "nmea_host_test"
            subprocess.run([
                "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(component / "include"),
                "-I", str(component),
                str(component / "nmea.c"),
                str(component / "tests/nmea_host_test.c"),
                "-lm", "-o", str(executable),
            ], check=True, capture_output=True, text=True)
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)
