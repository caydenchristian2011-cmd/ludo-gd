"""Browser mock of the Wrapped recap (month / year), using the real images.
python3 tools/mock_wrapped.py month out.png   |   python3 tools/mock_wrapped.py year out.png"""
import os, sys, base64, random
from playwright.sync_api import sync_playwright
R = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'resources')
mode = sys.argv[1] if len(sys.argv) > 1 else 'month'
out = sys.argv[2] if len(sys.argv) > 2 else f'/tmp/wrapped_{mode}.png'
W, H = 440, 290
def u(n): return f'file://{R}/ludo_{n}.png'
INK = '#2b1b20'
if mode == 'month':
    title, sub = 'October', 'your Ludo month'
    stats = [('14h 20m', 'played'), ('1,086', 'deaths'), ('4', 'levels beaten'), ('9', 'levels played')]
    top = ('Bloodbath', '5h 02m', True)
    spot = ('Bloodbath', '40-45%', '212 deaths')
    quip = 'You beat 4 levels this month.'
    random.seed(4)
    vals = [random.choice([0, 0, 1, 2, 3, 4]) for _ in range(31)]
    strip_label = 'days you played'
else:
    title, sub = '2026', 'your Ludo year'
    stats = [('161h', 'played'), ('12,904', 'deaths'), ('23', 'levels beaten'), ('58', 'levels played')]
    top = ('Acropolis', '19h 11m', True)
    spot = ('Sakupen Circles', '60-65%', '1,340 deaths')
    quip = 'Busiest month: October.'
    vals = [3, 2, 4, 5, 3, 2, 6, 9, 7, 14, 8, 5]
    strip_label = 'hours per month'
bars = ''
if mode == 'month':
    for i, v in enumerate(vals):
        a = 0.12 + v * 0.2 if v else 0.08
        bars += f'<div style="width:10px;height:20px;border-radius:3px;background:rgba(199,31,53,{min(a,1):.2f})"></div>'
else:
    m = max(vals); names = 'JFMAMJJASOND'
    for i, v in enumerate(vals):
        h = 4 + v / m * 26
        bars += f'<div style="width:22px;display:flex;flex-direction:column;align-items:center;justify-content:flex-end;height:42px;gap:2px"><div style="width:14px;height:{h:.0f}px;border-radius:3px;background:linear-gradient(180deg,#e0324a,#a31a2e)"></div><div style="font:600 7px sans-serif;opacity:.7">{names[i]}</div></div>'
def pill(label, on, x):
    return f'<div style="position:absolute;left:{x}px;top:20px;width:52px;height:22px;background:url({u("pill_on" if on else "pill_off")}) center/100% 100%;font:700 9.5px sans-serif;color:{"#fff" if on else "#ece4de"};display:flex;align-items:center;justify-content:center">{label}</div>'
html = f'''<html><head><style>@font-face{{font-family:Fr;src:url(file://{R}/Fraunces.ttf);font-weight:100 900}}
body{{margin:0;background:#141018;padding:20px}} .n{{font:700 21px Fr,serif;color:{INK}}} .l{{font:400 9.5px sans-serif;color:{INK};opacity:.75;margin-top:1px}}</style></head><body>
<div style="position:relative;width:{W}px;height:{H}px">
<div style="position:absolute;inset:0;border-style:solid;border-width:16px;border-image:url({u("scroll_page")}) 64 fill / 16px / 0 stretch"></div>
<div style="position:absolute;left:34px;top:25px;font:700 25px Fr,serif;color:{INK}">{title}</div>
<div style="position:absolute;left:36px;top:55px;font:italic 400 10px Fr,serif;color:{INK};opacity:.8">{sub}</div>
<img src="{u("brush")}" style="position:absolute;left:30px;top:68px;width:112px">
{pill("Month", mode=="month", W-34-52-56)}{pill("Year", mode=="year", W-34-52)}
<div style="position:absolute;left:34px;top:88px;display:grid;grid-template-columns:112px 112px;row-gap:12px">
{''.join(f'<div><div class="n">{v}</div><div class="l">{c}</div></div>' for v,c in stats)}</div>
<div style="position:absolute;left:260px;top:84px;width:146px">
 <div class="l" style="margin:0 0 3px">top level</div>
 <div style="display:flex;align-items:center;gap:6px"><div><div style="font:700 13px Fr,serif;color:{INK}">{top[0]}</div><div class="l">{top[1]} played</div></div>
 <img src="{u("stamp")}" style="width:26px;transform:rotate(-6deg)"></div>
 <div class="l" style="margin:11px 0 3px">hardest spot</div>
 <div style="font:700 13px Fr,serif;color:{INK}">{spot[0]}</div>
 <div class="l">deaths peak at {spot[1]}</div>
</div>
<div style="position:absolute;left:34px;right:34px;top:{188}px;height:1px;background:rgba(43,27,32,.25)"></div>
<div class="l" style="position:absolute;left:34px;top:194px">{strip_label}</div>
<div style="position:absolute;left:34px;top:{210 if mode=="month" else 204}px;width:370px;display:flex;flex-wrap:wrap;gap:{2 if mode=="month" else 6}px">{bars}</div>
<div style="position:absolute;left:34px;top:248px;font:italic 400 10px Fr,serif;color:{INK};opacity:.85">{quip}</div>
<img src="{u("samurai_happy")}" style="position:absolute;right:30px;top:226px;width:34px">
</div></body></html>'''
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': W + 40, 'height': H + 40}, device_scale_factor=2)
    tmp = out + '.html'; open(tmp, 'w').write(html); pg.goto('file://' + tmp); pg.wait_for_timeout(500); os.remove(tmp); pg.screenshot(path=out); b.close()
