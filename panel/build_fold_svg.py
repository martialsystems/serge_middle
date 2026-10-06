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
    """Stamped legend: a faint offset ghost under the ink, and a small tilt per legend."""
    rot = rnd.uniform(-1.4, 1.4); dx = rnd.uniform(-0.6, 0.6); dy = rnd.uniform(-0.5, 0.5)
    gx, gy = rnd.choice((-0.7, 0.7)), rnd.choice((-0.5, 0.6))
    attr = (f'font-size="{size}" font-weight="{weight}" letter-spacing="{spacing}" text-anchor="{anchor}"')
    tag = f' id="{ident}"' if ident else ""
    A(f'<g{tag} transform="translate({x+dx:.2f} {y+dy:.2f}) rotate({rot:.2f})">'
      f'<text {attr} x="{gx}" y="{gy}" fill="{INK}" opacity=".22">{text}</text>'
      f'<text {attr} fill="{INK}" opacity="{rnd.uniform(.84,.95):.2f}">{text}</text></g>')

A(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*2}" height="{H*2}" '
  f'font-family="\'Courier Prime\',\'Courier New\',Courier,monospace">')
A('''<defs>
<linearGradient id="alu" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#8d9094"/><stop offset=".25" stop-color="#d6d8da"/><stop offset=".6" stop-color="#b3b6b9"/><stop offset="1" stop-color="#7c7f83"/></linearGradient>
<radialGradient id="aluHole" cx=".38" cy=".32" r=".8"><stop offset="0" stop-color="#eceded"/><stop offset=".6" stop-color="#b4b7ba"/><stop offset="1" stop-color="#6f7276"/></radialGradient>
<linearGradient id="paper" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#efe9d8"/><stop offset=".55" stop-color="#e8e1cd"/><stop offset="1" stop-color="#ddd4bc"/></linearGradient>
<filter id="tooth" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="1.15" numOctaves="3" seed="7"/><feColorMatrix values="0 0 0 0 .32  0 0 0 0 .27  0 0 0 0 .18  0 0 0 -1.1 .62"/></filter>
<filter id="fibre" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency=".035 .5" numOctaves="2" seed="19"/><feColorMatrix values="0 0 0 0 1  0 0 0 0 .98  0 0 0 0 .9  0 0 0 -.9 .5"/></filter>
<filter id="ink" x="-5%" y="-5%" width="110%" height="110%"><feTurbulence type="fractalNoise" baseFrequency=".9" numOctaves="2" seed="3" result="n"/><feDisplacementMap in="SourceGraphic" in2="n" scale=".9"/><feGaussianBlur stdDeviation=".14"/></filter>
<filter id="soft" x="-40%" y="-40%" width="180%" height="180%"><feGaussianBlur stdDeviation="2.4"/></filter>
<filter id="soft1" x="-40%" y="-40%" width="180%" height="180%"><feGaussianBlur stdDeviation="1.1"/></filter>
<linearGradient id="streak" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="120" y2="0"><stop offset="0" stop-color="#fff" stop-opacity="0"/><stop offset=".35" stop-color="#fff" stop-opacity=".05"/><stop offset=".5" stop-color="#fff" stop-opacity=".3"/><stop offset=".58" stop-color="#fff" stop-opacity=".1"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
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

# 2. Paper sheet, folded over the top and bottom edges, short of the side edges.
A('<g id="paper" clip-path="url(#sheet)">')
A('<rect x="4" y="0" width="192" height="700" fill="url(#paper)"/>')
A('<rect x="4" y="0" width="192" height="700" filter="url(#fibre)" opacity=".5"/>')
A('<rect x="4" y="0" width="192" height="700" filter="url(#tooth)" opacity=".55"/>')
A('<rect x="4" y="0" width="192" height="5" fill="#000" opacity=".1"/><rect x="4" y="695" width="192" height="5" fill="#000" opacity=".16"/>')
A('<rect x="4" y="0" width="1.2" height="700" fill="#000" opacity=".25"/><rect x="194.8" y="0" width="1.2" height="700" fill="#000" opacity=".3"/>')
# Unused grid holes stay under the paper: a shallow dimple where the sheet spans the hole.
used = {KNOB} | {(x, y) for _, x, y, _, _ in JACKS}
for cx in COLS:
    for cy in ROWS:
        if (cx, cy) in used:
            continue
        A(f'<g class="covered-hole"><circle cx="{cx}" cy="{cy}" r="15.5" fill="#000" opacity=".045"/>'
          f'<path d="M{cx-15.5},{cy} A15.5,15.5 0 0 1 {cx+15.5},{cy}" fill="none" stroke="#000" stroke-opacity=".2" stroke-width=".9"/>'
          f'<path d="M{cx-15.5},{cy} A15.5,15.5 0 0 0 {cx+15.5},{cy}" fill="none" stroke="#fff" stroke-opacity=".55" stroke-width=".9"/></g>')
A('</g>')

# 3. Printed graphics, on the paper and under the mylar.
A('<g id="print" filter="url(#ink)">')
A(f'<rect x="14" y="20" width="172" height="44" fill="{INK}" opacity=".9"/>')
A('<g transform="translate(100.4 54.6) rotate(-.6)"><text font-size="38" font-weight="700" letter-spacing="7" text-anchor="middle" fill="#e9e2cf">FOLD</text></g>')
A(f'<line x1="14" y1="70" x2="186" y2="70.6" stroke="{INK}" stroke-width="1.2" opacity=".85"/>')
# One line folded six times, wider at each fold: the six cells in series.
pts, y = [(150, 88)], 88
for k in range(6):
    half = 9 + k * 4.2
    y += 17; pts.append((150 + half, y))
    y += 17; pts.append((150 - half, y))
y += 14; pts.append((150, y))
d = "M" + " L".join(f"{x:.1f},{yy:.1f}" for x, yy in pts)
A(f'<path d="{d}" fill="none" stroke="#b5432c" stroke-width="3.4" stroke-linejoin="miter" opacity=".9"/>')
A(f'<path d="{d}" transform="translate(.9 .7)" fill="none" stroke="{INK}" stroke-width=".8" opacity=".75"/>')
A(f'<path d="M150,{y} V354" stroke="{INK}" stroke-width="1" stroke-dasharray="1.5 3" fill="none" opacity=".8"/>')
# Knob scale: five ticks on the plate, the long arc between them.
kx, ky = KNOB
def pol(r, deg):
    a = math.radians(deg - 90)
    return kx + r * math.cos(a), ky + r * math.sin(a)
x0, y0 = pol(33, -135); x1, y1 = pol(33, 135)
A(f'<path d="M{x0:.2f},{y0:.2f} A33,33 0 1 1 {x1:.2f},{y1:.2f}" fill="none" stroke="{INK}" stroke-width=".9" opacity=".85"/>')
for i in range(17):
    deg = -135 + i * 270 / 16
    major = i % 4 == 0
    a, b = pol(33, deg), pol(38.5 if major else 35.5, deg)
    A(f'<line x1="{a[0]:.2f}" y1="{a[1]:.2f}" x2="{b[0]:.2f}" y2="{b[1]:.2f}" stroke="{INK}" stroke-width="{1.5 if major else .7}"/>')
# Signal brackets: the two inputs meet at a sum mark ahead of the cells, and OUT gets an arrow.
A(f'<path d="M72,400 H128" stroke="{INK}" stroke-width="1" fill="none" opacity=".8"/>')
A(f'<circle cx="100" cy="400" r="5.5" fill="#e8e1cd" stroke="{INK}" stroke-width="1"/><path d="M96.6,400 H103.4 M100,396.6 V403.4" stroke="{INK}" stroke-width="1"/>')
A(f'<path d="M100,394.5 V354 H150" stroke="{INK}" stroke-width="1" stroke-dasharray="1.5 3" fill="none" opacity=".8"/>')
A(f'<path d="M72,500 H96 M72,496 V504" stroke="{INK}" stroke-width="1" fill="none" opacity=".8"/>')
A(f'<path d="M104,500 H126 M120,495.5 L127,500 L120,504.5" stroke="{INK}" stroke-width="1.2" fill="none"/>')
A(f'<rect x="14" y="560" width="172" height="1.1" fill="{INK}" opacity=".8"/>')
A(f'<circle cx="24" cy="651" r="4" fill="{INK}"/><circle cx="112" cy="651" r="4" fill="#1f56b4"/>')
A('</g>')

# 4. Aluminium at the drilled holes: the paper and mylar are cut back around each part.
A('<g id="holes">')
for cx, cy in [KNOB] + [(x, y) for _, x, y, _, _ in JACKS]:
    r = 12.5 if (cx, cy) == KNOB else 17
    A(f'<circle cx="{cx}" cy="{cy+.8}" r="{r+.9}" fill="#000" opacity=".28"/><circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#aluHole)"/>'
      f'<circle cx="{cx}" cy="{cy}" r="{r-.4}" fill="none" stroke="#fff" stroke-opacity=".5" stroke-width=".5"/>')
for sx, sy in ((100, 11), (100, 689)):
    A(f'<circle cx="{sx}" cy="{sy+.7}" r="5.6" fill="#000" opacity=".3"/><circle cx="{sx}" cy="{sy}" r="5" fill="url(#screw)"/>'
      f'<line x1="{sx-3.2}" y1="{sy+1.6}" x2="{sx+3.2}" y2="{sy-1.6}" stroke="#2b2c2e" stroke-width="1.1"/>')
A('</g>')

# 5. Mylar film: a faint tint and one soft specular streak. Move #mylar-streak with the panel light.
A('<g id="mylar" clip-path="url(#sheet)" style="mix-blend-mode:screen"><rect x="4" y="0" width="192" height="700" fill="url(#mylarTint)"/>'
  '<g id="mylar-streak" transform="translate(18 0) rotate(24 100 350)"><rect x="0" y="-200" width="120" height="1100" fill="url(#streak)"/>'
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
rope = "M50,400 C54,452 86,468 91,536 C95,604 152,618 150,500"
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

# 9. Legends: stamped, over the cable.
A('<g id="legends" filter="url(#ink)">')
for text, deg in TICKS:
    x, y = pol(45.5, deg)
    stamp(text, x, y + 2.8, 8.6, spacing=0)
stamp("FOLD", 50, 258, 12.5, spacing=2.4, ident="legend-knob")
for label, cx, cy, kind, ident in JACKS:
    stamp(label, cx, cy - 23, 12.5, spacing=1.6, ident=f"legend-{ident}")
stamp("AC", 32, 654.5, 9, anchor="start", spacing=1)
stamp("DC", 120, 654.5, 9, anchor="start", spacing=1)
stamp("SIX CELLS IN SERIES", 100, 576, 7.4, spacing=1.1, weight="400")
A('</g></svg>')

out = Path(__file__).with_name("fold.svg")
out.write_text("\n".join(o) + "\n")
print(out, out.stat().st_size)
