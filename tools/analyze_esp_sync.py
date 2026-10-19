#!/usr/bin/env python3
"""Passive analysis of Vesta ESP Sync Probe CSV (no game access or modifications)."""
from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from pathlib import Path


FIELDS = (
    "pose_age_ms", "world_age_ms", "bone_age_ms", "pose_camera_age_ms",
    "camera_gap_ms", "camera_projection_shift_px", "frame_build_ms",
    "native_dxgi_present_ms", "present_call_ms", "pacing_wait_ms",
    "pose_to_present_ms", "sim_unchanged_ms",
    "camera_rotation_deg", "camera_translation_units",
)
SKIP_NEGATIVE = {"sim_unchanged_ms", "camera_rotation_deg", "camera_translation_units",
                 "camera_gap_ms"}


def number(row: dict[str, str], key: str) -> float | None:
    try:
        value = float(row.get(key, ""))
    except (ValueError, TypeError):
        return None
    if not math.isfinite(value) or (key in SKIP_NEGATIVE and value < 0):
        return None
    return value


def percentile(values: list[float], p: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * p
    lo = math.floor(position)
    hi = math.ceil(position)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (position - lo)


def correlation(xs: list[float], ys: list[float]) -> float | None:
    if len(xs) < 4 or len(xs) != len(ys):
        return None
    mx, my = statistics.mean(xs), statistics.mean(ys)
    numerator = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    dx = sum((x - mx) ** 2 for x in xs)
    dy = sum((y - my) ** 2 for y in ys)
    return numerator / math.sqrt(dx * dy) if dx > 0 and dy > 0 else None


def summarize(rows: list[dict[str, str]]) -> dict:
    result: dict = {"samples": len(rows)}
    for key in FIELDS:
        vals = [v for row in rows if (v := number(row, key)) is not None]
        result[key] = {
            "count": len(vals),
            "median": percentile(vals, 0.5),
            "p95": percentile(vals, 0.95),
            "max": max(vals) if vals else None,
        }
    pairs = [
        (number(row, "camera_gap_ms"), number(row, "camera_projection_shift_px"))
        for row in rows
    ]
    valid = [(a, b) for a, b in pairs if a is not None and b is not None]
    result["camera_gap_vs_projection_shift_r"] = (
        correlation([a for a, _ in valid], [b for _, b in valid]) if valid else None
    )
    for key in ("camera_rotation_deg", "camera_translation_units"):
        aligned = [(a, b) for row in rows
                   if (a := number(row, key)) is not None
                   and (b := number(row, "camera_projection_shift_px")) is not None]
        result[key + "_vs_projection_shift_r"] = (
            correlation([a for a, _ in aligned], [b for _, b in aligned])
            if aligned else None
        )
    for key in ("fallback_bones", "present_dropped", "gpu_effects", "performance_mode"):
        valid_flags = [number(row, key) for row in rows]
        vals = [value for value in valid_flags if value is not None]
        result[key + "_ratio"] = sum(bool(v) for v in vals) / len(vals) if vals else None
    drawn = sum(number(row, "draw_frames") or 0 for row in rows)
    repeated = sum(number(row, "repeated_pose_frames") or 0 for row in rows)
    result["repeated_pose_fraction_of_draw_frames"] = repeated / drawn if drawn else None
    result["draw_frames"] = drawn
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="CSV exported by ESP Sync Probe")
    parser.add_argument(
        "--phases", nargs="*", metavar="NAME:START_MS:END_MS", default=[],
        help="Explicit time windows relative to elapsed_ms (e.g. still:0:10000 rotate:10000:25000)",
    )
    parser.add_argument("--json", type=Path, dest="json_output", help="Write machine-readable results")
    parser.add_argument("--motion-split", action="store_true",
                        help="Group samples by observed between-snapshot camera changes (new CSV only)")
    parser.add_argument("--rotation-threshold", type=float, default=0.05,
                        help="Degrees of observed camera rotation considered motion (default 0.05)")
    parser.add_argument("--translation-threshold", type=float, default=0.10,
                        help="Game units of observed camera translation considered motion (default 0.10)")
    args = parser.parse_args()
    with args.csv.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or "elapsed_ms" not in reader.fieldnames:
            parser.error("Missing elapsed_ms column")
        rows = list(reader)
    if not rows:
        parser.error("No samples found")
    reports = {"file": str(args.csv), "overall": summarize(rows), "phases": {}}
    if args.motion_split:
        required = {"camera_rotation_deg", "camera_translation_units"}
        if not required.issubset(reader.fieldnames):
            parser.error("--motion-split requires a CSV with camera_rotation_deg and camera_translation_units")
        if not math.isfinite(args.rotation_threshold) or args.rotation_threshold < 0:
            parser.error("Rotation threshold must be finite and nonnegative")
        if not math.isfinite(args.translation_threshold) or args.translation_threshold < 0:
            parser.error("Translation threshold must be finite and nonnegative")
        groups = {"no_change_observed": [], "rotation_only_observed": [],
                  "translation_only_observed": [], "both_observed": [], "invalid": []}
        for row in rows:
            rotation = number(row, "camera_rotation_deg")
            translation = number(row, "camera_translation_units")
            if rotation is None or translation is None:
                group = "invalid"
            else:
                r = rotation > args.rotation_threshold
                t = translation > args.translation_threshold
                group = ("both_observed" if r and t else
                         "rotation_only_observed" if r else
                         "translation_only_observed" if t else "no_change_observed")
            groups[group].append(row)
        reports["observed_camera_motion"] = {name: summarize(group) for name, group in groups.items()}
    for spec in args.phases:
        try:
            name, start, end = spec.rsplit(":", 2)
            start, end = float(start), float(end)
            if not name or start >= end or not all(math.isfinite(x) for x in (start, end)):
                raise ValueError
        except ValueError:
            parser.error(f"Invalid phase {spec!r}; expected NAME:START_MS:END_MS")
        selection = [
            row for row in rows if (elapsed := number(row, "elapsed_ms")) is not None
            and start <= elapsed < end
        ]
        reports["phases"][name] = summarize(selection)
    print(json.dumps(reports, ensure_ascii=False, indent=2, allow_nan=False))
    if args.json_output:
        args.json_output.write_text(
            json.dumps(reports, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
