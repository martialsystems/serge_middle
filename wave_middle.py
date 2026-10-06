"""Memoryless map of the Serge Wave Multipliers middle section.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Six identical cells. The acceptance curve is this map at g = 1 on
vin in [-6, 6]. Oversampling, the halfband, the fixed output gain,
and the DC block belong to the later iPlug2 module.
"""

from __future__ import annotations

import math
from pathlib import Path
from typing import Sequence

# Shockley parameters for one cell. VT is in volts, Is in amperes, R in ohms.
VT = 0.02585
IS = 2.52e-9
ETA = 1.68
R = 33000.0

N_CELLS = 6
G_MIN = 0.5
G_MAX = 8.0
G_DEFAULT = 1.0

VIN_MIN = -6.0
VIN_MAX = 6.0
N_SAMPLES = 6001

_ETA_VT = ETA * VT
_LOG_K = math.log((IS * R) / _ETA_VT)

_ROOT = Path(__file__).resolve().parent
_ACCEPTANCE_DIR = _ROOT / "tests"


def _w0_initial(log_z: float) -> float:
    """Seed for the principal branch. log_z is ln of the W argument."""
    if log_z < -2.0:
        z = math.exp(log_z)
        z2 = z * z
        return z - z2 + 1.5 * z2 * z - (8.0 / 3.0) * z2 * z2
    if log_z < 1.0:
        z = math.exp(log_z)
        return z / (1.0 + z)
    # Argument >= e. W(e) = 1, and log_z == 1 returns 1.
    l2 = math.log(log_z)
    return log_z - l2 + l2 / log_z


def lambert_w0_kexp(abs_v: float) -> float:
    """Principal Lambert W of (Is R)/(eta VT) * exp(|v|/(eta VT)).

    Halley iteration on w + ln(w) - ln(z) = 0. The product k * exp(|v|/(eta VT))
    is never formed, so a gain of 8 at 6 V stays inside float64.
    """
    if abs_v < 0.0 or not math.isfinite(abs_v):
        raise ValueError(f"abs_v must be finite and non-negative, got {abs_v}")
    log_z = abs_v / _ETA_VT + _LOG_K
    w = _w0_initial(log_z)
    if not math.isfinite(w) or w <= 0.0:
        w = math.exp(min(log_z, 0.0))
        if w <= 0.0:
            w = 1e-300
    scale = max(1.0, abs(log_z))
    for _ in range(8):
        log_w = math.log(w)
        f = w + log_w - log_z
        if abs(f) <= 1e-14 * scale:
            return w
        inv_w = 1.0 / w
        fp = 1.0 + inv_w
        # f'' = -1/w^2, so the Halley denominator is fp + f/(2 fp w^2).
        step = f / (fp + f * (inv_w * inv_w) / (2.0 * fp))
        w_next = w - step
        if not math.isfinite(w_next) or w_next <= 0.0:
            w_next = 0.5 * w
        if w_next == w:
            return w
        w = w_next
    f = w + math.log(w) - log_z
    if abs(f) > 1e-10 * scale:
        raise RuntimeError(
            f"Lambert W did not converge: log_z={log_z}, w={w}, residual={f}"
        )
    return w


def diode_node(v_in: float) -> float:
    """Op-amp positive-input node.

    v_plus = sign(v) * (|v| - eta VT W(z)), and v_plus(0) = 0.
    """
    if v_in == 0.0:
        return 0.0
    if not math.isfinite(v_in):
        raise ValueError(f"v_in must be finite, got {v_in}")
    mag = abs(v_in)
    signed = mag - _ETA_VT * lambert_w0_kexp(mag)
    if v_in < 0.0:
        return -signed
    return signed


def cell(v_in: float) -> float:
    """One middle-section stage.

    v_out = 2 v_plus - v_in = sign(v) * (|v| - 2 eta VT W(z)).
    The factor 2 is this resistor combination, not a second Lambert solve.
    v_out(0) = 0.
    """
    if v_in == 0.0:
        return 0.0
    return 2.0 * diode_node(v_in) - v_in


def process(x: float, g: float = G_DEFAULT, cells: int = N_CELLS) -> float:
    """y = C applied `cells` times to g*x. Default cells is 6, default g is 1."""
    if cells < 0:
        raise ValueError(f"cells must be non-negative, got {cells}")
    if not math.isfinite(g):
        raise ValueError(f"g must be finite, got {g}")
    y = g * x
    for _ in range(cells):
        y = cell(y)
    return y


def acceptance_vin() -> list[float]:
    """Odd grid on [-6, 6], 6,001 samples, endpoints included.

    The positive half is built first and the negative half is its negation,
    so vin[i] == -vin[n-1-i] exactly.
    """
    half = N_SAMPLES // 2
    positive = [VIN_MAX * i / half for i in range(half + 1)]
    negative = [-v for v in reversed(positive[1:])]
    return negative + positive


def acceptance_samples() -> list[tuple[float, float]]:
    return [(v, process(v, G_DEFAULT)) for v in acceptance_vin()]


def acceptance_csv(samples: Sequence[tuple[float, float]] | None = None) -> str:
    rows = acceptance_samples() if samples is None else samples
    lines = ["vin,vout"]
    lines.extend(f"{v!r},{y!r}" for v, y in rows)
    return "\n".join(lines) + "\n"


def _nice_limit(peak: float) -> float:
    padded = max(peak, 1e-6) * 1.12
    exponent = math.floor(math.log10(padded))
    base = 10.0 ** exponent
    for mantissa in (1.0, 1.2, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0):
        limit = mantissa * base
        if limit >= padded:
            return limit
    return 10.0 * base


def _tick_step(limit: float) -> float:
    raw = limit / 4.0
    exponent = math.floor(math.log10(raw))
    base = 10.0 ** exponent
    for mantissa in (1.0, 2.0, 2.5, 5.0, 10.0):
        step = mantissa * base
        if step >= raw:
            return step
    return 10.0 * base


def _ticks(limit: float, step: float) -> list[float]:
    count = int(round(limit / step))
    return [i * step for i in range(-count, count + 1)]


def _tick_format(step: float) -> str:
    if step >= 1.0:
        return ".0f"
    if step >= 0.1:
        return ".1f"
    return ".2f"


def acceptance_svg(samples: Sequence[tuple[float, float]] | None = None) -> str:
    """Transfer figure for the committed acceptance curve."""
    rows = list(acceptance_samples() if samples is None else samples)
    width, height = 960, 560
    left, right, top, bottom = 76, 28, 72, 68
    plot_w = width - left - right
    plot_h = height - top - bottom
    peak = max(abs(y) for _, y in rows)
    y_lim = _nice_limit(peak)
    x_step = 2.0
    y_step = _tick_step(y_lim)
    x_fmt = _tick_format(x_step)
    y_fmt = _tick_format(y_step)

    def x_px(v: float) -> float:
        return left + (v - VIN_MIN) / (VIN_MAX - VIN_MIN) * plot_w

    def y_px(y: float) -> float:
        return top + (y_lim - y) / (2.0 * y_lim) * plot_h

    parts: list[str] = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        "<!-- Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->",
        (
            '<svg xmlns="http://www.w3.org/2000/svg" '
            f'width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
        ),
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        (
            '<text x="76" y="28" font-family="sans-serif" font-size="18" '
            'fill="#222">Serge middle section, g = 1</text>'
        ),
        (
            '<text x="76" y="50" font-family="sans-serif" font-size="13" '
            'fill="#444">Six cells. vin from -6 V to 6 V. Raw map, before '
            "output gain.</text>"
        ),
    ]

    def line(x1: float, y1: float, x2: float, y2: float, stroke: str) -> str:
        return (
            f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
            f'stroke="{stroke}" stroke-width="1"/>'
        )

    plot_right = left + plot_w
    plot_bottom = top + plot_h
    for tick in _ticks(y_lim, y_step):
        py = y_px(tick)
        parts.append(line(left, py, plot_right, py, "#eeeeee"))
    for tick in _ticks(VIN_MAX, x_step):
        px = x_px(tick)
        parts.append(line(px, top, px, plot_bottom, "#eeeeee"))
    parts.append(line(x_px(0.0), top, x_px(0.0), plot_bottom, "#cccccc"))
    parts.append(line(left, y_px(0.0), plot_right, y_px(0.0), "#cccccc"))
    parts.append(line(left, top, left, plot_bottom, "#222222"))
    parts.append(line(left, plot_bottom, plot_right, plot_bottom, "#222222"))

    for tick in _ticks(VIN_MAX, x_step):
        px = x_px(tick)
        label = format(tick, x_fmt)
        parts.append(
            f'<text x="{px:.2f}" y="{plot_bottom + 18:.2f}" text-anchor="middle" '
            'font-family="sans-serif" font-size="12" fill="#222">'
            f"{label}</text>"
        )
    for tick in _ticks(y_lim, y_step):
        py = y_px(tick)
        label = format(tick, y_fmt)
        parts.append(
            f'<text x="{left - 8:.2f}" y="{py + 4:.2f}" text-anchor="end" '
            'font-family="sans-serif" font-size="12" fill="#222">'
            f"{label}</text>"
        )

    points = " ".join(f"{x_px(v):.2f},{y_px(y):.2f}" for v, y in rows)
    parts.append(
        f'<polyline fill="none" stroke="#111111" stroke-width="1.5" '
        f'stroke-linejoin="round" points="{points}"/>'
    )
    parts.append(
        f'<text x="{left + plot_w / 2:.2f}" y="{height - 16}" text-anchor="middle" '
        'font-family="sans-serif" font-size="14" fill="#222">vin (V)</text>'
    )
    parts.append(
        f'<text transform="translate(22,{top + plot_h / 2:.2f}) rotate(-90)" '
        'text-anchor="middle" font-family="sans-serif" font-size="14" '
        'fill="#222">vout (V)</text>'
    )
    parts.append("</svg>")
    parts.append("")
    return "\n".join(parts)


def write_acceptance(directory: Path | None = None) -> Path:
    """Write tests/transfer_g1.csv and tests/transfer_g1.svg."""
    dest = _ACCEPTANCE_DIR if directory is None else Path(directory)
    dest.mkdir(parents=True, exist_ok=True)
    samples = acceptance_samples()
    csv_path = dest / "transfer_g1.csv"
    svg_path = dest / "transfer_g1.svg"
    csv_path.write_text(acceptance_csv(samples), encoding="utf-8")
    svg_path.write_text(acceptance_svg(samples), encoding="utf-8")
    return dest
