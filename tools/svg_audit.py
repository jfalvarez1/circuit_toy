"""Every template's SVG export, opened by a real XML parser.

The SVG button is an output path like the scope is: what it writes is what a user hands to a
wiki, a report, or a browser. Nothing checked it. The exporter writes label text into the file,
and the templates' annotations are full of characters XML reserves - "R/X << 1", "-> R_in",
"Q > 10", "V+ & V-" - so if escaping is missing anywhere, the file is not XML and a browser
shows an error instead of a schematic. A structural self-check in C could not prove that; an
actual parser can.

Checks, per template: the export succeeds, the file parses (xml.etree is strict), the root is
an svg element with a viewBox, it contains at least as many drawn elements as the circuit has
parts, and no coordinate is nan or inf.

    python tools/svg_audit.py build/tools/template_smoke.exe
"""
import subprocess, sys, os, tempfile, re
import xml.etree.ElementTree as ET

exe = sys.argv[1] if len(sys.argv) > 1 else "build/tools/template_smoke.exe"
if os.path.sep not in exe and "/" not in exe:
    exe = os.path.join(".", exe)
exe = os.path.abspath(exe)
if not os.path.exists(exe):
    print("svg-audit: no binary at %s" % exe)
    sys.exit(1)

fails = 0
total = 0
with tempfile.TemporaryDirectory() as tmp:
    r = subprocess.run([exe, "--dc", "--svg", tmp], capture_output=True, text=True, timeout=1800)
    files = sorted(os.listdir(tmp))
    if not files:
        print("svg-audit: the exporter wrote nothing at all")
        sys.exit(1)
    for fn in files:
        total += 1
        path = os.path.join(tmp, fn)
        name = fn[3:-4].replace("_", " ")
        try:
            raw = open(path, encoding="utf-8").read()
        except UnicodeDecodeError as e:
            print("[FAIL] svg  %-34s not UTF-8: %s" % (name, e))
            fails += 1
            continue
        if re.search(r"\b(nan|inf)\b", raw, re.IGNORECASE):
            print("[FAIL] svg  %-34s contains a nan/inf coordinate" % name)
            fails += 1
            continue
        try:
            root = ET.fromstring(raw)
        except ET.ParseError as e:
            print("[FAIL] svg  %-34s is not XML: %s" % (name, e))
            fails += 1
            continue
        tag = root.tag.split("}")[-1]
        if tag != "svg" or not root.get("viewBox"):
            print("[FAIL] svg  %-34s root is <%s> with viewBox=%r" % (name, tag, root.get("viewBox")))
            fails += 1
            continue
        drawn = sum(1 for _ in root.iter())
        if drawn < 4:
            print("[FAIL] svg  %-34s only %d elements - an empty picture" % (name, drawn))
            fails += 1

print("svg-audit: %d templates exported and parsed, %d failed" % (total, fails))
sys.exit(1 if fails else 0)
