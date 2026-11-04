"""Pull the samurai/octopus SVGs and the blossom branch out of the Ludo launcher, so the
mod's art matches it. Run from the repo root: python3 tools/extract_art.py <launcher renderer dir>"""
import sys
from playwright.sync_api import sync_playwright
renderer = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/launcher/renderer'
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={'width': 1400, 'height': 900})
    pg.goto(f'file://{renderer}/index.html?store=riot#/library')
    pg.wait_for_timeout(2500)
    for kind in ('samurai', 'octopus'):
        svg = pg.evaluate(f"Friend.preview('{kind}')")
        open(f'art/src/{kind}.svg', 'w').write(svg)
    br = pg.evaluate("(document.querySelector('.ink-branch')||{}).outerHTML || ''")
    open('art/src/branch.svg', 'w').write(br)
    print(len(br))
    b.close()
