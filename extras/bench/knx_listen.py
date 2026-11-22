#!/usr/bin/env python3
"""knx_listen.py — record all KNX group traffic seen by a KNXnet/IP interface.

Runs unattended: connect to the interface (auto-discovered unless --gateway is given), write
every telegram as one JSON line to a log file, stop on Ctrl-C or after --minutes. Meant for a
Mac that sits in the test case's own network without internet, so the log is analysed later.

Limits of a KNXnet/IP tunnel: it shows telegrams, not TP-level ACKs or repetitions.

Setup (once):
    /opt/homebrew/bin/python3.12 -m venv extras/bench/.venv
    extras/bench/.venv/bin/pip install xknx
Run:
    extras/bench/.venv/bin/python extras/bench/knx_listen.py
    extras/bench/.venv/bin/python extras/bench/knx_listen.py --gateway 192.168.1.10 --minutes 30
    extras/bench/.venv/bin/python extras/bench/knx_listen.py --scan       # just find interfaces
    extras/bench/.venv/bin/python extras/bench/knx_listen.py --routing    # IP router: multicast
"""

import argparse
import asyncio
import json
import signal
import sys
import time
from datetime import datetime
from pathlib import Path

from xknx import XKNX
from xknx.io import ConnectionConfig, ConnectionType, GatewayScanner
from xknx.exceptions import CommunicationError
from xknx.telegram import Telegram


def describe_payload(telegram: Telegram) -> dict:
    """Service name and raw value of a telegram, without assuming a DPT."""
    payload = telegram.payload
    out = {"service": type(payload).__name__}
    value = getattr(payload, "value", None)
    if value is not None:
        raw = getattr(value, "value", value)        # DPTBinary.value is int, DPTArray.value a tuple
        if isinstance(raw, (tuple, list)):
            out["kind"] = "array"
            out["raw"] = list(raw)
            out["hex"] = " ".join(f"{b:02X}" for b in raw)
        else:
            out["kind"] = "binary"
            out["raw"] = raw
    return out


async def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gateway", help="IP of the KNXnet/IP interface (default: auto-discover)")
    ap.add_argument("--local-ip", help="local IP or interface to use (default: let xknx choose)")
    ap.add_argument("--minutes", type=float, default=0, help="stop after this many minutes (0 = until Ctrl-C)")
    ap.add_argument("--routing", action="store_true",
                    help="listen to KNX IP routing multicast (IP router) instead of tunnelling")
    ap.add_argument("--scan", action="store_true", help="only search for interfaces, print them, exit")
    ap.add_argument("--out", help="log file (default: extras/bench/logs/knx-YYYYmmdd-HHMMSS.jsonl)")
    args = ap.parse_args()

    if args.scan:
        xknx = XKNX()
        found = 0
        print(f"searching for KNXnet/IP interfaces (local ip {args.local_ip or 'auto'}) ...", flush=True)
        async for gw in GatewayScanner(xknx, local_ip=args.local_ip, timeout_in_seconds=5).async_scan():
            found += 1
            print(f"  {gw.name!r}  {gw.ip_addr}:{gw.port}  individual {gw.individual_address}  "
                  f"tunnelling {gw.supports_tunnelling}  routing {gw.supports_routing}", flush=True)
        print(f"{found} found" if found else
              "none found — interface in another subnet? Read its IP from ETS, or try --routing.")
        return 0 if found else 1

    out_path = Path(args.out) if args.out else (
        Path(__file__).parent / "logs" / f"knx-{datetime.now():%Y%m%d-%H%M%S}.jsonl")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    log = out_path.open("a", buffering=1)            # line-buffered: survives a hard stop
    count = 0

    def on_telegram(telegram: Telegram) -> None:
        nonlocal count
        count += 1
        now = time.time()
        rec = {
            "t": round(now, 3),
            "time": datetime.fromtimestamp(now).isoformat(timespec="milliseconds"),
            "dir": telegram.direction.value,          # "Incoming" or "Outgoing"
            "src": str(telegram.source_address),
            "dst": str(telegram.destination_address),
            **describe_payload(telegram),
        }
        log.write(json.dumps(rec) + "\n")
        print(f'{rec["time"]}  {rec["src"]:>9} -> {rec["dst"]:<9} {rec["service"]:<20} '
              f'{rec.get("hex", rec.get("raw", ""))}', flush=True)

    if args.routing:
        config = ConnectionConfig(connection_type=ConnectionType.ROUTING, local_ip=args.local_ip)
    elif args.gateway:
        config = ConnectionConfig(connection_type=ConnectionType.TUNNELING,
                                  gateway_ip=args.gateway, local_ip=args.local_ip)
    else:
        config = ConnectionConfig(connection_type=ConnectionType.AUTOMATIC, local_ip=args.local_ip)

    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop.set)

    xknx = XKNX(connection_config=config)
    xknx.telegram_queue.register_telegram_received_cb(on_telegram, match_for_outgoing=True)
    log.write(json.dumps({"t": round(time.time(), 3), "event": "start",
                          "gateway": "routing" if args.routing else (args.gateway or "auto")}) + "\n")
    print(f"connecting ({"routing multicast" if args.routing else (args.gateway or "auto-discover")}) ... logging to {out_path}", flush=True)
    try:
        await xknx.start()
    except CommunicationError as err:
        print(f"!! {err} — is this Mac in the interface's network? Try --gateway <IP>.", flush=True)
        log.close()
        return 1
    print("connected — Ctrl-C to stop", flush=True)
    try:
        if args.minutes > 0:
            try:
                await asyncio.wait_for(stop.wait(), timeout=args.minutes * 60)
            except asyncio.TimeoutError:
                pass
        else:
            await stop.wait()
    finally:
        await xknx.stop()                            # frees the tunnel slot on the interface
        log.write(json.dumps({"t": round(time.time(), 3), "event": "stop", "telegrams": count}) + "\n")
        log.close()
        print(f"stopped, {count} telegrams in {out_path}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
