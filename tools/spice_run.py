#!/usr/bin/env python3
"""Run an external index.json SPICE corpus through template_smoke --netlist-solve.

Records every diagnostic and exit code. --self-test checks the classifier without
requiring the external course; cli_smoke.py includes those checks in the battery.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile


def classify(output, returncode):
    skipped = re.search(r"skipped (\d+) lines?\b", output)
    match = re.search(r"residual \(A\*x - b[^\n]*?: (\S+) A", output)
    try:
        residual = float(match[1]) if match else None
    except ValueError:
        residual = None
    if "NO OPERATING POINT" in output or "placed nothing" in output:
        status = "refused"
    elif "CONVERGED BUT NOT FINITE" in output:
        status = "nonfinite"
    elif "IMPLAUSIBLE" in output:
        status = "implausible"
    elif "NOT A SOLUTION" in output or (residual is not None and
                                       (not math.isfinite(residual) or residual > 1e-6)):
        status = "not_solution"
    elif "KCL VIOLATED" in output:
        status = "kcl_violation"
    elif returncode != 0 or residual is None:
        status = "error"
    else:
        status = "solution"
    return dict(status=status, skipped=int(skipped[1]) if skipped else 0,
                residual=residual if residual is not None and math.isfinite(residual) else None,
                returncode=returncode)


def self_test():
    residual = "  residual (A*x - b at the solver's own final iterate): 2e-12 A\n"
    cases = [
        (residual, 0, "solution", 0),
        ("placed 3 parts; skipped 1 line\n" + residual, 0, "solution", 1),
        ("placed 3 parts; skipped 12 lines\n" + residual, 0, "solution", 12),
        (residual + "NOT A SOLUTION\n", 0, "not_solution", 0),
        (residual.replace("2e-12", "0.001"), 0, "not_solution", 0),
        (residual.replace("2e-12", "nan"), 0, "not_solution", 0),
        (residual + "IMPLAUSIBLE - source carries 2.7e5 A", 1, "implausible", 0),
        (residual + "KCL VIOLATED at out", 1, "kcl_violation", 0),
        (residual + "CONVERGED BUT NOT FINITE", 1, "nonfinite", 0),
        ("NO OPERATING POINT", 1, "refused", 0),
        ("placed nothing (cannot open file)", 1, "refused", 0),
        (residual, 1, "error", 0),
        ("", 0, "error", 0),
    ]
    failures = 0
    for output, code, status, skipped in cases:
        got = classify(output, code)
        if (got["status"], got["skipped"]) != (status, skipped):
            print(f"FAIL spice classifier: expected {status}/{skipped}, got {got}")
            failures += 1
    print(f"spice classifier: {len(cases)} checks, {failures} failures")
    return failures


def cli_test(exe):
    """Check the real CLI, including diagnostics printed after a small residual."""
    cases = [
        ("mixed-case nets", "V1 Rail 0 DC 10\nR1 rail mid 1k\nR2 MID 0 1k\n", True),
        ("AC current offset", "I1 0 n SIN(3m 100m 60)\nR1 n 0 100\n", True),
        ("missing ground", "R1 a b 1k\n", False),
        ("overdriven diode", "V1 n 0 DC 2\nD1 n 0 1N4148\n", False),
        ("clamped BJT", "V1 b 0 DC 1.2\nV2 c 0 DC 0.1\nQ1 c b 0 2N3904\n", False),
        ("KCL across current scales",
         "Vbulk bulk 0 DC 100MEG\nRbulk bulk 0 1MEG\n"
         "V1 b 0 DC 1.2\nV2 c 0 DC 0.1\nQ1 c b 0 2N3904\n", False),
        ("grounded-input two-stage opamp",
         "VCC vcc 0 DC 15\nVEE 0 vee DC 15\nVINP inp 0 AC 1m\n"
         "Q1 c1 inm e12 2N3904\nQ2 s2out inp e12 2N3904\n"
         "Q3 c1 c1 vcc 2N3906\nQ4 s2out c1 vcc 2N3906\nITAIL1 e12 vee DC 100u\n"
         "Q5 s3in s2out vee 2N3904\nR5 vcc s3in 10k\nCC s2out s3in 30p\n"
         "Q6 vcc s3in out 2N3904\nQ7 vee s3in out 2N3906\n"
         "RLOAD out 0 10k\nRF inm out 10k\nRI inp inm 10k\n", False),
    ]
    failures = 0
    with tempfile.TemporaryDirectory(prefix="spice_cli_") as scratch:
        path = Path(scratch) / "input.cir"
        for name, netlist, accepted in cases:
            path.write_text(netlist, encoding="utf-8")
            result = subprocess.run([str(Path(exe).resolve()), "--netlist-solve", str(path)],
                                    cwd=scratch, capture_output=True, text=True,
                                    errors="replace", timeout=30)
            output = result.stdout + result.stderr
            row = classify(output, result.returncode)
            ok = ((row["status"] == "solution") == accepted and
                  (result.returncode == 0) == accepted)
            if name == "AC current offset":
                current = re.search(r"^\s+I1\s+(\S+) A through it", output, re.M)
                ok = ok and current is not None and abs(float(current[1]) - .003) < 1e-9
            if name == "mixed-case nets":
                voltage = re.search(r"^\s+mid\s+(\S+) V", output, re.M)
                ok = ok and voltage is not None and abs(float(voltage[1]) - 5) < 1e-6
            if not ok:
                print(f"FAIL netlist CLI {name}: {row['status']}, exit {result.returncode}\n{output}")
                failures += 1
    print(f"netlist CLI: {len(cases)} checks, {failures} failures")
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", nargs="?", type=Path, help="directory containing index.json")
    parser.add_argument("--exe", type=Path, default=Path("build/tools/template_smoke.exe"))
    parser.add_argument("--output", type=Path, help="write a JSON report including raw diagnostics")
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--check-cli", action="store_true", help="test actual solver diagnostics")
    args = parser.parse_args()
    if args.self_test:
        return 1 if self_test() else 0
    if args.check_cli:
        return 1 if cli_test(args.exe) else 0
    if args.corpus is None:
        parser.error("provide a corpus directory or --self-test")
    exe = args.exe.resolve()
    if not exe.is_file():
        parser.error(f"solver executable does not exist: {exe}")
    entries = json.loads((args.corpus / "index.json").read_text(encoding="utf-8"))
    if not entries:
        parser.error("corpus index is empty")
    rows = []
    # Some CLI modes write diagnostics relative to cwd; keep them out of the source tree.
    with tempfile.TemporaryDirectory(prefix="spice_corpus_") as scratch:
        for entry in entries:
            path = (args.corpus / entry["file"]).resolve()
            try:
                result = subprocess.run([str(exe), "--netlist-solve", str(path)],
                                        cwd=scratch, capture_output=True, text=True,
                                        errors="replace", timeout=args.timeout)
                output = result.stdout + result.stderr
                row = classify(output, result.returncode)
            except subprocess.TimeoutExpired:
                output = f"timed out after {args.timeout} seconds"
                row = dict(status="timeout", skipped=0, residual=None, returncode=None)
            row.update(id=entry["id"], file=entry["file"], output=output)
            rows.append(row)
    counts = dict(sorted(Counter(row["status"] for row in rows).items()))
    clean = sum(row["status"] == "solution" and row["skipped"] == 0 for row in rows)
    print(f"{len(rows)} circuits: {counts}; {clean} solutions with nothing skipped")
    for row in rows:
        if row["status"] != "solution":
            print(f"  {row['id']}: {row['status']} (exit {row['returncode']})")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(dict(counts=counts, clean=clean, results=rows),
                                          indent=2, allow_nan=False) + "\n", encoding="utf-8")
    # A diagnostic corpus can contain deliberately unsolvable circuits. Still make any
    # rejected result visible to automation instead of silently declaring the run green.
    return 1 if any(row["status"] != "solution" for row in rows) else 0


if __name__ == "__main__":
    raise SystemExit(main())
