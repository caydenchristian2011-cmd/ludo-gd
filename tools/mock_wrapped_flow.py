"""Mock of the Wrapped pop-up and the Wrapped tab, using the real images.
python3 tools/mock_wrapped_flow.py popup out.png | tab out.png"""
import os, sys
from playwright.sync_api import sync_playwright
R = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'resources')
mode = sys.argv[1]; out = sys.argv[2]
def u(n): return f'file://{R}/ludo_{n}.png'
W, H = 460, 290
INK = '#2b1b20'
def pill(label, on, x, y, w=92, h=26):
    return f'<div style="position:absolute;left:{x}px;top:{y}px;width:{w}px;height:{h}px;background:url({u("pill_on" if on else "pill_off")}) center/100% 100%;font:700 11px sans-serif;color:{"#fff" if on else "#ece4de"};display:flex;align-items:center;justify-content:center">{label}</div>'
hub = f'''<img src="{u("hub_bg")}" style="position:absolute;left:0;top:0;width:{W}px;height:{H}px">
<img src="{u("sun")}" style="position:absolute;left:16px;top:-25px;width:80px;opacity:.75">
<img src="{u("ic_x")}" style="position:absolute;left:4px;top:4px;width:20px;opacity:0">
<div style="position:absolute;left:150px;top:{22-14}px;transform:translateX(-50%);font:700 26px Fr,serif;color:#ece4de">Ludo</div>
<img src="{u("brush")}" style="position:absolute;left:87px;top:33px;width:126px">'''
tabs = ''
for n, name in enumerate(['Overview', 'Levels', 'Collections', 'Wrapped']):
    tabs += pill(name, name == ('Wrapped' if mode == 'tab' else 'Overview'), 82 - 46 + 96 * n - 10, 228 - 13 + 0 - 0 if False else 290 - 228 - 13 + 0, 92, 26)
body = hub + tabs
if mode == 'popup':
    body += '<div style="position:absolute;inset:0;background:rgba(0,0,0,.55)"></div>'
    pw, ph = 300, 170; px, py = (W - pw) / 2, (H - ph) / 2
    body += f'''<div style="position:absolute;left:{px}px;top:{py}px;width:{pw}px;height:{ph}px;box-sizing:border-box;border-style:solid;border-width:16px;border-image:url({u("scroll_page")}) 64 fill / 16px / 0 stretch"></div>
<img src="{u("wrap_icon")}" style="position:absolute;left:{W/2-22}px;top:{py+24}px;width:44px">
<div style="position:absolute;left:{W/2}px;top:{py+72}px;transform:translateX(-50%);font:700 19px Fr,serif;color:{INK};white-space:nowrap">Your October recap is ready</div>
<div style="position:absolute;left:{W/2}px;top:{py+98}px;transform:translateX(-50%);font:italic 11px Fr,serif;color:{INK};opacity:.8;white-space:nowrap">A look back at your month in Ludo.</div>
{pill("Watch", True, W/2-100, py+122, 92)}{pill("Not now", False, W/2+8, py+122, 92)}'''
else:
    # content area (20,46)-(440,208) in GD coords -> css top = 290-208 = 82
    def card(x, title, sub, badge, pri):
        return f'''<div style="position:absolute;left:{x}px;top:88px;width:196px;height:112px;background:url({u("tile")}) center/100% 100%;opacity:.95"></div>
<div style="position:absolute;left:{x+14}px;top:98px;font:700 22px Fr,serif;color:#ece4de">{title}</div>
<div style="position:absolute;left:{x+14}px;top:126px;font:italic 10px Fr,serif;color:#ece4de;opacity:.75">{sub}</div>
<img src="{u(badge)}" style="position:absolute;left:{x+196-14-48}px;top:98px;width:44px">
{pill("Watch story", pri, x+14, 160, 84, 24)}{pill("Scroll", False, x+104, 160, 60, 24)}'''
    body += card(24, 'October', 'ready to watch', 'title_owl', True) + card(240, '2026', 'comes in December', 'title_wonder', False)
    body += f'<div style="position:absolute;left:24px;top:210px;font:italic 10px Fr,serif;color:#ece4de;opacity:.7">Earlier months: September, August ...</div>'
html = f'''<html><head><style>@font-face{{font-family:Fr;src:url(file://{R}/Fraunces.ttf);font-weight:100 900}}body{{margin:0;background:#0c0910;padding:16px}}</style></head><body><div style="position:relative;width:{W}px;height:{H}px">{body}</div></body></html>'''
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': W + 32, 'height': H + 32}, device_scale_factor=2)
    tmp = out + '.html'; open(tmp, 'w').write(html); pg.goto('file://' + tmp); pg.wait_for_timeout(500); os.remove(tmp)
    pg.screenshot(path=out); b.close()
