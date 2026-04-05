#!/usr/bin/env python3
"""
Generate line coverage summary for NC sources.

Usage:
  python3 tests/coverage_report.py --min-line 100

This script expects gcda/gcno files in nc/build produced by
an instrumented build (for example: make -C nc coverage).
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="NC coverage report")
    parser.add_argument(
        "--min-line",
        type=float,
        default=100.0,
        help="Minimum required total line coverage percent (default: 100.0)",
    )
    return parser.parse_args()


def parse_gcov_file(path: Path) -> tuple[str | None, int, int]:
    source_name: str | None = None
    executable = 0
    covered = 0

    with path.open("r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if "Source:" in line and source_name is None:
                source_name = line.split("Source:", 1)[1].strip()
                continue

            if ":" not in line:
                continue

            prefix = line.split(":", 1)[0].strip()
            if prefix in {"", "-", "====="}:
                continue

            if prefix == "#####":
                executable += 1
                continue

            clean = prefix.replace("*", "")
            if clean.isdigit():
                executable += 1
                if int(clean) > 0:
                    covered += 1

    return source_name, executable, covered


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    src_dir = repo_root / "nc" / "src"
    build_dir = repo_root / "nc" / "build"
    coverage_dir = repo_root / "coverage"
    gcov_dir = coverage_dir / "gcov"

    if shutil.which("gcov") is None:
        print("ERROR: gcov not found in PATH")
        print("Install gcov (or llvm-cov with gcov compatibility) and retry.")
        return 2

    if not build_dir.exists():
        print(f"ERROR: build directory not found: {build_dir}")
        print("Run coverage build first (for example: make -C nc coverage).")
        return 2

    source_files = sorted(src_dir.glob("*.c"))
    if not source_files:
        print(f"ERROR: no source files found in {src_dir}")
        return 2

    source_names = {p.name for p in source_files}

    if gcov_dir.exists():
        shutil.rmtree(gcov_dir)
    gcov_dir.mkdir(parents=True, exist_ok=True)

    gcov_cmd = ["gcov", "-p", "-o", str(build_dir)] + [str(p) for p in source_files]
    proc = subprocess.run(
        gcov_cmd,
        cwd=gcov_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if proc.returncode != 0:
        print("ERROR: gcov failed while generating coverage data")
        print(proc.stdout)
        return 2

    file_rows: list[dict[str, object]] = []
    total_exec = 0
    total_cov = 0

    for gcov_file in sorted(gcov_dir.glob("*.gcov")):
        src, executable, covered = parse_gcov_file(gcov_file)
        if not src:
            continue

        src_name = Path(src).name
        if src_name not in source_names:
            continue

        pct = 100.0 if executable == 0 else (covered * 100.0 / executable)
        file_rows.append(
            {
                "file": src_name,
                "executable_lines": executable,
                "covered_lines": covered,
                "line_coverage_percent": round(pct, 2),
            }
        )
        total_exec += executable
        total_cov += covered

    file_rows.sort(key=lambda row: (row["line_coverage_percent"], row["file"]))

    total_pct = 100.0 if total_exec == 0 else (total_cov * 100.0 / total_exec)

    coverage_dir.mkdir(parents=True, exist_ok=True)
    summary_path = coverage_dir / "summary.txt"
    json_path = coverage_dir / "summary.json"

    lines = []
    lines.append("NC Coverage Summary")
    lines.append("===================")
    lines.append(
        f"Total: {total_cov}/{total_exec} executable lines "
        f"({total_pct:.2f}%)"
    )
    lines.append("")
    lines.append("Per-file coverage:")
    for row in file_rows:
        lines.append(
            f"- {row['file']}: {row['covered_lines']}/{row['executable_lines']} "
            f"({row['line_coverage_percent']:.2f}%)"
        )
    summary_text = "\n".join(lines) + "\n"
    summary_path.write_text(summary_text, encoding="utf-8")

    payload = {
        "total": {
            "covered_lines": total_cov,
            "executable_lines": total_exec,
            "line_coverage_percent": round(total_pct, 2),
        },
        "files": file_rows,
    }
    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    print(summary_text, end="")
    print(f"Wrote: {summary_path}")
    print(f"Wrote: {json_path}")

    if total_pct + 1e-9 < args.min_line:
        print(
            f"FAIL: coverage {total_pct:.2f}% is below required {args.min_line:.2f}%"
        )
        return 1

    print(f"PASS: coverage threshold met ({args.min_line:.2f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
