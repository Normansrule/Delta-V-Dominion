"""Draws the README progress bars as SVG files in docs/media/progress (GitHub renders them as images).

Edit PROGRESS below at each milestone and re-run:  python Scripts/make_progress_bars.py
Percentages are judged against the plan in docs/CAMPAIGN.md, not against a finished AAA game.
"""

import os

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "media", "progress")

# key: (label, percent)
PROGRESS = {
    "combat": ("Combat and squad", 80),
    "missions": ("Missions and maps", 50),
    "vehicles": ("Vehicles", 75),
    "kestrel": ("Kestrel hub", 75),
    "towns": ("Towns and factions", 42),
    "coop": ("Co-op", 25),
    "art": ("Art and audio", 15),
    "campaign": ("Campaign and story", 24),
}


def colour(p):
    if p >= 70:
        return "#2ea44f"
    if p >= 40:
        return "#d4a72c"
    return "#cf5b2e"


def bar(path, label, p, width=420, height=26):
    fill = int((width - 4) * p / 100)
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" role="img" aria-label="{label}: {p}%">
  <title>{label}: {p}%</title>
  <rect x="0" y="0" width="{width}" height="{height}" rx="6" fill="#1f2328"/>
  <rect x="2" y="2" width="{fill}" height="{height - 4}" rx="5" fill="{colour(p)}"/>
  <text x="{width / 2}" y="{height / 2 + 5}" text-anchor="middle" font-family="Segoe UI, Helvetica, Arial, sans-serif"
        font-size="14" font-weight="600" fill="#ffffff">{p}%</text>
</svg>
"""
    with open(path, "w", encoding="utf-8") as f:
        f.write(svg)


def main():
    os.makedirs(OUT, exist_ok=True)
    for key, (label, p) in PROGRESS.items():
        bar(os.path.join(OUT, key + ".svg"), label, p)
    overall = round(sum(p for _, p in PROGRESS.values()) / len(PROGRESS))
    bar(os.path.join(OUT, "overall.svg"), "Overall", overall, width=720, height=34)
    print("overall %d%%" % overall)


if __name__ == "__main__":
    main()
