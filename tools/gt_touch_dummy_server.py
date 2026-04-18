#!/usr/bin/env python3
"""Dummy USB CDC BAP host for exercising the GT Touch UI."""

from __future__ import annotations

import argparse
import math
import sys
import time
from dataclasses import dataclass, field
from typing import Optional

try:
    import serial as serial_module
    from serial import SerialException
except ImportError:  # pragma: no cover - handled at runtime
    serial_module = None

    class SerialException(Exception):
        """Fallback exception type when pyserial is unavailable."""


RESPONSE_MAP = {
    "hashrate": "hashrate",
    "temperature": "chipTemp",
    "power": "power",
    "fan_speed": "fan_speed",
    "fan_speed_percent": "fan_speed_percent",
    "shares": "shares",
    "best_difficulty": "best_difficulty",
    "block_height": "block_height",
}

SYSTEM_INFO_ORDER = (
    "deviceModel",
    "asicModel",
    "pool",
    "poolPort",
    "poolUser",
    "mode",
    "voltage",
)


@dataclass
class BAPMessage:
    command: str
    parameter: str
    value: Optional[str]
    checksum_ok: bool = True


@dataclass
class DummyMinerState:
    frequency_mhz: float = 600.0
    asic_voltage_mv: float = 1200.0
    auto_fan: bool = True
    manual_fan_percent: int = 75
    device_model: str = "Bitaxe Gamma"
    asic_model: str = "BM1370"
    pool_url: str = "stratum+tcp://public-pool.io"
    pool_port: str = "3333"
    pool_user: str = "gt-touch-demo"
    shares: int = 42
    best_difficulty: int = 1024
    block_height: int = 891234
    mode: str = "normal"
    _last_share_update: float = field(default_factory=time.monotonic)
    _last_block_update: float = field(default_factory=time.monotonic)

    def apply_set(self, parameter: str, value: str) -> None:
        if parameter == "frequency":
            self.frequency_mhz = float(value)
            if self.frequency_mhz >= 650:
                self.mode = "high"
            elif self.frequency_mhz <= 580:
                self.mode = "low"
            else:
                self.mode = "normal"
        elif parameter == "asic_voltage":
            self.asic_voltage_mv = float(value)
        elif parameter == "fan_speed":
            self.manual_fan_percent = max(0, min(100, int(float(value))))
            self.auto_fan = False
        elif parameter == "auto_fan":
            self.auto_fan = value not in {"0", "false", "False"}

    def advance_counters(self, now: float) -> None:
        while now - self._last_share_update >= 5.0:
            self._last_share_update += 5.0
            self.shares += 1
            self.best_difficulty += 64

        while now - self._last_block_update >= 30.0:
            self._last_block_update += 30.0
            self.block_height += 1

    def metric_value(self, response_parameter: str, elapsed: float) -> str:
        perf_factor = (self.frequency_mhz - 575.0) / 80.0
        voltage_factor = (self.asic_voltage_mv - 1160.0) / 40.0

        if response_parameter == "hashrate":
            hashrate = (
                1050.0
                + perf_factor * 160.0
                + 45.0 * math.sin(elapsed / 7.5)
                + 12.0 * math.sin(elapsed / 2.7)
            )
            return f"{max(hashrate, 0.0):.2f}"

        if response_parameter == "chipTemp":
            temperature = 48.0 + perf_factor * 7.0 + voltage_factor * 1.8
            temperature += 2.8 * math.sin(elapsed / 13.0)
            return f"{temperature:.2f}"

        if response_parameter == "power":
            power = 14.0 + perf_factor * 5.0 + voltage_factor * 1.5
            power += 0.9 * math.sin(elapsed / 8.0)
            return f"{power:.1f}"

        if response_parameter == "fan_speed":
            if self.auto_fan:
                chip_temp = float(self.metric_value("chipTemp", elapsed))
                fan_rpm = 3400 + max(chip_temp - 45.0, 0.0) * 210.0
            else:
                fan_rpm = 1500 + self.manual_fan_percent * 48
            return str(int(round(fan_rpm)))

        if response_parameter == "fan_speed_percent":
            return str(self.manual_fan_percent)

        if response_parameter == "shares":
            return str(self.shares)

        if response_parameter == "best_difficulty":
            return str(self.best_difficulty)

        if response_parameter == "block_height":
            return str(self.block_height)

        if response_parameter == "deviceModel":
            return self.device_model

        if response_parameter == "asicModel":
            return self.asic_model

        if response_parameter == "pool":
            return self.pool_url

        if response_parameter == "poolPort":
            return self.pool_port

        if response_parameter == "poolUser":
            return self.pool_user

        if response_parameter == "mode":
            return self.mode

        if response_parameter == "voltage":
            return f"{self.asic_voltage_mv:.2f}"

        raise KeyError(f"Unsupported response parameter: {response_parameter}")


def xor_checksum(sentence_body: str) -> int:
    checksum = 0
    for ch in sentence_body:
        checksum ^= ord(ch)
    return checksum


def format_checked_message(command: str, parameter: str, value: Optional[str] = None) -> str:
    if value is None:
        body = f"BAP,{command},{parameter}"
    else:
        body = f"BAP,{command},{parameter},{value}"
    return f"${body}*{xor_checksum(body):02X}\r\n"


def parse_message(line: str) -> Optional[BAPMessage]:
    stripped = line.strip()
    if not stripped.startswith("$BAP,"):
        return None

    if "*" in stripped:
        payload, checksum_text = stripped[1:].split("*", 1)
        parts = payload.split(",", 3)
        if len(parts) < 3 or parts[0] != "BAP":
            return None
        expected = f"{xor_checksum(payload):02X}"
        return BAPMessage(
            command=parts[1],
            parameter=parts[2],
            value=parts[3] if len(parts) > 3 else None,
            checksum_ok=checksum_text[:2].upper() == expected,
        )

    parts = stripped[1:].split(",", 3)
    if len(parts) < 3 or parts[0] != "BAP":
        return None

    return BAPMessage(
        command=parts[1],
        parameter=parts[2],
        value=parts[3] if len(parts) > 3 else None,
        checksum_ok=True,
    )


class DummyBAPServer:
    def __init__(self, port: str, baudrate: int, update_interval: float) -> None:
        self.port = port
        self.baudrate = baudrate
        self.update_interval = update_interval
        self.state = DummyMinerState()
        self.subscriptions: set[str] = set()
        self.start_time = time.monotonic()
        self.last_stream_time = 0.0
        self.serial_port: Optional[object] = None

    def open(self) -> None:
        if serial_module is None:
            raise SystemExit(
                "pyserial is required. Install it with: python3 -m pip install pyserial"
            )

        self.serial_port = serial_module.Serial(
            port=self.port,
            baudrate=self.baudrate,
            timeout=0.2,
            write_timeout=1.0,
        )
        self.serial_port.dtr = True
        self.serial_port.rts = True
        self.log(f"Opened {self.port} at {self.baudrate} baud")
        self.log("CDC DTR asserted, waiting for GT Touch subscriptions...")

    def run(self) -> None:
        assert self.serial_port is not None

        while True:
            self.handle_incoming()
            self.maybe_stream_metrics()

    def handle_incoming(self) -> None:
        assert self.serial_port is not None

        raw = self.serial_port.readline()
        if not raw:
            return

        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            return

        self.log(f"RX {line}")
        message = parse_message(line)
        if message is None:
            self.log("Ignoring non-BAP line")
            return

        if message.command in {"REQ", "SET", "CMD", "RES"} and not message.checksum_ok:
            self.log("Ignoring line with invalid checksum")
            return

        if message.command == "SUB":
            self.handle_subscription(message.parameter)
            return

        if message.command == "REQ":
            self.handle_request(message.parameter)
            return

        if message.command == "SET" and message.value is not None:
            self.state.apply_set(message.parameter, message.value)
            self.log(
                "Applied SET "
                f"{message.parameter}={message.value} "
                f"(freq={self.state.frequency_mhz:.0f}MHz, "
                f"voltage={self.state.asic_voltage_mv:.2f}, "
                f"auto_fan={self.state.auto_fan}, "
                f"manual_fan={self.state.manual_fan_percent}%)"
            )
            if message.parameter in {"frequency", "asic_voltage", "fan_speed", "auto_fan"}:
                self.send_response("mode")
                self.send_response("voltage")
                self.send_response("fan_speed")
                self.send_response("fan_speed_percent")
            return

    def handle_subscription(self, parameter: str) -> None:
        if parameter not in RESPONSE_MAP:
            self.log(f"GT Touch subscribed to unknown parameter '{parameter}'")
            return

        if parameter not in self.subscriptions:
            self.subscriptions.add(parameter)
            self.log(f"Registered subscription for {parameter}")

        self.send_response(RESPONSE_MAP[parameter])

    def handle_request(self, parameter: str) -> None:
        if parameter == "systemInfo":
            for response_parameter in SYSTEM_INFO_ORDER:
                self.send_response(response_parameter)
            return

        self.log(f"Unhandled REQ parameter '{parameter}'")

    def maybe_stream_metrics(self) -> None:
        if not self.subscriptions:
            return

        now = time.monotonic()
        if now - self.last_stream_time < self.update_interval:
            return

        self.last_stream_time = now
        for subscription in sorted(self.subscriptions):
            self.send_response(RESPONSE_MAP[subscription], now=now)

    def send_response(self, parameter: str, now: Optional[float] = None) -> None:
        assert self.serial_port is not None

        if now is None:
            now = time.monotonic()

        self.state.advance_counters(now)
        elapsed = now - self.start_time
        value = self.state.metric_value(parameter, elapsed)
        line = format_checked_message("RES", parameter, value)
        self.serial_port.write(line.encode("ascii"))
        self.serial_port.flush()
        self.log(f"TX {line.strip()}")

    @staticmethod
    def log(message: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        print(f"[{timestamp}] {message}")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Send dummy BAP data to a GT Touch over USB CDC serial.",
    )
    parser.add_argument(
        "port",
        help="Serial port for the GT Touch, for example /dev/cu.usbmodemXXXX",
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=115200,
        help="Serial baud rate to open the CDC ACM port with. Default: 115200",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=1.0,
        help="Seconds between streamed metric updates after subscription. Default: 1.0",
    )
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()
    server = DummyBAPServer(
        port=args.port,
        baudrate=args.baudrate,
        update_interval=max(args.interval, 0.1),
    )

    try:
        server.open()
        server.run()
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
