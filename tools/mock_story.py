"""Story-style Wrapped: one idea per screen, tap to continue. Contact sheet of the frames.
python3 tools/mock_story.py out.png"""
import os, sys
from playwright.sync_api import sync_playwright
R = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'resources')
out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/story.png'
def u(n): return f'file://{R}/ludo_{n}.png'
W, H = 440, 290
def petals(seed):
    import random; r = random.Random(seed); o = ''
    for i in range(9):
        o += f'<img src="{u(f"petal_{i%3}")}" style="position:absolute;left:{r.randint(10,400)}px;top:{r.randint(10,250)}px;width:{r.randint(12,22)}px;opacity:{r.uniform(.25,.6):.2f};transform:rotate({r.randint(0,360)}deg)">'
    return o
def frame(i, bg, kicker, big, small, extra='', fr='samurai_happy', bgi=None):
    segs = ''.join(f'<div style="flex:1;height:3px;border-radius:2px;background:rgba(255,255,255,{.9 if k<=i else .22})"></div>' for k in range(6))
    return f'''<div style="position:relative;width:{W}px;height:{H}px;border-radius:14px;overflow:hidden;color:#fff"><img src="{u(f"wrap_bg_{i if bgi is None else bgi}")}" style="position:absolute;left:0;top:0;width:{W}px;height:{H}px">
{petals(i)}
<div style="position:absolute;left:18px;right:18px;top:14px;display:flex;gap:4px">{segs}</div>
<div style="position:absolute;left:30px;top:56px;font:600 11px sans-serif;letter-spacing:1.5px;text-transform:uppercase;opacity:.85">{kicker}</div>
<div style="position:absolute;left:30px;top:78px;right:30px;font:800 {big[1]}px/1.02 Fr,serif;text-shadow:0 2px 14px rgba(0,0,0,.35)">{big[0]}</div>
<div style="position:absolute;left:30px;bottom:34px;right:130px;font:italic 400 13px Fr,serif;opacity:.92">{small}</div>
{extra}
<img src="{u(fr)}" style="position:absolute;right:22px;bottom:18px;width:64px">
<div style="position:absolute;right:20px;top:26px;font:600 9px sans-serif;opacity:.6">tap to continue</div></div>'''
mtn = ''.join(f'<div style="width:14px;height:{h*2.2:.0f}px;background:linear-gradient(180deg,#ff6676,#8e1428);border-radius:3px 3px 0 0"></div>' for h in [1,2,4,3,5,9,6,4,14,22,12,6,3,4,8,5,2,3,1,1])
frames = [
 frame(0, 'linear-gradient(135deg,#7a1230,#251020 70%)', 'Ludo Wrapped', ('Your<br>October', 46), 'Let&rsquo;s look back.', '', 'samurai_neutral'),
 frame(1, 'linear-gradient(135deg,#1f3a63,#14101f 75%)', 'You played for', ('14h 20m', 58), 'That&rsquo;s 43 full runs of Bloodbath&rsquo;s length.<br>(Sample comparison.)'),
 frame(2, 'linear-gradient(135deg,#c23a55,#2a1020 80%)', 'Your top level', ('Bloodbath', 52), '5h 02m this month. You finally beat it.', f'<img src="{u("stamp")}" style="position:absolute;right:40px;top:84px;width:96px;transform:rotate(-8deg)">'),
 frame(3, 'linear-gradient(135deg,#3a1a4a,#120d1c 80%)', 'Where it hurt', ('40&ndash;45%', 58), '212 deaths in this one stretch.', f'<div style="position:absolute;left:30px;right:30px;bottom:70px;display:flex;align-items:flex-end;gap:2px;height:50px;opacity:.9">{mtn}</div>', 'samurai_oh'),
 frame(4, '', 'Your month, in a word', ('Night Owl', 52), '41% of your play happened after 10pm.', f'<img src="{u("title_owl")}" style="position:absolute;right:40px;top:78px;width:104px"><img src="{u("spark")}" style="position:absolute;right:22px;top:72px;width:18px"><img src="{u("spark")}" style="position:absolute;right:34px;top:176px;width:13px">', 'samurai_happy'),
 frame(5, 'linear-gradient(135deg,#7a1230,#251020 70%)', 'That&rsquo;s a wrap', ('1,086<br>deaths.', 46), 'Come back stronger. See the full recap &rarr;', '', 'samurai_happy'),
]
html = f'<html><head><style>@font-face{{font-family:Fr;src:url(file://{R}/Fraunces.ttf);font-weight:100 900}}body{{margin:0;background:#0c0910;padding:16px;display:grid;grid-template-columns:{W}px {W}px;gap:16px}}</style></head><body>{"".join(frames)}</body></html>'
with sync_playwright() as p:
    b = p.chromium.launch(); pg = b.new_page(viewport={'width': 2 * W + 48, 'height': 3 * H + 80}, device_scale_factor=1.5)
    tmp = out + '.html'; open(tmp, 'w').write(html); pg.goto('file://' + tmp); pg.wait_for_timeout(500); os.remove(tmp)
    pg.screenshot(path=out); b.close()
