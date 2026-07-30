#!/usr/bin/env python3
"""
parse_bench.py  —  Parse wrk benchmark output and print a pretty table.

Usage:
    python3 parse_bench.py results/hit_20260729_*.txt
    python3 parse_bench.py results/miss_20260729_*.txt
    python3 parse_bench.py results/hit_*.txt results/miss_*.txt
    python3 parse_bench.py results/hit_*.txt --no-csv
    python3 parse_bench.py results/hit_*.txt --label my_experiment

The script:
  • Groups runs by (mode, concurrency).
  • Computes the MEDIAN across runs for: RPS, p50, p99, timeouts.
  • Prints a coloured terminal table.
  • Auto-saves one CSV per mode to results/csv/<label>_<mode>_<timestamp>.csv

─────────────────────────────────────────────────────────────────────────────
EXPECTED INPUT FORMAT  (produced by bash scripts)
─────────────────────────────────────────────────────────────────────────────

Each file is a plain-text concatenation of wrk runs.  Every run MUST start
with a ">>>" header line followed immediately by standard wrk --latency output.

Template for ONE run block
(lines marked [OPT] are optional and skipped gracefully if absent):

    >>> mode=<hit|miss> connections=<int> threads=<int> run=<int> [version=<str>]
    Running <duration> test @ http://<host>:<port>
      <threads> threads and <connections> connections
      Thread Stats   Avg      Stdev     Max   +/- Stdev
        Latency   <val><unit>  ...
        Req/Sec   ...
      Latency Distribution
         50%    <val><unit>           ← REQUIRED for p50
         75%    <val><unit>           [OPT]
         90%    <val><unit>           [OPT]
         99%    <val><unit>           ← REQUIRED for p99
      <N> requests in <duration>, <bytes> read
    Requests/sec:  <float>             ← REQUIRED for RPS
    Transfer/sec:  <bytes>             [OPT]
    Socket errors: connect 0, read 0, write 0, timeout <int>  [OPT]

Rules the parser depends on:
  1. The ">>>" header line MUST contain: mode=<word>, connections=<int>,
     threads=<int>, run=<int>  (in that order, space-separated key=value).
  2. "Requests/sec:" MUST appear (case-sensitive, colon included).
  3. Latency percentiles MUST be indented and match "  50%" / "  99%" exactly
     (two leading spaces, then the percentage).
  4. Latency units MUST be one of: us, ms, s  (no spaces between value & unit).
  5. "timeout" (case-insensitive) is the only socket-error field extracted.
  6. A run is considered complete only if it has mode + RPS + p50 + p99.
     Incomplete runs (e.g. wrk killed mid-run) are silently dropped.

Example of a minimal valid two-run file:

    >>> mode=hit connections=8 threads=4 run=1
    Running 30s test @ http://127.0.0.1:8080
      4 threads and 8 connections
      Thread Stats   Avg      Stdev     Max   +/- Stdev
        Latency   352.00us  120.00us   5.00ms   90.00%
        Req/Sec     2.50k    400.00     3.00k    75.00%
      Latency Distribution
         50%  352.00us
         75%  450.00us
         90%  600.00us
         99%    1.46ms
      75000 requests in 30.01s, 8.90MB read
    Requests/sec:   2500.00
    Transfer/sec:    303.50KB

    >>> mode=hit connections=8 threads=4 run=2
    ...
─────────────────────────────────────────────────────────────────────────────
"""

import re
import sys
import statistics
from collections import defaultdict
from datetime import datetime
from pathlib import Path


# ── Regex patterns for wrk --latency output ────────────────────────────────

# Header written by bench_hit.sh / bench_miss.sh:
#   >>> mode=hit connections=4 threads=4 run=2
RE_HEADER = re.compile(
    r">>>\s+mode=(?P<mode>\w+)\s+connections=(?P<c>\d+)\s+threads=\d+\s+run=\d+"
)

# Requests/sec:   12345.67
RE_RPS = re.compile(r"Requests/sec:\s+([\d.]+)")

# Latency Distribution
#   50%    352.00us
#   99%      1.46ms
RE_LATENCY = re.compile(r"\s+(50|99)%\s+([\d.]+)(us|ms|s)\b")

# Socket errors: connect 0, read 0, write 0, timeout 3
RE_TIMEOUT = re.compile(r"timeout\s+(\d+)", re.IGNORECASE)


# ── Unit normalisation ──────────────────────────────────────────────────────

def to_us(value: float, unit: str) -> float:
    """Convert any latency unit to microseconds (float)."""
    return value * {"us": 1, "ms": 1_000, "s": 1_000_000}[unit]


def fmt_latency(us: float) -> str:
    """Pretty-print microseconds back to a human-readable string."""
    if us < 1_000:
        return f"{us:.0f}us"
    elif us < 1_000_000:
        ms = us / 1_000
        # Use 3 sig-figs
        if ms >= 100:
            return f"{ms:.0f}ms"
        elif ms >= 10:
            return f"{ms:.1f}ms"
        else:
            return f"{ms:.2f}ms"
    else:
        return f"{us/1_000_000:.2f}s"


# ── Format validation ───────────────────────────────────────────────────────

class FormatError(Exception):
    """Raised when a results file does not match the expected format."""


def validate_format(path: Path) -> None:
    """
    Quick pre-parse scan of *path*.  Raises FormatError with a clear message
    if the file is missing any of the three required elements:
      - at least one '>>>' run header with mode/connections/threads/run
      - at least one 'Requests/sec:' line
      - at least one '50%' AND one '99%' latency percentile line

    Also warns (to stderr) about runs that look incomplete, without aborting.
    """
    found_header   = False
    found_rps      = False
    found_p50      = False
    found_p99      = False
    bad_unit_lines = []
    incomplete_runs = 0
    # track per-run completeness
    run_has_rps = run_has_p50 = run_has_p99 = False
    in_run = False

    with path.open() as fh:
        for lineno, line in enumerate(fh, 1):
            if RE_HEADER.search(line):
                # close off previous run
                if in_run and not (run_has_rps and run_has_p50 and run_has_p99):
                    incomplete_runs += 1
                found_header = True
                in_run = True
                run_has_rps = run_has_p50 = run_has_p99 = False
                continue
            if RE_RPS.search(line):
                found_rps = run_has_rps = True
            if RE_LATENCY.search(line):
                m = RE_LATENCY.search(line)
                if m is not None:
                    pct = m.group(1)
                    if pct == "50":
                        found_p50 = run_has_p50 = True
                    elif pct == "99":
                        found_p99 = run_has_p99 = True
            # Catch wrong latency units (e.g. "ns" which the parser doesn't handle)
            if re.search(r"\s+(50|75|90|99)%\s+[\d.]+(?!us|ms|\bs\b)", line):
                bad_unit_lines.append(lineno)

    # close final run
    if in_run and not (run_has_rps and run_has_p50 and run_has_p99):
        incomplete_runs += 1

    errors = []
    if not found_header:
        errors.append(
            "  - No run header found.\n"
            "    Expected format:  >>> mode=<hit|miss> connections=<N> threads=<N> run=<N>\n"
            "    Check that the file was produced by bench_hit.sh / bench_miss.sh / bench_compare.sh."
        )
    if not found_rps:
        errors.append(
            "  - No 'Requests/sec:' line found.\n"
            "    The wrk output may be truncated or the file is not a wrk result."
        )
    if not found_p50:
        errors.append(
            "  - No '  50%' latency line found.\n"
            "    Make sure wrk was invoked with the --latency flag."
        )
    if not found_p99:
        errors.append(
            "  - No '  99%' latency line found.\n"
            "    Make sure wrk was invoked with the --latency flag."
        )
    if bad_unit_lines:
        errors.append(
            f"  - Unrecognised latency unit on line(s) {bad_unit_lines}.\n"
            "    Parser handles: us, ms, s  only."
        )

    if errors:
        raise FormatError(
            f"[FORMAT ERROR] {path}\n" + "\n".join(errors)
        )

    if incomplete_runs:
        print(
            f"  [WARN] {path}: {incomplete_runs} incomplete run(s) detected "
            "(missing RPS or latency — likely wrk was killed mid-run). "
            "These will be silently dropped.",
            file=sys.stderr,
        )


# ── Parsing ─────────────────────────────────────────────────────────────────

def parse_file(path: Path):
    """
    Yield dicts: {mode, c, rps, p50_us, p99_us, timeouts}
    for every completed wrk run found in *path*.
    """
    current = {}  # accumulate fields for the active run

    def flush(d):
        if d.get("mode") and "rps" in d and "p50" in d and "p99" in d:
            yield {
                "mode":     d["mode"],
                "c":        d["c"],
                "rps":      d["rps"],
                "p50_us":   d["p50"],
                "p99_us":   d["p99"],
                "timeouts": d.get("timeouts", 0),
            }

    with path.open() as fh:
        for line in fh:
            m = RE_HEADER.search(line)
            if m:
                yield from flush(current)
                current = {"mode": m.group("mode"), "c": int(m.group("c"))}
                continue

            m = RE_RPS.search(line)
            if m:
                current["rps"] = float(m.group(1))
                continue

            m = RE_LATENCY.search(line)
            if m:
                pct, val, unit = m.group(1), float(m.group(2)), m.group(3)
                current[f"p{pct}"] = to_us(val, unit)
                continue

            m = RE_TIMEOUT.search(line)
            if m:
                current["timeouts"] = int(m.group(1))

    yield from flush(current)


# ── Aggregation ─────────────────────────────────────────────────────────────

def aggregate(records):
    """
    Group records by (mode, c) and return median stats.
    Returns: { mode: [ {c, rps, p50_us, p99_us, timeouts}, ... ] }
    Rows within each mode are sorted by c ascending.
    """
    buckets = defaultdict(lambda: defaultdict(list))
    # buckets[mode][c] = list of dicts

    for r in records:
        buckets[r["mode"]][r["c"]].append(r)

    result = {}
    for mode, by_c in buckets.items():
        rows = []
        for c in sorted(by_c):
            runs = by_c[c]
            n = len(runs)
            rows.append({
                "c":        c,
                "n":        n,
                "rps":      statistics.median(r["rps"]      for r in runs),
                "p50_us":   statistics.median(r["p50_us"]   for r in runs),
                "p99_us":   statistics.median(r["p99_us"]   for r in runs),
                "timeouts": statistics.median(r["timeouts"] for r in runs),
            })
        result[mode] = rows
    return result


# ── Pretty-print ─────────────────────────────────────────────────────────────

RESET  = "\033[0m"
BOLD   = "\033[1m"
CYAN   = "\033[96m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
DIM    = "\033[2m"

def print_table(mode: str, rows: list):
    # Column widths
    col_c   = max(len("c"),   max(len(str(r["c"])) for r in rows))
    col_rps = max(len("RPS"), max(len(f"{r['rps']:,.0f}") for r in rows))
    col_p50 = max(len("p50"), max(len(fmt_latency(r["p50_us"])) for r in rows))
    col_p99 = max(len("p99"), max(len(fmt_latency(r["p99_us"])) for r in rows))
    col_to  = max(len("Timeouts"), max(len(str(int(r["timeouts"]))) for r in rows))
    col_n   = max(len("runs"), max(len(str(r["n"])) for r in rows))

    sep = "─"
    total_w = col_c + col_rps + col_p50 + col_p99 + col_to + col_n + 5 * 3

    print()
    print(f"{BOLD}{CYAN}  mode = {mode}{RESET}")
    print(f"  {sep * total_w}")
    header = (
        f"  {BOLD}{'c':<{col_c}}   "
        f"{'RPS':>{col_rps}}   "
        f"{'p50':>{col_p50}}   "
        f"{'p99':>{col_p99}}   "
        f"{'Timeouts':>{col_to}}   "
        f"{'runs':>{col_n}}{RESET}"
    )
    print(header)
    print(f"  {sep * total_w}")

    for i, r in enumerate(rows):
        to_val = int(r["timeouts"])
        to_str = f"{YELLOW}{to_val}{RESET}" if to_val > 0 else str(to_val)
        rps_str = f"{GREEN}{r['rps']:>{col_rps},.0f}{RESET}"
        row = (
            f"  {r['c']:<{col_c}}   "
            f"{rps_str}   "
            f"{fmt_latency(r['p50_us']):>{col_p50}}   "
            f"{fmt_latency(r['p99_us']):>{col_p99}}   "
            f"{to_str:>{col_to}}   "
            f"{r['n']:>{col_n}}"
        )
        print(row)

    print(f"  {sep * total_w}")
    print()


# ── CSV export ───────────────────────────────────────────────────────────────

CSV_DIR = Path("results/csv")


def write_csv(agg: dict, label: str | None = None):
    """Write one CSV per mode into results/csv/, with a timestamped filename."""
    import csv
    CSV_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    written = []
    for mode in sorted(agg):
        stem = f"{label}_{mode}" if label else mode
        out_path = CSV_DIR / f"{stem}_{timestamp}.csv"
        with out_path.open("w", newline="") as fh:
            writer = csv.writer(fh)
            writer.writerow(["mode", "c", "rps", "p50", "p99", "timeouts", "runs"])
            for r in agg[mode]:
                writer.writerow([
                    mode,
                    r["c"],
                    f"{r['rps']:.1f}",
                    fmt_latency(r["p50_us"]),
                    fmt_latency(r["p99_us"]),
                    int(r["timeouts"]),
                    r["n"],
                ])
        written.append(out_path)
        print(f"  CSV written → {out_path}")
    return written


# ── Entry point ──────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 parse_bench.py <results_file> [results_file ...] [--no-csv] [--label NAME]")
        sys.exit(1)

    files = []
    save_csv = True   # CSV is written by default
    label = None      # optional prefix for CSV filenames
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--no-csv":
            save_csv = False
        elif args[i] == "--label":
            i += 1
            label = args[i]
        else:
            p = Path(args[i])
            if not p.exists():
                print(f"[WARN] file not found: {p}", file=sys.stderr)
            else:
                files.append(p)
        i += 1

    if not files:
        print("[ERROR] no input files found.", file=sys.stderr)
        sys.exit(1)

    # Validate format before doing any real work
    for f in files:
        try:
            validate_format(f)
        except FormatError as e:
            print(e, file=sys.stderr)
            sys.exit(1)

    # Parse all files
    all_records = []
    for f in files:
        recs = list(parse_file(f))
        print(f"  {DIM}parsed {len(recs)} runs from {f}{RESET}")
        all_records.extend(recs)

    if not all_records:
        print("[ERROR] no benchmark runs found in input files.", file=sys.stderr)
        sys.exit(1)

    agg = aggregate(all_records)

    # Print tables (hit first if present, then miss)
    for mode in sorted(agg, key=lambda m: (m != "hit", m)):
        print_table(mode, agg[mode])

    if save_csv:
        write_csv(agg, label)


if __name__ == "__main__":
    main()
