"""Draws every image the Ludo mod uses, so the art is reproducible and matches the launcher.

All sizes below are in game points; the images are written at 4x (Geode treats the files in
resources/ as UHD, where one point is four pixels).

    python3 tools/extract_art.py     # once, pulls the launcher's creatures and branch
    python3 tools/make_art.py [name ...]
"""
import os
import random
import sys
from playwright.sync_api import sync_playwright

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'art', 'src')
OUT = os.path.join(ROOT, 'resources')
os.makedirs(OUT, exist_ok=True)

INK = '#0e0b10'
RED = '#ff4757'
CHALK = '#ece4de'


def read(name):
    with open(os.path.join(SRC, name)) as f:
        return f.read()


def page(body, css='', w=100, h=100):
    return f"""<!doctype html><html><head><meta charset="utf-8"><style>
html,body{{margin:0;background:transparent}}
#a{{width:{w}px;height:{h}px;position:relative;overflow:hidden}}
{css}</style></head><body><div id="a">{body}</div></body></html>"""


# ---------------------------------------------------------------- pieces

def splatter(seed, w, h, n):
    r = random.Random(seed)
    out = []
    for _ in range(n):
        x, y = r.random() * w, r.random() * h
        s = 0.4 + r.random() ** 2 * 2.2
        red = r.random() < 0.28
        col = 'rgba(255,71,87,%.2f)' % (0.18 + r.random() * 0.3) if red else 'rgba(236,228,222,%.2f)' % (0.05 + r.random() * 0.1)
        out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{s:.2f}" fill="{col}"/>')
    return ''.join(out)


def grain(w, h, opacity=0.07, seed=3):
    return f"""<svg width="{w}" height="{h}" style="position:absolute;left:0;top:0;mix-blend-mode:overlay;opacity:{opacity}">
<filter id="g"><feTurbulence type="fractalNoise" baseFrequency="1.1" numOctaves="2" seed="{seed}"/><feColorMatrix values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 1.4 -.2"/></filter>
<rect width="100%" height="100%" filter="url(#g)"/></svg>"""


def smoke(w, h, seed):
    r = random.Random(seed)
    blobs = []
    for i in range(7):
        x, y = r.random() * w, h * (0.35 + r.random() * 0.7)
        rx, ry = 60 + r.random() * 120, 24 + r.random() * 50
        blobs.append(f'<ellipse cx="{x:.0f}" cy="{y:.0f}" rx="{rx:.0f}" ry="{ry:.0f}" fill="rgba(210,196,200,{0.035 + r.random() * 0.045:.3f})"/>')
    return f'<svg width="{w}" height="{h}" style="position:absolute;left:0;top:0;filter:blur(18px)">{"".join(blobs)}</svg>'


def ridge_path(w, base, amp, seed, rough=5, step=4):
    """A jagged ink-wash ridge line along the bottom, by midpoint displacement."""
    r = random.Random(seed)
    pts = [base - amp * .3, base - amp * .3]
    n = 2
    for lvl in range(rough):
        new = []
        for i in range(len(pts) - 1):
            new.append(pts[i])
            new.append((pts[i] + pts[i + 1]) / 2 + r.uniform(-1, 1) * amp / (1.9 ** lvl))
        new.append(pts[-1])
        pts = new
    xs = [w * i / (len(pts) - 1) for i in range(len(pts))]
    d = 'M0 %d ' % (base + 40) + ' '.join(f'L{x:.1f} {min(y, base + 20):.1f}' for x, y in zip(xs, pts)) + f' L{w} {base + 40} Z'
    return d


def frame(radius, inset_red=3.0):
    return f"""<div style="position:absolute;inset:0;border-radius:{radius}px;box-shadow:inset 0 0 0 1px rgba(236,228,222,.20), inset 0 0 28px rgba(0,0,0,.55)"></div>
<div style="position:absolute;inset:{inset_red}px;border-radius:{radius - inset_red}px;border:1.1px solid rgba(255,71,87,.5)"></div>
<div style="position:absolute;inset:{inset_red + 3.5}px;border-radius:{radius - inset_red - 3.5}px;border:.6px solid rgba(236,228,222,.1)"></div>"""


def blossom_svg(grad_id='bp', size=20, dim=False, uid=''):
    """One five petal blossom, drawn the same way as the launcher's branch."""
    if dim:
        stops = [('0%', '#9a92a3'), ('50%', '#6e6578'), ('100%', '#4a4252')]
        centre, dots = '#a79fb0', '#c9c2d0'
    else:
        stops = [('0%', '#fff1f8'), ('45%', '#ffb6d6'), ('100%', '#e8629e')]
        centre, dots = '#ffe7a0', '#fff6cf'
    g = f'<radialGradient id="{grad_id}{uid}" cx="30%" cy="50%" r="80%">' + ''.join(f'<stop offset="{o}" stop-color="{c}"/>' for o, c in stops) + '</radialGradient>'
    petals = ''.join(
        f'<ellipse cx="{size * .42:.2f}" cy="0" rx="{size * .5:.2f}" ry="{size * .34:.2f}" fill="url(#{grad_id}{uid})" transform="rotate({k * 72 - 90})" stroke="rgba(120,20,60,.35)" stroke-width=".35"/>'
        for k in range(5))
    import math
    d = ''.join(
        f'<circle cx="{math.cos(a) * size * .26:.2f}" cy="{math.sin(a) * size * .26:.2f}" r="{size * .035:.2f}" fill="{dots}"/>'
        for a in [i / 6 * 6.2832 + 0.4 for i in range(6)])
    return g, petals + f'<circle r="{size * .14:.2f}" fill="{centre}"/>' + d


# ---------------------------------------------------------------- assets

def hub_bg():
    w, h = 460, 290
    body = f"""
<div style="position:absolute;inset:0;border-radius:18px;overflow:hidden;
 background:radial-gradient(120% 95% at 72% 6%, #2a1a22 0%, #150f16 46%, #0a070c 100%)">
 {smoke(w, h, 11)}
 <svg width="{w}" height="{h}" style="position:absolute;left:0;top:0">
  <path d="{ridge_path(w, h - 58, 34, 3)}" fill="rgba(236,228,222,.030)"/>
  <path d="{ridge_path(w, h - 40, 26, 9)}" fill="rgba(236,228,222,.045)"/>
  <path d="{ridge_path(w, h - 24, 18, 21)}" fill="rgba(20,13,19,.78)"/>
 </svg>
 <svg width="{w}" height="{h}" style="position:absolute;left:0;top:0">{splatter(5, w, h, 150)}</svg>
 <div style="position:absolute;inset:0;background:linear-gradient(0deg,rgba(7,5,9,.55),transparent 38%)"></div>
 {grain(w, h, .085)}
</div>{frame(18)}"""
    return page(body, w=w, h=h), (w, h)


def panel():
    w = h = 96
    body = f"""
<div style="position:absolute;inset:0;border-radius:18px;overflow:hidden;
 background:radial-gradient(120% 100% at 70% 0%, #241721 0%, #130e14 55%, #0a070c 100%)">
 {grain(w, h, .07, 8)}
</div>{frame(18)}"""
    return page(body, w=w, h=h), (w, h)


def tile():
    w = h = 48
    body = """<div style="position:absolute;inset:0;border-radius:10px;
 background:linear-gradient(180deg,rgba(60,46,62,.92),rgba(34,26,38,.92));
 box-shadow:inset 0 0 0 .8px rgba(236,228,222,.13), inset 0 1px 0 rgba(255,255,255,.07)"></div>"""
    return page(body, w=w, h=h), (w, h)


def sun():
    w = h = 160
    body = """<div style="position:absolute;inset:0;background:radial-gradient(circle, rgba(255,60,80,.42) 0%, rgba(255,50,72,.22) 34%, rgba(255,50,72,.08) 52%, transparent 70%)"></div>
<div style="position:absolute;left:34px;top:34px;width:92px;height:92px;border-radius:50%;overflow:hidden;
 background:radial-gradient(circle at 40% 34%, #ff6e5f 0%, #e22d42 52%, #9b1228 100%)">
 <svg width="92" height="92" style="position:absolute;left:0;top:0;mix-blend-mode:soft-light;opacity:.5">
 <filter id="b"><feTurbulence type="fractalNoise" baseFrequency=".015 .5" numOctaves="3" seed="4"/><feColorMatrix values="0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0 2.4 -.9"/></filter>
 <rect width="92" height="92" filter="url(#b)"/></svg></div>"""
    return page(body, w=w, h=h), (w, h)


BRANCH_W = 190
BRANCH_H = 118
BRANCH_K = BRANCH_W / 760.0          # svg units -> points


MTN_W, MTN_H = 196, 70


def mountain_card():
    """Dusk sky behind the death mountain: sun low on the horizon, far ridges, mist. The
    mod draws your deaths on top as the near ridge."""
    w, h = MTN_W, MTN_H
    far1 = ridge_path(w, 42, 15, 5, rough=5)
    far2 = ridge_path(w, 34, 11, 17, rough=5)
    body = f"""<div style="position:absolute;inset:0;border-radius:10px;overflow:hidden;
 background:linear-gradient(180deg,#24121c 0%,#4a1e2c 38%,#93313f 68%,#d9524f 88%,#f08a6a 100%)">
 <div style="position:absolute;left:118px;top:12px;width:34px;height:34px;border-radius:50%;
  background:radial-gradient(circle at 40% 34%,#fff0d8 0%,#ffc0a0 45%,#ff7a6a 100%);box-shadow:0 0 18px 8px rgba(255,150,120,.45)"></div>
 <svg width="{w}" height="{h}" style="position:absolute;left:0;top:0">
  <path d="{far1}" fill="#3c1a2b"/>
  <path d="{far2}" fill="#26101c"/>
 </svg>
 <div style="position:absolute;left:0;right:0;top:34px;height:16px;background:linear-gradient(180deg,transparent,rgba(255,220,210,.14),transparent);filter:blur(3px)"></div>
 <div style="position:absolute;left:0;right:0;bottom:0;height:14px;background:#0b080d"></div>
</div>
<div style="position:absolute;inset:0;border-radius:10px;box-shadow:inset 0 0 0 .8px rgba(236,228,222,.16)"></div>"""
    return page(body, w=w, h=h), (w, h)


def branch_parts():
    """The launcher's branch, split in two: bare wood (drawn as one image) and the blossoms
    (placed one by one by the mod, so they can light up with your progress)."""
    svg = read('branch.svg')
    i = svg.index('<g class="ink-flowers">')
    wood = svg[:i] + '</svg>'
    import re
    buds = []
    for x, y, rot, inner in re.findall(r'<g transform="translate\(([-\d.]+) ([-\d.]+)\) rotate\(([-\d.]+)\)">(.*?)</g>', svg[i:]):
        size = float(re.findall(r'rx="([\d.]+)"', inner)[0]) * 2
        buds.append((float(x) * BRANCH_K, (float(y) + 30) * BRANCH_K, size * BRANCH_K, float(rot)))
    buds.sort()                       # trunk first, tips last
    return wood, buds


def write_bud_data():
    _, buds = branch_parts()
    lines = ['// Generated by tools/make_art.py: where each blossom sits on the branch image.',
             '// x/y are points from the image\'s top-left, size is the blossom size, rot its turn in degrees.',
             '#pragma once', '', 'namespace ludo {', '', 'struct Bud { float x, y, size, rot; };', '',
             f'inline constexpr float kBranchW = {BRANCH_W}.f;', f'inline constexpr float kBranchH = {BRANCH_H}.f;',
             f'inline constexpr float kBudRef = 20.f;   // size the bud images are drawn at', '',
             'inline constexpr Bud kBuds[] = {']
    for x, y, size, rot in buds:
        lines.append(f'    {{{x:.2f}f, {y:.2f}f, {size:.2f}f, {rot:.0f}.f}},')
    lines += ['};', '', 'inline constexpr int kBudCount = static_cast<int>(sizeof(kBuds) / sizeof(kBuds[0]));', '', '}', '']
    with open(os.path.join(ROOT, 'src', 'BranchData.hpp'), 'w') as f:
        f.write('\n'.join(lines))


def branch():
    w, h = BRANCH_W, BRANCH_H
    wood, _ = branch_parts()
    wood = wood.replace('class="ink-branch"', f'width="{w}" height="{h}"')
    body = f"""<div style="width:{w}px;height:{h}px;-webkit-mask-image:linear-gradient(180deg,#000 0%,#000 42%,transparent 82%);
 filter:drop-shadow(0 0 3px rgba(255,120,180,.25))">{wood}</div>"""
    return page(body, w=w, h=h), (w, h)


def bud(kind):
    """One blossom at the reference size (20 pt). kind: lit, dim, glow."""
    w = h = 44
    if kind == 'glow':
        body = """<div style="position:absolute;inset:0;background:radial-gradient(circle, rgba(255,150,200,.75) 0%, rgba(255,120,180,.32) 34%, rgba(255,100,170,.1) 56%, transparent 72%)"></div>"""
        return page(body, w=w, h=h), (w, h)
    dim = kind == 'dim'
    g, shapes = blossom_svg('bd', 20, dim, kind)
    if dim:   # an unopened bud: muted mauve instead of the grey of a switched-off icon
        g = g.replace('#9a92a3', '#7a5f78').replace('#6e6578', '#5a4560').replace('#4a4252', '#3d3045')
    body = f"""<svg width="{w}" height="{h}" viewBox="-22 -22 44 44"><defs>{g}</defs>{shapes}</svg>"""
    return page(body, w=w, h=h), (w, h)


def petal(i):
    w = h = 16
    hues = [('#fff1f8', '#ffb6d6', '#e8629e'), ('#ffe3ef', '#ff9fc6', '#d8508a'), ('#fff7fb', '#ffc9de', '#ef7aa8')]
    a, b, c = hues[i % 3]
    body = f"""<svg width="16" height="16" viewBox="0 0 20 20" style="transform:rotate({i * 37}deg)"><defs>
<radialGradient id="p" cx="35%" cy="30%" r="85%"><stop offset="0" stop-color="{a}"/><stop offset=".5" stop-color="{b}"/><stop offset="1" stop-color="{c}"/></radialGradient></defs>
<path d="M10 19 C3 14.5 2 6.5 6 3 C8 1.4 9.5 2.6 10 4.4 C10.5 2.6 12 1.4 14 3 C18 6.5 17 14.5 10 19Z" fill="url(#p)"/></svg>"""
    return page(body, w=w, h=h), (w, h)


def blossom_icon(dim):
    w = h = 28
    g, shapes = blossom_svg('bi', 11.5, dim, 'd' if dim else 'o')
    body = f"""<svg width="28" height="28" viewBox="-14 -14 28 28"><defs>{g}</defs>
<g filter="drop-shadow(0 .6px 1px rgba(0,0,0,.5))">{shapes}</g></svg>"""
    return page(body, w=w, h=h), (w, h)


def stamp():
    w = h = 26
    body = """<svg width="26" height="26" viewBox="0 0 26 26"><defs>
<filter id="r" x="-10%" y="-10%" width="120%" height="120%"><feTurbulence type="fractalNoise" baseFrequency=".55" numOctaves="2" seed="9" result="n"/><feDisplacementMap in="SourceGraphic" in2="n" scale="1.4"/></filter>
<mask id="m"><rect width="26" height="26" fill="#fff"/>
<text x="13" y="19.4" text-anchor="middle" font-family="Noto Serif CJK JP" font-weight="900" font-size="17" fill="#000">完</text></mask>
<linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ee3a4e"/><stop offset="1" stop-color="#b9182c"/></linearGradient></defs>
<g filter="url(#r)"><rect x="2" y="2" width="22" height="22" rx="3.2" fill="url(#g)" mask="url(#m)"/>
<rect x="3.6" y="3.6" width="18.8" height="18.8" rx="2" fill="none" stroke="rgba(255,225,225,.55)" stroke-width=".7" mask="url(#m)"/></g></svg>"""
    return page(body, w=w, h=h), (w, h)


def disc(size, glyph, ring=RED, dim=False):
    s = size
    ring_col = 'rgba(236,228,222,.28)' if dim else ring
    body = f"""<div style="position:absolute;inset:2.4px;border-radius:50%;
 background:radial-gradient(circle at 38% 28%, #34262f 0%, #1a1219 60%, #0f0a10 100%);
 box-shadow:0 0 0 1.3px {ring_col}, inset 0 0 0 3px rgba(0,0,0,.35), 0 1px 2px rgba(0,0,0,.5)"></div>{glyph}"""
    return page(body, w=s, h=s), (s, s)


def hub_button():
    s = 48
    glyph = f"""<svg width="{s}" height="{s}" viewBox="0 0 48 48" style="position:absolute;left:0;top:0">
<defs><radialGradient id="s" cx="40%" cy="34%" r="70%"><stop offset="0" stop-color="#ff6e5f"/><stop offset=".55" stop-color="#e22d42"/><stop offset="1" stop-color="#9b1228"/></radialGradient></defs>
<circle cx="31" cy="17.5" r="8" fill="url(#s)"/><circle cx="31" cy="17.5" r="11" fill="rgba(255,60,80,.18)"/>
<path d="M8 35 L16.5 22.5 L21 28.5 L27.5 19 L40 35Z" fill="{CHALK}" opacity=".93"/>
<path d="M8 35 L16.5 22.5 L21 28.5 L27.5 19 L40 35Z" fill="none" stroke="rgba(14,11,16,.35)" stroke-width=".6"/>
<path d="M12 38.2 H36" stroke="{RED}" stroke-width="1.4" stroke-linecap="round"/></svg>"""
    return disc(s, glyph)


def blossom_button(dim):
    s = 44
    g, shapes = blossom_svg('bb', 12.5, dim, 'd' if dim else 'o')
    glyph = f"""<svg width="{s}" height="{s}" viewBox="-22 -22 44 44" style="position:absolute;left:0;top:0"><defs>{g}</defs>{shapes}</svg>"""
    return disc(s, glyph, dim=dim)


def scroll_glyph(cx, cy, w, h, col=CHALK):
    return f"""<g transform="translate({cx} {cy})"><rect x="{-w / 2}" y="{-h / 2 + 1.6}" width="{w}" height="{h - 3.2}" rx="1.2" fill="{col}" opacity=".92"/>
<rect x="{-w / 2 - 1.8}" y="{-h / 2}" width="3.6" height="{h}" rx="1.8" fill="#d9b25a"/><rect x="{w / 2 - 1.8}" y="{-h / 2}" width="3.6" height="{h}" rx="1.8" fill="#d9b25a"/>
<path d="M{-w / 4} {-h / 6} H{w / 4} M{-w / 4} {h / 10} H{w / 6}" stroke="rgba(14,11,16,.45)" stroke-width="1.1" stroke-linecap="round"/></g>"""


def scroll_button():
    s = 44
    glyph = f"""<svg width="{s}" height="{s}" viewBox="0 0 44 44" style="position:absolute;left:0;top:0">{scroll_glyph(22, 22, 17, 17)}</svg>"""
    return disc(s, glyph, ring='#7fa8ff')


def small_icon(kind):
    s = 26
    if kind == 'open':
        g = f'<path d="M9 13 H17 M13.5 8.5 L18 13 L13.5 17.5" fill="none" stroke="{CHALK}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>'
    elif kind == 'x':
        g = f'<path d="M9 9 L17 17 M17 9 L9 17" fill="none" stroke="{CHALK}" stroke-width="2" stroke-linecap="round"/>'
    elif kind == 'plus':
        g = f'<path d="M13 8 V18 M8 13 H18" fill="none" stroke="{CHALK}" stroke-width="2" stroke-linecap="round"/>'
    else:  # back
        g = f'<path d="M17 13 H9 M12.5 8.5 L8 13 L12.5 17.5" fill="none" stroke="{CHALK}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>'
    glyph = f'<svg width="{s}" height="{s}" viewBox="0 0 26 26" style="position:absolute;left:0;top:0">{g}</svg>'
    return disc(s, glyph, ring='rgba(255,71,87,.7)')


def check(on):
    s = 22
    if on:
        body = f"""<div style="position:absolute;inset:1.5px;border-radius:6px;background:linear-gradient(180deg,#ff5d6c,#c71f35);box-shadow:0 0 0 1px rgba(255,200,205,.35), 0 1px 3px rgba(0,0,0,.5)"></div>
<svg width="22" height="22" viewBox="0 0 22 22" style="position:absolute;left:0;top:0"><path d="M6 11.5 L9.6 15 L16 7.5" fill="none" stroke="#fff4f2" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>"""
    else:
        body = """<div style="position:absolute;inset:1.5px;border-radius:6px;background:#1a1219;box-shadow:inset 0 0 0 1.2px rgba(236,228,222,.3)"></div>"""
    return page(body, w=s, h=s), (s, s)


def pill(on):
    w, h = 64, 26
    if on:
        bg = 'linear-gradient(180deg,#ff6676,#cf2238)'
        sh = 'inset 0 1px 0 rgba(255,255,255,.35), inset 0 0 0 .8px rgba(255,160,170,.55), 0 1px 2px rgba(0,0,0,.4)'
    else:
        bg = 'linear-gradient(180deg,rgba(52,40,54,.95),rgba(30,22,34,.95))'
        sh = 'inset 0 0 0 .9px rgba(236,228,222,.2), inset 0 1px 0 rgba(255,255,255,.06)'
    body = f'<div style="position:absolute;inset:.5px;border-radius:12.5px;background:{bg};box-shadow:{sh}"></div>'
    return page(body, w=w, h=h), (w, h)


def bar(fill):
    w, h = 32, 6
    if fill:
        bg = 'linear-gradient(180deg,#ff6676,#cf2238)'
        sh = 'inset 0 .5px 0 rgba(255,255,255,.35)'
    else:
        bg = 'rgba(236,228,222,.14)'
        sh = 'inset 0 0 0 .5px rgba(236,228,222,.12)'
    body = f'<div style="position:absolute;inset:0;border-radius:3px;background:{bg};box-shadow:{sh}"></div>'
    return page(body, w=w, h=h), (w, h)


def scroll_page():
    """An unrolled scroll: wooden rods top and bottom, aged paper between. Nine-slice, 16 pt caps."""
    w, h = 120, 80
    body = f"""<svg width="{w}" height="{h}" viewBox="0 0 {w} {h}"><defs>
<linearGradient id="wood" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a1d18"/><stop offset=".35" stop-color="#7d5c3b"/><stop offset=".7" stop-color="#4a3326"/><stop offset="1" stop-color="#22171a"/></linearGradient>
<linearGradient id="gold" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#a98230"/><stop offset=".45" stop-color="#f1d58a"/><stop offset="1" stop-color="#b58d36"/></linearGradient>
<linearGradient id="paper" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#d8c7a3"/><stop offset=".12" stop-color="#eadfc4"/><stop offset=".88" stop-color="#eadfc4"/><stop offset="1" stop-color="#d4c29c"/></linearGradient>
<linearGradient id="shade" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000" stop-opacity=".42"/><stop offset=".12" stop-color="#000" stop-opacity="0"/><stop offset=".88" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".42"/></linearGradient>
<filter id="fib" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency=".06 .5" numOctaves="3" seed="7"/><feColorMatrix values="0 0 0 0 .45  0 0 0 0 .32  0 0 0 0 .16  0 0 0 .55 -.12"/></filter>
<filter id="stain"><feGaussianBlur stdDeviation="5"/></filter></defs>
<rect x="7" y="9" width="106" height="62" fill="url(#paper)"/>
<rect x="7" y="9" width="106" height="62" filter="url(#fib)" opacity=".55"/>
<g filter="url(#stain)" opacity=".5"><ellipse cx="34" cy="34" rx="18" ry="8" fill="#b89a62" opacity=".35"/><ellipse cx="92" cy="52" rx="16" ry="9" fill="#b89a62" opacity=".3"/></g>
<rect x="7" y="9" width="106" height="62" fill="url(#shade)"/>
<rect x="7.4" y="9.4" width="105.2" height="61.2" fill="none" stroke="rgba(110,76,40,.35)" stroke-width=".8"/>
<rect x="4" y="1.5" width="112" height="12" rx="6" fill="url(#wood)"/><rect x="0.6" y="0.6" width="7" height="14" rx="3" fill="url(#gold)"/><rect x="112.4" y="0.6" width="7" height="14" rx="3" fill="url(#gold)"/>
<rect x="4" y="66.5" width="112" height="12" rx="6" fill="url(#wood)"/><rect x="0.6" y="65.4" width="7" height="14" rx="3" fill="url(#gold)"/><rect x="112.4" y="65.4" width="7" height="14" rx="3" fill="url(#gold)"/>
<path d="M10 4 H110 M10 69 H110" stroke="rgba(255,255,255,.16)" stroke-width="1" stroke-linecap="round"/>
</svg>"""
    return page(body, w=w, h=h), (w, h)


def scroll_row():
    w, h = 120, 34
    body = f"""<svg width="{w}" height="{h}" viewBox="0 0 {w} {h}"><defs>
<linearGradient id="wood" x1="0" x2="1"><stop offset="0" stop-color="#3a2a22"/><stop offset=".5" stop-color="#7a5a3a"/><stop offset="1" stop-color="#2a1d18"/></linearGradient>
<linearGradient id="paper" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3b2e3d"/><stop offset=".5" stop-color="#2a2030"/><stop offset="1" stop-color="#201824"/></linearGradient></defs>
<rect x="14" y="4" width="{w - 28}" height="{h - 8}" fill="url(#paper)"/>
<path d="M14 4.5 H{w - 14} M14 {h - 4.5} H{w - 14}" stroke="rgba(236,228,222,.2)" stroke-width=".8"/>
<g><rect x="4" y="1.2" width="11" height="{h - 2.4}" rx="5.5" fill="url(#wood)"/><rect x="2.6" y="3" width="3.2" height="{h - 6}" rx="1.6" fill="#d9b25a"/><path d="M8 3 V{h - 3}" stroke="rgba(255,255,255,.18)" stroke-width="1"/></g>
<g><rect x="{w - 15}" y="1.2" width="11" height="{h - 2.4}" rx="5.5" fill="url(#wood)"/><rect x="{w - 5.8}" y="3" width="3.2" height="{h - 6}" rx="1.6" fill="#d9b25a"/><path d="M{w - 8} 3 V{h - 3}" stroke="rgba(255,255,255,.18)" stroke-width="1"/></g>
<path d="M12 {h / 2 - 3} H17 M12 {h / 2 + 3} H17" stroke="{RED}" stroke-width="1.6" stroke-linecap="round"/>
</svg>"""
    return page(body, w=w, h=h), (w, h)


def brush_line():
    w, h = 140, 10
    body = """<svg width="140" height="10" viewBox="0 0 140 10"><defs>
<filter id="rough" x="-5%" y="-40%" width="110%" height="180%"><feTurbulence type="fractalNoise" baseFrequency=".9 .35" numOctaves="2" seed="2" result="n"/><feDisplacementMap in="SourceGraphic" in2="n" scale="2.4"/></filter>
<linearGradient id="f" x1="0" x2="1"><stop offset="0" stop-color="#ff4757" stop-opacity="0"/><stop offset=".15" stop-color="#ff4757" stop-opacity=".95"/><stop offset=".8" stop-color="#d8334a" stop-opacity=".9"/><stop offset="1" stop-color="#d8334a" stop-opacity="0"/></linearGradient></defs>
<path d="M3 5.6 C30 3.2 60 4.6 92 4 C112 3.7 126 4.4 137 5 C125 5.8 108 6.1 92 6.4 C60 6.9 30 6.4 3 5.6Z" fill="url(#f)" filter="url(#rough)"/></svg>"""
    return page(body, w=w, h=h), (w, h)


FRIEND_CSS = """
.fr-lid{transform-box:fill-box;transform-origin:50% 0;transform:scaleY(0)}
.fr-oh{opacity:0}.fr-cheek{opacity:.4}
"""
POSES = {
    'neutral': '',
    'blink': '.fr-lid{transform:scaleY(1)!important}',
    'happy': '.fr-lid{transform-origin:50% 100%!important;transform:scaleY(.4)!important}.fr-cheek{opacity:.95!important}',
    'oh': '.fr-oh{opacity:1!important}.fr-smile{opacity:0!important}.fr-cheek{opacity:.7!important}',
}


def friend(kind, pose):
    w, h = 120, 74
    svg = read(f'{kind}.svg')
    body = f'<style>{FRIEND_CSS}{POSES[pose]}</style>{svg}'
    return page(body, w=w, h=h), (w, h)


def make_all():
    jobs = {
        'ludo_hub_bg': hub_bg,
        'ludo_panel': panel,
        'ludo_tile': tile,
        'ludo_sun': sun,
        'ludo_branch': branch,
        'ludo_mtn_card': mountain_card,
        'ludo_bud_lit': lambda: bud('lit'),
        'ludo_bud_dim': lambda: bud('dim'),
        'ludo_bud_glow': lambda: bud('glow'),
        'ludo_blossom_on': lambda: blossom_icon(False),
        'ludo_blossom_off': lambda: blossom_icon(True),
        'ludo_stamp': stamp,
        'ludo_btn_hub': hub_button,
        'ludo_btn_blossom_on': lambda: blossom_button(False),
        'ludo_btn_blossom_off': lambda: blossom_button(True),
        'ludo_btn_scroll': scroll_button,
        'ludo_ic_open': lambda: small_icon('open'),
        'ludo_ic_x': lambda: small_icon('x'),
        'ludo_ic_plus': lambda: small_icon('plus'),
        'ludo_ic_back': lambda: small_icon('back'),
        'ludo_check_on': lambda: check(True),
        'ludo_check_off': lambda: check(False),
        'ludo_pill_on': lambda: pill(True),
        'ludo_pill_off': lambda: pill(False),
        'ludo_scroll_row': scroll_row,
        'ludo_scroll_page': scroll_page,
        'ludo_bar_track': lambda: bar(False),
        'ludo_bar_fill': lambda: bar(True),
        'ludo_brush': brush_line,
    }
    for i in range(3):
        jobs[f'ludo_petal_{i}'] = (lambda i=i: petal(i))
    for kind in ('samurai', 'octopus'):
        for pose in POSES:
            jobs[f'ludo_{kind}_{pose}'] = (lambda k=kind, p=pose: friend(k, p))
    return jobs


def main():
    write_bud_data()
    want = set(sys.argv[1:])
    jobs = make_all()
    with sync_playwright() as p:
        b = p.chromium.launch()
        ctx = b.new_context(device_scale_factor=4, viewport={'width': 900, 'height': 700})
        pg = ctx.new_page()
        for name, fn in jobs.items():
            if want and name not in want:
                continue
            html, (w, h) = fn()
            pg.set_viewport_size({'width': max(w + 4, 50), 'height': max(h + 4, 50)})
            pg.set_content(html)
            pg.wait_for_timeout(60)
            pg.locator('#a').screenshot(path=os.path.join(OUT, name + '.png'), omit_background=True)
            print('wrote', name)
        b.close()


if __name__ == '__main__':
    main()
