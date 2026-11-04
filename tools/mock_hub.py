"""Rough browser mock of the hub layout, using the real images at the coordinates Hub.cpp uses.
Only for checking proportions without the game:  python3 tools/mock_hub.py [out.png] [overview|levels|collections]"""
import os, sys, math
from playwright.sync_api import sync_playwright
R = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'resources')
out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/mock.png'
tab = sys.argv[2] if len(sys.argv) > 2 else 'overview'
H = 290

def img(name, x, y, w=None, h=None, anchor=(.5, .5), extra=''):
    # x,y are GD coordinates (origin bottom-left, y up) of the anchor point
    from PIL import Image
    im = Image.open(f'{R}/ludo_{name}.png'); iw, ih = im.width / 4, im.height / 4
    w = w or iw; h = h or ih
    left = x - w * anchor[0]; top = H - y - h * (1 - anchor[1])
    return f'<img src="file://{R}/ludo_{name}.png" style="position:absolute;left:{left}px;top:{top}px;width:{w}px;height:{h}px;{extra}">'

def text(s, x, y, size, color='#ece4de', anchor='l', font='Fraunces', weight=700, opacity=1):
    ta = {'l': 'left', 'c': 'center', 'r': 'right'}[anchor]
    tx = {'l': '0', 'c': '-50%', 'r': '-100%'}[anchor]
    return f'<div style="position:absolute;left:{x}px;top:{H - y}px;transform:translate({tx},-50%);font:{weight} {size}px {font},serif;color:{color};white-space:nowrap;opacity:{opacity}">{s}</div>'

parts = [img('hub_bg', 230, 145)]
parts.append(img('sun', 56, H - 29, w=80, h=80, extra='opacity:.75'))
import re
buds = [tuple(map(float, m)) for m in re.findall(r'\{([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)\.f\}', open(os.path.join(R, '..', 'src', 'BranchData.hpp')).read())]
BW, BH = 190, 118
K = .75
parts.append(img('branch', 456, H - 4, w=190 * K, h=118 * K, anchor=(1, 1), extra='transform:scaleX(-1);transform-origin:center'))
vis = [b for b in buds if b[1] < 68]
lit = round(float(os.environ.get('PROGRESS', '0.5')) * len(vis))
for n, (bx, by, bs, br) in enumerate(vis):
    fade = min(max(1 - (by - 38) / 30, 0), 1)
    gx, gy = 456 - bx * K, H - 4 - by * K
    k = bs / 20 * K
    on = n < lit
    def put(name, size, extra):
        return f'<img src="file://{R}/ludo_{name}.png" style="position:absolute;left:{gx - size / 2}px;top:{H - gy - size / 2 - 0}px;width:{size}px;height:{size}px;{extra}">'
    if on:
        parts.append(put('bud_glow', 44 * k * 1.9, f'opacity:{.6 * fade}'))
    parts.append(put('bud_lit' if on else 'bud_dim', 44 * k, f'opacity:{(1 if on else .8) * fade};transform:rotate({-br}deg)'))
parts.append(img('ic_x', 14, H - 14))
parts.append(text('Ludo', 150, H - 22, 26, anchor='c'))
parts.append(img('brush', 150, H - 37, w=126, h=8.1))
x = 82
for name, label in (('Overview', 'overview'), ('Levels', 'levels'), ('Collections', 'collections')):
    on = tab == label
    parts.append(img('pill_on' if on else 'pill_off', x, H - 62, w=92, h=26))
    parts.append(text(name, x, H - 62, 11, '#fff' if on else '#ece4de', 'c', 'sans-serif', 700))
    x += 96
ax, ay, aw, ah = 20, 46, 420, 162
def cx(v): return ax + v
def cy(v): return ay + v
if tab == 'overview':
    tw = aw / 5
    vals = [('4h 12m', 'played'), ('1,284', 'attempts'), ('1,201', 'deaths'), ('9', 'completions'), ('14', 'levels')]
    for i, (v, c) in enumerate(vals):
        c_x = cx(tw * (i + .5))
        parts.append(img('tile', c_x, cy(ah - 28), w=tw - 8, h=52, extra='opacity:.92'))
        parts.append(text(v, c_x, cy(ah - 22), 16, anchor='c'))
        parts.append(text(c, c_x, cy(ah - 42), 8.5, anchor='c', font='sans-serif', weight=400, opacity=.75))
    top = ah - 72
    parts.append(text('Most played', cx(4), cy(top), 12, '#ff7882'))
    for k, (n, t) in enumerate([('Bloodbath', '2h 05m'), ('Sakupen Circles', '58m'), ('Acu', '41m'), ('Cataclysm', '22m')]):
        yy = cy(top - 22 - 20 * k)
        parts.append(text(f'{k+1}.  {n}', cx(4), yy, 10, font='sans-serif', weight=700))
        parts.append(text(t, cx(196), yy, 9, anchor='r', font='sans-serif', weight=400, opacity=.8))
    mx, mw = 220, aw - 220 - 4
    parts.append(text('Death mountain', cx(mx), cy(top), 12, '#ff7882'))
    parts.append(text('Bloodbath', cx(mx), cy(top - 13), 8.3, font='sans-serif', weight=400, opacity=.8))
    parts.append(text('most deaths at 40-45%', cx(mx + mw), cy(top - 13), 8.3, '#ff969c', 'r', 'sans-serif', 400))
    parts.append(img('mtn_card', cx(mx + mw / 2), cy(37), w=196, h=70))
    hist = [1, 2, 4, 3, 5, 9, 6, 4, 14, 22, 12, 6, 3, 4, 8, 5, 2, 3, 1, 1]
    N = 80; base = 14; maxh = 40; pk = max(hist)
    hs = []
    for i in range(N + 1):
        pos = min(max(i / N * 20 - .5, 0), 19); a_ = int(pos); b_ = min(a_ + 1, 19); f = pos - a_; f = f * f * (3 - 2 * f)
        hs.append(base + 3 + (hist[a_] * (1 - f) + hist[b_] * f) / pk * maxh)
    far = [base + 3 + (hs[min(N, i + 14)] - base - 3) * .55 for i in range(N + 1)]
    def poly(arr): return ' '.join(f'{1 + 194 * i / N:.1f},{70 - v:.1f}' for i, v in enumerate(arr))
    top_i = max(range(N + 1), key=lambda i: hs[i]); fx = 1 + 194 * top_i / N; fy = 70 - hs[top_i]
    parts.append(f'<svg style="position:absolute;left:{cx(mx)}px;top:{H - cy(2) - 70}px" width="196" height="70">'
                 f'<polygon points="1,{70 - base + 1} {poly(far)} 195,{70 - base + 1}" fill="#2b121c" stroke="rgba(158,64,84,.75)" stroke-width=".5"/>'
                 f'<polygon points="1,{70 - base + 1} {poly(hs)} 195,{70 - base + 1}" fill="#0b080d" stroke="#ff4757" stroke-width=".8"/>'
                 f'<line x1="{fx}" y1="{fy}" x2="{fx}" y2="{fy - 9}" stroke="#ffe5e0" stroke-width=".5"/>'
                 f'<polygon points="{fx},{fy - 9} {fx + 7},{fy - 6.5} {fx},{fy - 4}" fill="#ff4757"/></svg>')
    parts.append(text('0%', cx(mx + 6), cy(9), 6.5, font='sans-serif', weight=400, opacity=.6))
    parts.append(text('50%', cx(mx + mw / 2), cy(9), 6.5, anchor='c', font='sans-serif', weight=400, opacity=.6))
    parts.append(text('100%', cx(mx + mw - 6), cy(9), 6.5, anchor='r', font='sans-serif', weight=400, opacity=.6))
elif tab == 'levels':
    parts.append(img('check_off', cx(14), cy(ah - 12), w=18.7, h=18.7))
    parts.append(text('Favorites only', cx(30), cy(ah - 12), 9.5, font='sans-serif', weight=400))
    rows = [('Bloodbath', '2h 05m   412 tries   410 deaths', 62, True, True), ('Sakupen Circles', '58m   301 tries   300 deaths', 35, False, False), ('Acu', '41m   180 tries   179 deaths', 100, True, True)]
    for k, (n, t, best, fav, done) in enumerate(rows):
        y0 = ah - 30 - 38 * k - 17 + 17
        yc = cy(ah - 30 - 17 - 38 * k + 4)
        W = aw
        parts.append(img('tile', cx(W / 2), yc, w=W, h=34, extra='opacity:.88'))
        parts.append(text(n, cx(10), yc + 6, 11, font='sans-serif', weight=700))
        parts.append(text(t, cx(10), yc - 8, 8.3, font='sans-serif', weight=400, opacity=.75))
        bx = W - 184
        parts.append(img('bar_track', cx(bx + 26), yc - 8, w=52, h=6))
        parts.append(img('bar_fill', cx(bx + max(8, 52 * best / 100) / 2), yc - 8, w=max(8, 52 * best / 100), h=6))
        parts.append(text(f'{best}%', cx(bx + 55), yc - 8, 6.6, font='sans-serif', weight=400, opacity=.7))
        if done: parts.append(img('stamp', cx(W - 86), yc, w=22, h=22, extra='transform:rotate(-6deg)'))
        parts.append(img('blossom_on' if fav else 'blossom_off', cx(W - 54), yc, w=23.8, h=23.8))
        parts.append(img('ic_open', cx(W - 20), yc, w=20.8, h=20.8))
else:
    parts.append(img('pill_on', cx(aw - 66), cy(ah - 12), w=132, h=24))
    parts.append(text('New collection', cx(aw - 66), cy(ah - 12), 10, '#fff', 'c', 'sans-serif', 700))
    for k, (n, t) in enumerate([('Hardest', '3 levels   2h 10m played'), ('To beat later', '5 levels   40m played')]):
        yc = cy(ah - 30 - 17 - 38 * k + 4)
        parts.append(img('scroll_row', cx(aw / 2), yc, w=aw, h=34))
        parts.append(text(n, cx(34), yc + 5, 12.5, anchor='l'))
        parts.append(text(t, cx(34), yc - 8, 8.3, font='sans-serif', weight=400, opacity=.75))
        parts.append(img('pill_on', cx(aw - 82), yc, w=62, h=24))
        parts.append(text('Open', cx(aw - 82), yc, 9, '#fff', 'c', 'sans-serif', 700))
        parts.append(img('ic_x', cx(aw - 36), yc, w=20.8, h=20.8))
parts.append(text('Ludo v0.2.0', 24, 20, 8, font='sans-serif', weight=400, opacity=.45))
fr = os.environ.get('FRIEND', 'samurai')
parts.append(img(f'{fr}_neutral', 460 - 58, 3 + 74 * .5 / 2, w=60, h=37))
html = f"""<!doctype html><html><head><meta charset=utf-8><style>
@font-face{{font-family:Fraunces;src:url(file://{R}/Fraunces.ttf);font-weight:700}}
html,body{{margin:0;background:#3a3a44}}#h{{position:relative;width:460px;height:{H}px;overflow:hidden;margin:20px}}</style></head>
<body><div id=h>{''.join(parts)}</div></body></html>"""
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': 500, 'height': H + 40}, device_scale_factor=3)
    open('/tmp/_mock_hub.html', 'w').write(html); pg.goto('file:///tmp/_mock_hub.html'); pg.wait_for_timeout(600)
    pg.locator('#h').screenshot(path=out); b.close()
print(out)
