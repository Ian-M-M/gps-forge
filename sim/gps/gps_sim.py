#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0


import argparse
import math
import random
import socket
import socketserver
import threading
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


EUROPE_START_POINTS = (
    (51.5074, -0.1278),   # London
    (48.8566, 2.3522),    # Paris
    (52.5200, 13.4050),   # Berlin
    (41.9028, 12.4964),   # Rome
    (40.4168, -3.7038),   # Madrid
    (52.2297, 21.0122),   # Warsaw
    (59.3293, 18.0686),   # Stockholm
    (38.7223, -9.1393),   # Lisbon
    (50.0755, 14.4378),   # Prague
    (47.4979, 19.0402),   # Budapest
)


def random_europe_position(random_source: object = random) -> tuple[float, float]:
    """Choose a non-sensitive simulated start near a major European city."""
    latitude, longitude = random_source.choice(EUROPE_START_POINTS)
    return (
        latitude + random_source.uniform(-0.5, 0.5),
        longitude + random_source.uniform(-0.5, 0.5),
    )


@dataclass(frozen=True)
class GpxPoint:
    latitude: float
    longitude: float
    altitude_m: float
    start_segment: bool = False


def load_gpx(path: Path, default_altitude: float) -> list[GpxPoint]:
    root = ET.parse(path).getroot()
    points = []
    start_segment = True
    for element in root.iter():
        tag = element.tag.rsplit("}", 1)[-1]
        if tag in {"trkseg", "rte"}:
            start_segment = True
        if tag not in {"trkpt", "rtept"}:
            continue

        try:
            latitude = float(element.attrib["lat"])
            longitude = float(element.attrib["lon"])
            altitude = default_altitude
            for child in element:
                if child.tag.rsplit("}", 1)[-1] == "ele" and child.text:
                    altitude = float(child.text)
                    break
        except (KeyError, ValueError) as exc:
            raise ValueError(f"invalid GPX point in {path}") from exc

        if not (math.isfinite(latitude) and -90 <= latitude <= 90 and
                math.isfinite(longitude) and -180 <= longitude <= 180 and
                math.isfinite(altitude)):
            raise ValueError(f"out-of-range GPX point in {path}")

        points.append(GpxPoint(latitude, longitude, altitude, start_segment))
        start_segment = False

    if not points:
        raise ValueError(f"no track or route points found in {path}")
    return points


def route_motion(previous: GpxPoint | None, current: GpxPoint,
                 interval_seconds: float) -> tuple[float, float]:
    if previous is None or current.start_segment:
        return 0.0, 0.0

    lat1 = math.radians(previous.latitude)
    lat2 = math.radians(current.latitude)
    delta_lat = lat2 - lat1
    delta_lon = math.radians(current.longitude - previous.longitude)
    a = math.sin(delta_lat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(delta_lon / 2) ** 2
    distance_m = 2 * 6_371_000 * math.asin(min(1.0, math.sqrt(a)))
    speed_kmh = distance_m * 3.6 / interval_seconds

    y = math.sin(delta_lon) * math.cos(lat2)
    x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(delta_lon)
    course = math.degrees(math.atan2(y, x)) % 360 if distance_m > 0 else 0.0
    return speed_kmh, course


def advance_position(latitude: float, longitude: float, speed_kmh: float,
                     course: float, interval_seconds: float) -> tuple[float, float]:
    """Move a point along a great-circle bearing for one simulation interval."""
    distance_m = speed_kmh * 1000.0 * interval_seconds / 3600.0
    angular_distance = distance_m / 6_371_000.0
    bearing = math.radians(course % 360.0)
    lat1 = math.radians(latitude)
    lon1 = math.radians(longitude)

    lat2 = math.asin(
        math.sin(lat1) * math.cos(angular_distance)
        + math.cos(lat1) * math.sin(angular_distance) * math.cos(bearing)
    )
    lon2 = lon1 + math.atan2(
        math.sin(bearing) * math.sin(angular_distance) * math.cos(lat1),
        math.cos(angular_distance) - math.sin(lat1) * math.sin(lat2),
    )

    # Keep longitude in the NMEA-compatible -180..180 range.
    next_longitude = (math.degrees(lon2) + 540.0) % 360.0 - 180.0
    return math.degrees(lat2), next_longitude


def choose_course(configured_course: float, random_course: bool,
                  random_source: object = random) -> float:
    if random_course:
        return random_source.uniform(0.0, 360.0)
    return configured_course % 360.0


def nmea_checksum(payload: str) -> str:
    checksum = 0

    for char in payload:
        checksum ^= ord(char)

    return f"{checksum:02X}"


def nmea_sentence(payload: str) -> str:
    return f"${payload}*{nmea_checksum(payload)}\r\n"


def decimal_to_nmea(value: float, latitude: bool) -> tuple[str, str]:
    direction = (
        ("N" if value >= 0 else "S")
        if latitude
        else ("E" if value >= 0 else "W")
    )

    value = abs(value)

    degrees = int(value)
    minutes = (value - degrees) * 60.0

    if latitude:
        coordinate = f"{degrees:02d}{minutes:07.4f}"
    else:
        coordinate = f"{degrees:03d}{minutes:07.4f}"

    return coordinate, direction


def generate_rmc(
    now: datetime,
    latitude: float,
    longitude: float,
    speed_kmh: float,
    course: float,
) -> str:
    lat, lat_dir = decimal_to_nmea(latitude, latitude=True)
    lon, lon_dir = decimal_to_nmea(longitude, latitude=False)

    speed_knots = speed_kmh / 1.852

    payload = (
        f"GPRMC,"
        f"{now:%H%M%S}.00,"
        f"A,"
        f"{lat},{lat_dir},"
        f"{lon},{lon_dir},"
        f"{speed_knots:.1f},"
        f"{course:.1f},"
        f"{now:%d%m%y},"
        f",,"
        f"A"
    )

    return nmea_sentence(payload)


def generate_gga(
    now: datetime,
    latitude: float,
    longitude: float,
    altitude: float,
) -> str:
    lat, lat_dir = decimal_to_nmea(latitude, latitude=True)
    lon, lon_dir = decimal_to_nmea(longitude, latitude=False)

    payload = (
        f"GPGGA,"
        f"{now:%H%M%S}.00,"
        f"{lat},{lat_dir},"
        f"{lon},{lon_dir},"
        f"1,"          # GPS fix
        f"08,"         # satellites
        f"0.9,"        # HDOP
        f"{altitude:.1f},M,"
        f"0.0,M,"
        f","
    )

    return nmea_sentence(payload)


class ControlHandler(socketserver.StreamRequestHandler):
    def handle(self) -> None:
        for raw_line in self.rfile:
            command = raw_line.strip()
            if not command or len(command) > 60:
                continue
            try:
                command.decode("ascii")
                with self.server.send_lock:
                    self.server.qemu_socket.sendall(b"!" + command + b"\r\n")
            except (UnicodeDecodeError, OSError):
                return


class ControlServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address: tuple[str, int], qemu_socket: socket.socket,
                 send_lock: threading.Lock) -> None:
        super().__init__(address, ControlHandler)
        self.qemu_socket = qemu_socket
        self.send_lock = send_lock


def main() -> None:
    parser = argparse.ArgumentParser()

    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5556)
    parser.add_argument("--controls-port", type=int, default=5557,
                        help="Local TCP port for control commands; 0 disables it")

    parser.add_argument("--lat", type=float,
                        help="Starting latitude; defaults to a random European location")
    parser.add_argument("--lon", type=float,
                        help="Starting longitude; defaults to a random European location")

    parser.add_argument("--alt", type=float, default=20.0)
    parser.add_argument("--speed", type=float, default=0.0,
                        help="Speed in km/h; without --gpx, moves the position")
    parser.add_argument("--course", type=float, default=0.0,
                        help="Movement bearing in degrees clockwise from north")
    parser.add_argument("--random-course", action="store_true",
                        help="Choose a new random bearing for each interval")
    parser.add_argument("--gpx", type=Path, help="Replay GPX track or route points")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="Seconds between points/sentences (default: 1)")
    parser.add_argument("--loop", action="store_true", help="Repeat the GPX route")

    args = parser.parse_args()
    if not math.isfinite(args.interval) or args.interval <= 0:
        parser.error("--interval must be a positive number")
    if not math.isfinite(args.speed) or args.speed < 0:
        parser.error("--speed must be a non-negative number")
    if not math.isfinite(args.course):
        parser.error("--course must be a finite number")
    if (args.lat is None) != (args.lon is None):
        parser.error("--lat and --lon must be provided together")
    if args.lat is None:
        args.lat, args.lon = random_europe_position()
    if not (math.isfinite(args.lat) and -90 <= args.lat <= 90):
        parser.error("--lat must be between -90 and 90")
    if not (math.isfinite(args.lon) and -180 <= args.lon <= 180):
        parser.error("--lon must be between -180 and 180")
    if not math.isfinite(args.alt):
        parser.error("--alt must be a finite number")
    try:
        route = load_gpx(args.gpx, args.alt) if args.gpx else None
    except (OSError, ET.ParseError, ValueError) as exc:
        parser.error(str(exc))

    print(f"Starting simulated position: {args.lat:.5f}, {args.lon:.5f}")
    print(f"Connecting to QEMU UART1 at {args.host}:{args.port}")

    with socket.create_connection((args.host, args.port)) as sock:
        print("Connected")

        send_lock = threading.Lock()
        control_server = None
        if args.controls_port:
            control_server = ControlServer(("127.0.0.1", args.controls_port),
                                           sock, send_lock)
            threading.Thread(target=control_server.serve_forever,
                             daemon=True).start()
            print(f"Virtual controls listening on 127.0.0.1:{args.controls_port}")

        try:
            point_index = 0
            moving_latitude = args.lat
            moving_longitude = args.lon
            while True:
                now = datetime.now(timezone.utc)

                if route:
                    point = route[point_index]
                    previous = route[point_index - 1] if point_index > 0 else None
                    speed, course = route_motion(previous, point, args.interval)
                    latitude, longitude, altitude = (
                        point.latitude, point.longitude, point.altitude_m
                    )
                else:
                    latitude, longitude, altitude = (
                        moving_latitude, moving_longitude, args.alt
                    )
                    speed = args.speed
                    course = choose_course(args.course, args.random_course)

                sentences = (
                    generate_rmc(
                        now,
                        latitude,
                        longitude,
                        speed,
                        course,
                    ),
                    generate_gga(
                        now,
                        latitude,
                        longitude,
                        altitude,
                    ),
                )

                for sentence in sentences:
                    print(sentence.rstrip())
                    with send_lock:
                        sock.sendall(sentence.encode("ascii"))

                if route:
                    point_index += 1
                    if point_index == len(route):
                        if not args.loop:
                            break
                        point_index = 0
                else:
                    moving_latitude, moving_longitude = advance_position(
                        latitude,
                        longitude,
                        args.speed,
                        course,
                        args.interval,
                    )

                time.sleep(args.interval)
        finally:
            if control_server is not None:
                control_server.shutdown()
                control_server.server_close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped")
