#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

"""Send virtual encoder, button, and touch events through the GPS simulator."""

import argparse
import socket


def encode_command(command: str, last_touch: tuple[int, int]) -> tuple[list[str], tuple[int, int]]:
    parts = command.lower().split()
    if not parts:
        return [], last_touch

    if parts == ["left"]:
        return ["E -1"], last_touch
    if parts == ["right"]:
        return ["E 1"], last_touch
    if len(parts) == 2 and parts[0] == "encoder":
        steps = int(parts[1])
        if steps == 0 or not -127 <= steps <= 127:
            raise ValueError("encoder steps must be from -127 to -1 or 1 to 127")
        return [f"E {steps}"], last_touch

    if parts == ["button", "down"]:
        return ["B 1"], last_touch
    if parts == ["button", "up"]:
        return ["B 0"], last_touch
    if parts == ["button", "click"]:
        return ["B 1", "B 0"], last_touch

    if parts == ["touch", "up"]:
        x, y = last_touch
        return [f"T 0 {x} {y}"], last_touch
    if len(parts) == 3 and parts[0] == "touch":
        x, y = int(parts[1]), int(parts[2])
        if not (0 <= x < 480 and 0 <= y < 480):
            raise ValueError("touch coordinates must be within 0..479")
        return [f"T 1 {x} {y}"], (x, y)
    if len(parts) == 4 and parts[:2] == ["touch", "tap"]:
        x, y = int(parts[2]), int(parts[3])
        if not (0 <= x < 480 and 0 <= y < 480):
            raise ValueError("touch coordinates must be within 0..479")
        return [f"T 1 {x} {y}", f"T 0 {x} {y}"], (x, y)

    raise ValueError("unknown command; type help for examples")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5557)
    parser.add_argument("--command", action="append", default=[],
                        help="Send one command; may be repeated")
    args = parser.parse_args()

    last_touch = (240, 240)
    with socket.create_connection((args.host, args.port)) as connection:
        print(f"Connected to virtual controls on {args.host}:{args.port}")

        def send(command: str) -> None:
            nonlocal last_touch
            messages, last_touch = encode_command(command, last_touch)
            for message in messages:
                connection.sendall((message + "\n").encode("ascii"))
                print(f"sent {message}")

        if args.command:
            for command in args.command:
                send(command)
            return

        print("Commands: left, right, encoder N, button down/up/click, "
              "touch X Y, touch tap X Y, touch up, quit")
        while True:
            try:
                command = input("controls> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if command.lower() in {"quit", "exit"}:
                break
            if command.lower() == "help":
                print("left | right | encoder N | button down/up/click | "
                      "touch X Y | touch tap X Y | touch up | quit")
                continue
            try:
                send(command)
            except ValueError as exc:
                print(exc)


if __name__ == "__main__":
    main()
