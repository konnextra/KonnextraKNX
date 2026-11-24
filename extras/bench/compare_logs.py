#!/usr/bin/env python3
"""compare_logs.py — match the XIAO's serial log against an xknx capture, telegram by telegram.

Both logs carry wall-clock times from the same Mac (the serial log via `pio device monitor
-f time`, the capture via knx_listen.py), so a telegram seen by both should appear within a
fraction of a second in each. Every xknx telegram from the XIAO's own address must match a TX in
the serial log; every other one must match an RX. Anything left over on either side is a
telegram one listener saw and the other did not.

Reads both KnxLog::Deep output (frame hex dumps + con byte) and KnxLog::Basic summary lines.

Usage:
    python3 extras/bench/compare_logs.py <device-monitor-*.log> <knx-*.jsonl> [--own 1.1.5] [--window 1.0]
"""

import argparse
import json
import re
import statistics
from datetime import datetime

LINE = re.compile(r"^(\d\d:\d\d:\d\d\.\d{3}) > (.*)$")
FRAME = re.compile(r"(?:DRV tx frame|RX  frame): ((?:[0-9A-F]{2} ?)+)")
# KnxLog::Basic summary lines, e.g. "[knx] TX  1.1.5 -> 0/1/1  Write     01  (DPT1: on)  confirmed"
BASIC = re.compile(r"\[knx\] (?:! )?(TX|RX)  (\d+\.\d+\.\d+) -> (\d+/\d+/\d+)  (?:Write|Response)\s+((?:[0-9A-F]{2} ?)+)")


def ga(hi: int, lo: int) -> str:
    return f"{hi >> 3}/{hi & 0x07}/{lo}"


def pa(hi: int, lo: int) -> str:
    return f"{hi >> 4}.{hi & 0x0F}.{lo}"


def frame_value(b: list[int]):
    """Value as xknx reports it: 6-bit inline data for short frames, else the data bytes."""
    length = b[5] & 0x0F
    if length == 1:
        return b[7] & 0x3F
    return b[8:8 + length - 1]


def as_list(val):
    """Basic lines print inline data as one byte, so compare 6-bit values and byte lists alike."""
    return val if isinstance(val, list) else [val]


def parse_serial(path: str, day: str) -> list[dict]:
    events, pending_tx = [], None
    for raw in open(path, errors="replace"):
        m = LINE.match(raw.rstrip("\n"))
        if not m:
            continue
        t = datetime.fromisoformat(f"{day}T{m.group(1)}").timestamp()
        text = m.group(2)
        bm = BASIC.search(text)
        if bm:
            ev = {"t": t, "kind": bm.group(1), "src": bm.group(2), "dst": bm.group(3),
                  "val": [int(x, 16) for x in bm.group(4).split()], "con": None}
            if ev["kind"] == "TX":
                ev["con"] = "not confirmed" not in text
            events.append(ev)
            continue
        fm = FRAME.search(text)
        if fm:
            b = [int(x, 16) for x in fm.group(1).split()]
            ev = {"t": t, "kind": "TX" if "tx frame" in text else "RX",
                  "src": pa(b[1], b[2]), "dst": ga(b[3], b[4]), "val": frame_value(b), "con": None}
            events.append(ev)
            if ev["kind"] == "TX":
                pending_tx = ev
        elif "con byte" in text and pending_tx is not None:
            pending_tx["con"] = "positive" in text
            pending_tx = None
    return events


def parse_capture(path: str) -> list[dict]:
    out = []
    for line in open(path):
        r = json.loads(line)
        if "src" in r:
            out.append({"t": r["t"], "src": r["src"], "dst": r["dst"], "val": r.get("raw")})
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("serial")
    ap.add_argument("capture")
    ap.add_argument("--own", default="1.1.5", help="the XIAO's physical address")
    ap.add_argument("--window", type=float, default=1.0, help="max time offset for a match, seconds")
    args = ap.parse_args()

    cap = parse_capture(args.capture)
    day = datetime.fromtimestamp(cap[0]["t"]).date().isoformat() if cap else datetime.now().date().isoformat()
    ser = parse_serial(args.serial, day)

    # Only compare the time span both logs cover.
    lo = max(min(e["t"] for e in ser), min(e["t"] for e in cap)) - args.window
    hi = min(max(e["t"] for e in ser), max(e["t"] for e in cap)) + args.window
    ser = [e for e in ser if lo <= e["t"] <= hi]
    cap = [e for e in cap if lo <= e["t"] <= hi]

    used, offsets, missing_in_serial = set(), [], []
    for c in cap:
        want = "TX" if c["src"] == args.own else "RX"
        best = None
        for i, s in enumerate(ser):
            if i in used or s["kind"] != want or s["src"] != c["src"] or s["dst"] != c["dst"] \
                    or as_list(s["val"]) != as_list(c["val"]):
                continue
            d = c["t"] - s["t"]
            if abs(d) <= args.window and (best is None or abs(d) < abs(best[1])):
                best = (i, d)
        if best:
            used.add(best[0])
            offsets.append(best[1])
        else:
            missing_in_serial.append(c)
    missing_in_capture = [s for i, s in enumerate(ser) if i not in used]

    tx = [s for s in ser if s["kind"] == "TX"]
    rx = [s for s in ser if s["kind"] == "RX"]
    print(f"window {datetime.fromtimestamp(lo):%H:%M:%S} – {datetime.fromtimestamp(hi):%H:%M:%S}")
    print(f"serial: {len(tx)} TX ({sum(1 for s in tx if s['con'])} positive con, "
          f"{sum(1 for s in tx if s['con'] is False)} negative, {sum(1 for s in tx if s['con'] is None)} no con), "
          f"{len(rx)} RX")
    print(f"xknx:   {len(cap)} telegrams ({sum(1 for c in cap if c['src'] == args.own)} from {args.own})")
    print(f"matched {len(offsets)}; offset xknx−serial: "
          + (f"median {statistics.median(offsets)*1000:.0f} ms, min {min(offsets)*1000:.0f}, "
             f"max {max(offsets)*1000:.0f}" if offsets else "-"))
    print(f"in xknx but not in serial log: {len(missing_in_serial)}")
    for c in missing_in_serial:
        print(f"   {datetime.fromtimestamp(c['t']):%H:%M:%S.%f}  {c['src']} -> {c['dst']} = {c['val']}")
    print(f"in serial log but not in xknx: {len(missing_in_capture)}")
    for s in missing_in_capture:
        print(f"   {datetime.fromtimestamp(s['t']):%H:%M:%S.%f}  {s['kind']} {s['src']} -> {s['dst']} = {s['val']}")

    # Burst view: the shortest gaps between consecutive bus telegrams, as xknx saw them.
    gaps = sorted(b["t"] - a["t"] for a, b in zip(cap, cap[1:]))
    if gaps:
        print(f"shortest gaps between telegrams (xknx): {', '.join(f'{g*1000:.0f} ms' for g in gaps[:5])}")


if __name__ == "__main__":
    main()
