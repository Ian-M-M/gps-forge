# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from gps_sim import (advance_position, choose_course, load_gpx,
                     random_europe_position, route_motion)


class GpxReplayTests(unittest.TestCase):
    def test_namespaced_track_and_motion(self):
        xml = """<gpx xmlns="http://www.topografix.com/GPX/1/1">
          <trk><trkseg>
            <trkpt lat="41.3662" lon="2.1169"><ele>20</ele></trkpt>
            <trkpt lat="41.3663" lon="2.1170"><ele>21</ele></trkpt>
          </trkseg></trk>
        </gpx>"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "route.gpx"
            path.write_text(xml)
            points = load_gpx(path, default_altitude=5)

        self.assertEqual(len(points), 2)
        self.assertEqual(points[0].altitude_m, 20)
        self.assertTrue(points[0].start_segment)
        self.assertAlmostEqual(points[1].longitude, 2.1170)
        self.assertEqual(route_motion(None, points[0], 1), (0, 0))
        speed, course = route_motion(points[0], points[1], 1)
        self.assertGreater(speed, 40)
        self.assertLess(speed, 60)
        self.assertGreater(course, 20)
        self.assertLess(course, 50)

    def test_segment_boundary_resets_motion(self):
        xml = """<gpx><trk>
          <trkseg><trkpt lat="0" lon="0"/></trkseg>
          <trkseg><trkpt lat="1" lon="1"/></trkseg>
        </trk></gpx>"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "route.gpx"
            path.write_text(xml)
            points = load_gpx(path, default_altitude=12)

        self.assertEqual(points[1].altitude_m, 12)
        self.assertEqual(route_motion(points[0], points[1], 1), (0, 0))

    def test_rejects_invalid_points(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.gpx"
            path.write_text('<gpx><rtept lat="91" lon="0"/></gpx>')
            with self.assertRaisesRegex(ValueError, "out-of-range"):
                load_gpx(path, 0)
            path.write_text("<gpx/>")
            with self.assertRaisesRegex(ValueError, "no track or route points"):
                load_gpx(path, 0)


class MovingPositionTests(unittest.TestCase):
    def test_random_start_uses_a_major_european_city_with_jitter(self):
        class FakeRandom:
            def choice(self, values):
                return values[0]

            def uniform(self, _minimum, _maximum):
                return 0.25

        latitude, longitude = random_europe_position(FakeRandom())
        self.assertAlmostEqual(latitude, 51.7574)
        self.assertAlmostEqual(longitude, 0.1222)

    def test_random_course_is_selected_for_each_interval(self):
        class FakeRandom:
            def __init__(self):
                self.values = iter((12.5, 271.0))

            def uniform(self, _minimum, _maximum):
                return next(self.values)

        source = FakeRandom()
        self.assertEqual(choose_course(90, True, source), 12.5)
        self.assertEqual(choose_course(90, True, source), 271.0)
        self.assertEqual(choose_course(450, False, source), 90.0)

    def test_moves_north_at_selected_speed(self):
        latitude, longitude = advance_position(0, 0, 36, 0, 1)

        self.assertAlmostEqual(latitude, 0.00008993, places=7)
        self.assertAlmostEqual(longitude, 0, places=7)

    def test_moves_east_and_wraps_longitude(self):
        latitude, longitude = advance_position(0, 179.99995, 36, 90, 1)

        self.assertAlmostEqual(latitude, 0, places=7)
        self.assertLess(longitude, -179.9)

    def test_zero_speed_keeps_position(self):
        latitude, longitude = advance_position(41.3, 2.1, 0, 270, 10)
        self.assertAlmostEqual(latitude, 41.3)
        self.assertAlmostEqual(longitude, 2.1)


if __name__ == "__main__":
    unittest.main()
