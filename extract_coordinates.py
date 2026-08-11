"""Extract UDB-style performer coordinate sheets into web-ready JSON."""

from __future__ import annotations

import json
import re
from pathlib import Path

import pdfplumber


ROOT = Path(__file__).parent
PDFS = [
    ROOT / "2025 Part 1 Coordinates.pdf",
    ROOT / "2025 Part 2 Coordinates.pdf",
    ROOT / "Part 3 Coordinates.pdf",
    ROOT / "Part 4 Coordinates.pdf",
]

HEADER = re.compile(r"Performer:\s*(.*?)\s+Symbol:\s*(.*?)\s+Label:\s*(\S+)")
ROW = re.compile(
    r"^(\d+[A-Z]?)\s+(?:\[[^]]+\]\s+)?(.+?)\s+(\d+)\s+"
    r"((?:Side [12]:\s+.+?|On 50) yd ln)\s+(.+)$"
)
ZERO_ROW = re.compile(r"^(\d+[A-Z]?)\s+(\d+)\s+((?:Side [12]:\s+.+?|On 50) yd ln)\s+(.+)$")
SIDE = re.compile(
    r"(?:Side ([12]):\s+)?(?:(On)|(\d+(?:\.\d+)?) steps (Inside|Outside))\s+"
    r"(\d+) yd ln",
    re.I,
)
FB = re.compile(
    r"(?:(On)|(\d+(?:\.\d+)?) steps (In Front Of|Behind))\s+"
    r"(Front side line|Front Hash|Back Hash|Back side line)",
    re.I,
)


def parse_side(text: str) -> dict | None:
    match = SIDE.search(text)
    if not match:
        return None
    side, on, offset, direction, yard = match.groups()
    return {
        "side": int(side or 1),
        "yardLine": int(yard),
        "offset": 0 if on else float(offset),
        "relation": "on" if on else direction.lower(),
    }


def parse_front_back(text: str) -> dict | None:
    match = FB.search(text)
    if not match:
        return None
    on, offset, direction, landmark = match.groups()
    return {
        "landmark": landmark.lower().replace(" side line", " sideline"),
        "offset": 0 if on else float(offset),
        "relation": "on" if on else direction.lower(),
    }


def main() -> None:
    performers: dict[str, dict] = {}
    warnings: list[str] = []

    for part_number, pdf_path in enumerate(PDFS, start=1):
        with pdfplumber.open(pdf_path) as pdf:
            for page_number, page in enumerate(pdf.pages, start=1):
                text = page.extract_text(x_tolerance=2, y_tolerance=3) or ""
                lines = [line.strip() for line in text.splitlines() if line.strip()]
                header = HEADER.search(lines[0] if lines else "")
                if not header:
                    warnings.append(f"{pdf_path.name} page {page_number}: no header")
                    continue
                instrument, symbol, label = header.groups()
                entry = performers.setdefault(label, {
                    "label": label,
                    "instrument": instrument.strip(),
                    "symbol": symbol.strip(),
                    "sets": [],
                })
                for line in lines[2:]:
                    if line.startswith("Page "):
                        continue
                    row = ROW.match(line)
                    zero_row = ZERO_ROW.match(line) if not row else None
                    if zero_row:
                        set_name, counts, side_text, fb_text = zero_row.groups()
                        measure = "—"
                    elif row:
                        set_name, measure, counts, side_text, fb_text = row.groups()
                    if not row:
                        if not zero_row:
                            warnings.append(f"{label} part {part_number}: {line}")
                            continue
                    lateral = parse_side(side_text)
                    vertical = parse_front_back(fb_text)
                    if not lateral or not vertical:
                        warnings.append(f"{label} part {part_number} set {set_name}: coordinate parse")
                        continue
                    entry["sets"].append({
                        "part": part_number,
                        "set": set_name,
                        "measure": measure.strip(),
                        "counts": int(counts),
                        "coordinate": f"{side_text} · {fb_text}",
                        "lateral": lateral,
                        "vertical": vertical,
                    })

    payload = {
        "show": "Rancho Bernardo Royal Regiment 2025",
        "sourceFiles": [p.name for p in PDFS],
        "performers": sorted(performers.values(), key=lambda p: p["label"]),
        "warningCount": len(warnings),
    }
    output = ROOT / "data" / "coordinates.json"
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(payload, separators=(",", ":")), encoding="utf-8")
    print(f"Wrote {output}: {len(performers)} performers, "
          f"{sum(len(p['sets']) for p in performers.values())} coordinates, "
          f"{len(warnings)} warnings")


if __name__ == "__main__":
    main()
