# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""Draws panel/fold.svg, the FOLD faceplate. 100 units = 1 inch. Run from the repo root:
python3 panel/build_fold_svg.py"""
import math, random
from pathlib import Path

W, H = 200, 700                      # 2 in by 7 in (4U)
COLS, ROWS = (50, 150), (100, 200, 300, 400, 500, 600)   # 1 in hole grid
KNOB = (50, 200)
AC, DC = "ac", "dc"
JACKS = [("IN", 50, 400, AC, "in"), ("IN 2", 150, 400, DC, "in2"),
         ("VC", 50, 500, DC, "vc"), ("OUT", 150, 500, AC, "out")]
TICKS = [(".5", -135.0), ("1", -67.5), ("2", 0.0), ("4", 67.5), ("8", 135.0)]  # log travel, 0.5 to 8
DEFAULT_ANGLE = -67.5                # g = 1
INK = "#1d1b18"
rnd = random.Random(1973)
o = []
A = o.append

def stamp(text, x, y, size, anchor="middle", weight="700", spacing=1.2, ident=None):
    """Screen-printed legend: a faint offset ghost under the ink, and a small tilt per legend."""
    rot = rnd.uniform(-0.7, 0.7); dx = rnd.uniform(-0.6, 0.6); dy = rnd.uniform(-0.5, 0.5)
    gx, gy = rnd.choice((-0.7, 0.7)), rnd.choice((-0.5, 0.6))
    attr = (f'font-size="{size}" font-weight="{weight}" letter-spacing="{spacing}" text-anchor="{anchor}"')
    tag = f' id="{ident}"' if ident else ""
    A(f'<g{tag} transform="translate({x+dx:.2f} {y+dy:.2f}) rotate({rot:.2f})">'
      f'<text {attr} x="{gx}" y="{gy}" fill="{INK}" opacity=".1">{text}</text>'
      f'<text {attr} fill="{INK}" opacity="{rnd.uniform(.84,.95):.2f}">{text}</text></g>')

A(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*2}" height="{H*2}" '
  f'font-family="Futura,\'Futura PT\',\'Century Gothic\',\'Avenir Next\',\'Helvetica Neue\',Arial,sans-serif">')
A('''<defs>
<linearGradient id="alu" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#8d9094"/><stop offset=".25" stop-color="#d6d8da"/><stop offset=".6" stop-color="#b3b6b9"/><stop offset="1" stop-color="#7c7f83"/></linearGradient>
<radialGradient id="aluHole" cx=".38" cy=".32" r=".8"><stop offset="0" stop-color="#eceded"/><stop offset=".6" stop-color="#b4b7ba"/><stop offset="1" stop-color="#6f7276"/></radialGradient>
<linearGradient id="paper" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f4f3ee"/><stop offset=".5" stop-color="#e9e8e2"/><stop offset="1" stop-color="#d6d5cf"/></linearGradient>
<radialGradient id="plug" cx=".38" cy=".3" r=".85"><stop offset="0" stop-color="#fbfaf6"/><stop offset=".7" stop-color="#e2e1da"/><stop offset="1" stop-color="#b9b8b1"/></radialGradient>
<filter id="tooth" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency=".22" numOctaves="2" seed="7"/><feColorMatrix values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 -1.6 .9"/><feGaussianBlur stdDeviation=".5"/></filter>
<filter id="fibre" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency=".035 .5" numOctaves="2" seed="19"/><feColorMatrix values="0 0 0 0 1  0 0 0 0 .98  0 0 0 0 .9  0 0 0 -.9 .5"/></filter>
<filter id="ink" x="-5%" y="-5%" width="110%" height="110%"><feTurbulence type="fractalNoise" baseFrequency=".9" numOctaves="2" seed="3" result="n"/><feDisplacementMap in="SourceGraphic" in2="n" scale=".35"/><feGaussianBlur stdDeviation=".08"/></filter>
<filter id="soft" x="-40%" y="-40%" width="180%" height="180%"><feGaussianBlur stdDeviation="2.4"/></filter>
<filter id="soft1" x="-40%" y="-40%" width="180%" height="180%"><feGaussianBlur stdDeviation="1.1"/></filter>
<linearGradient id="streak" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="120" y2="0"><stop offset="0" stop-color="#fff" stop-opacity="0"/><stop offset=".35" stop-color="#fff" stop-opacity=".05"/><stop offset=".46" stop-color="#fff" stop-opacity=".5"/><stop offset=".54" stop-color="#fff" stop-opacity=".55"/><stop offset=".62" stop-color="#fff" stop-opacity=".1"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
<linearGradient id="mylarTint" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".07"/><stop offset=".5" stop-color="#fff" stop-opacity="0"/><stop offset="1" stop-color="#20242a" stop-opacity=".07"/></linearGradient>
<radialGradient id="nut_ac" cx=".36" cy=".3" r=".85"><stop offset="0" stop-color="#5a5a5e"/><stop offset=".55" stop-color="#232326"/><stop offset="1" stop-color="#0b0b0c"/></radialGradient>
<radialGradient id="nut_dc" cx=".36" cy=".3" r=".85"><stop offset="0" stop-color="#6aa2ee"/><stop offset=".55" stop-color="#1f56b4"/><stop offset="1" stop-color="#0d2c66"/></radialGradient>
<radialGradient id="sleeve" cx=".4" cy=".32" r=".8"><stop offset="0" stop-color="#f3ead0"/><stop offset=".6" stop-color="#b9a877"/><stop offset="1" stop-color="#6a5c38"/></radialGradient>
<radialGradient id="bore" cx=".55" cy=".6" r=".7"><stop offset="0" stop-color="#000"/><stop offset="1" stop-color="#17140f"/></radialGradient>
<linearGradient id="skirt" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#3d3d40"/><stop offset=".5" stop-color="#17171a"/><stop offset="1" stop-color="#050506"/></linearGradient>
<linearGradient id="cap" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#34343a"/><stop offset="1" stop-color="#0e0e10"/></linearGradient>
<radialGradient id="capShine" cx=".5" cy=".5" r=".5"><stop offset="0" stop-color="#fff" stop-opacity=".22"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>
<radialGradient id="screw" cx=".38" cy=".3" r=".8"><stop offset="0" stop-color="#f1f1f0"/><stop offset="1" stop-color="#77797c"/></radialGradient>
<clipPath id="sheet"><rect x="4" y="0" width="192" height="700"/></clipPath>
</defs>''')

# 1. Bare aluminium plate. It shows at the two side edges and at every drilled hole.
A(f'<g id="aluminum"><rect width="{W}" height="{H}" fill="url(#alu)"/>')
for i in range(60):
    y = rnd.uniform(0, H)
    A(f'<line x1="0" x2="{W}" y1="{y:.1f}" y2="{y:.1f}" stroke="#fff" stroke-opacity="{rnd.uniform(.05,.2):.2f}" stroke-width=".3"/>')
A('</g>')

# 2. Baked enamel coat over the plate. It stops short of the two side edges, where the metal shows.
A('<g id="enamel" clip-path="url(#sheet)">')
A('<rect x="4" y="0" width="192" height="700" fill="url(#paper)"/>')
A('<rect x="4" y="0" width="192" height="700" filter="url(#tooth)" opacity=".5"/>')   # orange peel
A('<rect x="4" y="0" width="192" height="2.2" fill="#fff" opacity=".7"/><rect x="4" y="696.5" width="192" height="3.5" fill="#000" opacity=".22"/>')
A('<rect x="4" y="0" width="1.6" height="700" fill="#fff" opacity=".75"/><rect x="194" y="0" width="2" height="700" fill="#000" opacity=".28"/>')
A('</g>')
used = {KNOB} | {(x, y) for _, x, y, _, _ in JACKS}
PLUGS = [(cx, cy) for cx in COLS for cy in ROWS if (cx, cy) not in used]

# 3. Screen print: black, with one red line.
A('<g id="print" filter="url(#ink)">')
A(f'<rect x="14" y="22" width="172" height="40" fill="{INK}"/>')
A('<text x="101.5" y="54" font-size="32" font-weight="700" letter-spacing="9" text-anchor="middle" fill="#efeee8">FOLD</text>')
# Knob scale ring.
kx, ky = KNOB
def pol(r, deg):
    a = math.radians(deg - 90)
    return kx + r * math.cos(a), ky + r * math.sin(a)
x0, y0 = pol(33, -135); x1, y1 = pol(33, 135)
A(f'<path d="M{x0:.2f},{y0:.2f} A33,33 0 1 1 {x1:.2f},{y1:.2f}" fill="none" stroke="{INK}" stroke-width="1.1"/>')
for i in range(17):
    deg = -135 + i * 270 / 16
    major = i % 4 == 0
    a, b = pol(33, deg), pol(38.5 if major else 35.5, deg)
    A(f'<line x1="{a[0]:.2f}" y1="{a[1]:.2f}" x2="{b[0]:.2f}" y2="{b[1]:.2f}" stroke="{INK}" stroke-width="{1.6 if major else .8}"/>')
# One line folded six times, taller at each fold: the six cells in series.
pts, x = [(20, 344)], 20
for k in range(6):
    amp = 4 + k * 2.4
    x += 12.5; pts.append((x, 344 - amp))
    x += 12.5; pts.append((x, 344 + amp))
pts.append((x + 10, 344))
d = "M" + " L".join(f"{px:.1f},{py:.1f}" for px, py in pts)
A(f'<path d="{d}" fill="none" stroke="#c23a26" stroke-width="2.6" stroke-linejoin="round" stroke-linecap="round"/>')
# Inputs: open rings joined at a sum mark. CV: open ring with a tail. Output: solid block.
for jx, jy in ((50, 400), (150, 400), (50, 500)):
    A(f'<circle cx="{jx}" cy="{jy}" r="22.5" fill="none" stroke="{INK}" stroke-width="1.3"/>')
A(f'<path d="M72.5,400 H127.5" stroke="{INK}" stroke-width="1.3" fill="none"/>')
A(f'<circle cx="100" cy="400" r="6" fill="#ecebe5" stroke="{INK}" stroke-width="1.3"/><path d="M96.2,400 H103.8 M100,396.2 V403.8" stroke="{INK}" stroke-width="1.3"/>')
A(f'<path d="M100,406 V452 H150 V464" stroke="{INK}" stroke-width="1.3" fill="none"/>')
A(f'<path d="M72.5,500 H112 M105,494.5 L113,500 L105,505.5" stroke="{INK}" stroke-width="1.3" fill="none"/>')
A(f'<rect x="116" y="464" width="68" height="68" rx="9" fill="{INK}"/>')
A(f'<circle cx="24" cy="651" r="4" fill="{INK}"/><circle cx="112" cy="651" r="4" fill="#1f56b4"/>')
A(f'<rect x="14" y="636" width="172" height="1.2" fill="{INK}"/>')
A('</g>')

# 4. Aluminium at the drilled holes: the paper and mylar are cut back around each part.
A('<g id="holes">')
for cx, cy in [KNOB] + [(x, y) for _, x, y, _, _ in JACKS]:
    r = 12.5 if (cx, cy) == KNOB else 15.6
    A(f'<circle cx="{cx}" cy="{cy+.8}" r="{r+.9}" fill="#000" opacity=".28"/><circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#aluHole)"/>'
      f'<circle cx="{cx}" cy="{cy}" r="{r-.4}" fill="none" stroke="#fff" stroke-opacity=".5" stroke-width=".5"/>')
for cx, cy in PLUGS:   # unused grid holes: flush enamel button plugs
    A(f'<circle cx="{cx+.8}" cy="{cy+1.6}" r="13.4" fill="#000" opacity=".3" filter="url(#soft1)"/><circle cx="{cx}" cy="{cy}" r="12.6" fill="url(#plug)" stroke="#8d8c86" stroke-width=".5"/>'
      f'<path d="M{cx-9},{cy-5} A10.5,10.5 0 0 1 {cx+4},{cy-9.6}" fill="none" stroke="#fff" stroke-opacity=".9" stroke-width="1.2" stroke-linecap="round"/>')
for sx, sy in ((100, 11), (100, 689)):
    A(f'<circle cx="{sx}" cy="{sy+.7}" r="5.6" fill="#000" opacity=".3"/><circle cx="{sx}" cy="{sy}" r="5" fill="url(#screw)"/>'
      f'<line x1="{sx-3.2}" y1="{sy+1.6}" x2="{sx+3.2}" y2="{sy-1.6}" stroke="#2b2c2e" stroke-width="1.1"/>')
A('</g>')

# 5. Gloss on the enamel: a faint tint and one specular streak. Move #gloss-streak with the panel light.
A('<g id="gloss" clip-path="url(#sheet)" style="mix-blend-mode:screen"><rect x="4" y="0" width="192" height="700" fill="url(#mylarTint)"/>'
  '<g id="gloss-streak" transform="translate(18 0) rotate(24 100 350)"><rect x="0" y="-200" width="120" height="1100" fill="url(#streak)"/>'
  '<rect x="150" y="-200" width="46" height="1100" fill="url(#streak)" opacity=".35"/></g></g>')

# 6. Banana jacks: colored nut, plated sleeve, dark bore. Black is AC, blue is DC.
A('<g id="jacks">')
for label, cx, cy, kind, ident in JACKS:
    hexp = " ".join(f"{cx + 13.6*math.cos(math.radians(30+60*i)):.2f},{cy + 13.6*math.sin(math.radians(30+60*i)):.2f}" for i in range(6))
    A(f'<g id="jack-{ident}" data-kind="{kind}" data-live="{"1" if ident in ("in","out") else "0"}">'
      f'<ellipse cx="{cx+1.6}" cy="{cy+3}" rx="14.5" ry="14" fill="#000" opacity=".4" filter="url(#soft1)"/>'
      f'<polygon points="{hexp}" fill="url(#nut_{kind})" stroke="#050506" stroke-width=".7" stroke-linejoin="round"/>'
      f'<circle cx="{cx}" cy="{cy}" r="9.6" fill="url(#nut_{kind})" stroke="#000" stroke-opacity=".55" stroke-width=".6"/>'
      f'<path d="M{cx-8},{cy-4.5} A9.2,9.2 0 0 1 {cx+3},{cy-8.6}" fill="none" stroke="#fff" stroke-opacity=".45" stroke-width="1.1" stroke-linecap="round"/>'
      f'<circle cx="{cx}" cy="{cy}" r="6.4" fill="url(#sleeve)"/><circle cx="{cx}" cy="{cy}" r="4.3" fill="url(#bore)"/>'
      f'<path d="M{cx-3.6},{cy+2.2} A4.3,4.3 0 0 0 {cx+3.6},{cy+2.2}" fill="none" stroke="#cdbb86" stroke-opacity=".5" stroke-width=".6"/></g>')
A('</g>')

# 7. Skirted knob. #knob-shadow and #knob-cap take the same rotation, so the shadow turns with the pointer.
turn = f'rotate({DEFAULT_ANGLE} {kx} {ky})'
A(f'<g id="knob" data-min="0.5" data-max="8" data-default="1" data-min-angle="-135" data-max-angle="135" data-taper="log">')
A(f'<ellipse cx="{kx+2.5}" cy="{ky+5}" rx="29" ry="28" fill="#000" opacity=".5" filter="url(#soft)"/>')
A(f'<g id="knob-shadow" transform="{turn}"><rect x="{kx-1.2}" y="{ky-31}" width="5.4" height="24" rx="2" fill="#000" opacity=".5" filter="url(#soft1)" transform="translate(2.2 2.6)"/></g>')
A(f'<circle cx="{kx}" cy="{ky}" r="27" fill="url(#skirt)" stroke="#000" stroke-width=".8"/>')
A(f'<circle cx="{kx}" cy="{ky}" r="25.6" fill="none" stroke="#fff" stroke-opacity=".12" stroke-width=".7"/>')
A(f'<circle cx="{kx+.8}" cy="{ky+1.6}" r="18" fill="#000" opacity=".55" filter="url(#soft1)"/>')
A(f'<g id="knob-cap" transform="{turn}"><circle cx="{kx}" cy="{ky}" r="17" fill="url(#cap)" stroke="#000" stroke-width=".8"/>')
for i in range(36):
    a = math.radians(i * 10)
    A(f'<line x1="{kx+15.6*math.cos(a):.2f}" y1="{ky+15.6*math.sin(a):.2f}" x2="{kx+17*math.cos(a):.2f}" y2="{ky+17*math.sin(a):.2f}" stroke="#000" stroke-opacity=".6" stroke-width=".8"/>')
A(f'<rect x="{kx-1.5}" y="{ky-26.2}" width="3" height="23.5" rx="1.2" fill="#f4f1e6"/>'
  f'<rect x="{kx+.6}" y="{ky-26.2}" width=".9" height="23.5" fill="#000" opacity=".22"/></g>')
A(f'<ellipse cx="{kx-5}" cy="{ky-7}" rx="9" ry="5.5" fill="url(#capShine)" transform="rotate(-35 {kx-5} {ky-7})"/></g>')

# 8. One rope cable, IN to OUT, hanging below both jacks.
rope = "M50,400 C56,450 88,470 92,540 C96,606 152,612 150,500"
A('<g id="cable" data-from="in" data-to="out">')
A(f'<path d="{rope}" transform="translate(3 6)" fill="none" stroke="#000" stroke-opacity=".3" stroke-width="7" stroke-linecap="round" filter="url(#soft)"/>')
A(f'<path d="{rope}" fill="none" stroke="#6e100c" stroke-width="7" stroke-linecap="round"/>')
A(f'<path d="{rope}" fill="none" stroke="#d23a30" stroke-width="5.2" stroke-linecap="round"/>')
A(f'<path d="{rope}" fill="none" stroke="#6e100c" stroke-opacity=".55" stroke-width="5.2" stroke-dasharray="1.2 3.4"/>')
A(f'<path d="{rope}" transform="translate(-1 -.8)" fill="none" stroke="#ff9a8a" stroke-opacity=".7" stroke-width="1.3" stroke-dasharray="2.6 2" stroke-linecap="round"/>')
for px, py in ((50, 400), (150, 500)):
    A(f'<ellipse cx="{px+2}" cy="{py+3.5}" rx="11.5" ry="11" fill="#000" opacity=".45" filter="url(#soft1)"/>'
      f'<circle cx="{px}" cy="{py}" r="10.4" fill="#d23a30" stroke="#6e100c" stroke-width="1"/>'
      f'<path d="M{px-7.6},{py-3.6} A8.4,8.4 0 0 1 {px+2.6},{py-8}" fill="none" stroke="#ff9a8a" stroke-width="1.3" stroke-linecap="round"/>'
      f'<circle cx="{px}" cy="{py}" r="4.6" fill="#3a0806"/><circle cx="{px}" cy="{py}" r="3" fill="#0a0202"/>')
A('</g>')

# 9. Legends: screen-printed, drawn over the cable.
A('<g id="legends" filter="url(#ink)">')
for text, deg in TICKS:
    x, y = pol(45, deg)
    stamp(text, x, y + 2.8, 8, spacing=0)
stamp("AMOUNT", 50, 259, 9.5, spacing=1.8, ident="legend-knob")
for label, cx, cy, kind, ident in JACKS:
    if ident == "out":
        A(f'<text id="legend-out" x="150.6" y="548" font-size="11" font-weight="700" letter-spacing="2" text-anchor="middle" fill="{INK}">OUT</text>')
    else:
        stamp(label, cx, cy - 28, 11, spacing=1.6, ident=f"legend-{ident}")
stamp("AC", 32, 654.5, 9, anchor="start", spacing=1)
stamp("DC", 120, 654.5, 9, anchor="start", spacing=1)
stamp("SIX CELLS IN SERIES", 100, 323, 6.6, spacing=1.4, weight="400")
A('</g></svg>')

out = Path(__file__).with_name("fold.svg")
out.write_text("\n".join(o) + "\n")
print(out, out.stat().st_size)
