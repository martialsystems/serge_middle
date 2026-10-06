"""Memoryless map of the Serge Wave Multipliers middle section.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Six identical cells. The acceptance curve is this map at g = 1 on
vin in [-6, 6], before output gain, the fold-amount bound, the
smoother, and the DC block. The 20 ms smoother and the 63-tap
decimator are locked in this file. The iPlug2 module is not.
"""

from __future__ import annotations

import math
from pathlib import Path
from typing import Sequence

# Shockley parameters for one cell. VT is in volts, Is in amperes, R in ohms.
# v1 lock: tests/transfer_g1.csv uses this pair. Esqueda et al. 2017 Table 4
# (eta 1.752, VT 0.025864) is a different fit and is not the plugin.
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

# Audio sample a in [-1, 1]. d(g) = g, so v = 5 * d(g) * a.
# At g = 1 that is ±5 V. OUTPUT_GAIN is the reciprocal of the raw peak there.
# DRIVE_PEAKS is P(g) = max |process| on |v| <= 5 d(g). The bound is P(1) / P(g).
AUDIO_FULL_SCALE_VOLTS = 5.0
FULL_SCALE_PEAK_VIN = 4.7072868603604885
OUTPUT_GAIN = 4.3792716960440945

# One-pole highpass after the bound. r = exp(-2 * pi * DC_BLOCK_HZ / fs).
DC_BLOCK_HZ = 10.0

# One-pole on g, once per audio sample. c = exp(-1 / (SMOOTH_SECONDS * fs)).
SMOOTH_SECONDS = 0.020

# 4x upsample, six cells, 63-tap lowpass, decimate by 4.
OVERSAMPLE = 4
HALFBAND_TAPS_N = 63
# Cycles per sample of the 4x rate. 0.125 is the audio Nyquist.
HALFBAND_PASSBAND_EDGE = 0.125
HALFBAND_STOP_EDGE = 0.75 / 4

# P(g) at the knots below. Linear interpolation in g stays within this
# relative error on [G_MIN, G_MAX]: |P / P_hat - 1| <= DRIVE_PEAK_REL_ERROR.
DRIVE_PEAK_REL_ERROR = 1e-3
DRIVE_PEAKS = (
    (0.5, 0.1775277872935283),
    (0.5474003977393376, 0.1775277872935283),
    (0.5497340568500901, 0.1801083216190401),
    (0.5521767654519992, 0.1823810649417758),
    (0.5547067136468338, 0.1843095472151966),
    (0.5573675212310563, 0.18591382561662023),
    (0.5601591882046668, 0.18717748565229037),
    (0.5630599046694339, 0.18808406500314873),
    (0.566091480523589, 0.18863517809647518),
    (0.5692102959706695, 0.18881896732707976),
    (0.7307789759456843, 0.18881896732707976),
    (0.7332654411052391, 0.19231787361341546),
    (0.7358191080258628, 0.19538316727663818),
    (0.7384623772945786, 0.19801606632845936),
    (0.7411952489113867, 0.20019412313069718),
    (0.7440401234633096, 0.20191527290684846),
    (0.7469970009503477, 0.20316031609079727),
    (0.7500434807854778, 0.20391057268140328),
    (0.7531795629687, 0.20416215561354206),
    (0.9185864093019299, 0.20416215561354206),
    (0.9213309248495607, 0.2096438416071082),
    (0.9240983113600885, 0.21449114855650264),
    (0.9268885688335132, 0.21865434960366537),
    (0.9297016972698348, 0.22209582167109464),
    (0.9325605676319503, 0.22480970979773496),
    (0.9354651799198596, 0.22676595163237234),
    (0.9384384050964597, 0.2279533761144198),
    (0.9414573721988535, 0.22834847193959784),
    (1.0, 0.22834847193959784),
    (1.1099886913655106, 0.22834847193959784),
    (1.1168787026741451, 0.2538347487518584),
    (1.1306587252914142, 0.30541809576153445),
    (1.144438747908683, 0.3577434904844825),
    (1.1582187705259521, 0.41073672590994126),
    (1.1788888044518555, 0.49134440207063745),
    (1.1995588383777591, 0.573141728721227),
    (1.2340088949209316, 0.7117458228533513),
    (1.2753489627727383, 0.8812349685007477),
    (1.3304690532418142, 1.1115937204565158),
    (1.4062591776367936, 1.43480382544928),
    (1.509609347266311, 1.8847703095863748),
    (1.5853994716612905, 2.2199965883279056),
    (1.6749696186735388, 2.6207186070952035),
    (1.792099810920325, 3.1507658632937945),
    (1.943680059710284, 3.8447414237834288),
    (2.0, 4.104493885791202),
    (2.1503803989693187, 4.80224901460087),
    (2.4466508852406017, 6.191140686600411),
    (2.894501620301843, 8.315964343499907),
    (3.617952807708465, 11.78910827317095),
    (4.0, 13.636725832047457),
    (4.954615001583555, 18.279866505401003),
    (8.0, 33.228577867627834),
)

_LOBE_SCAN_STEP = 0.005

_ETA_VT = ETA * VT
_LOG_K = math.log((IS * R) / _ETA_VT)

_ROOT = Path(__file__).resolve().parent
_ACCEPTANCE_DIR = _ROOT / "tests"


def _load_tap_file(name: str, count: int) -> tuple[float, ...]:
    """Load a tap CSV. Blank lines and '#' comments are skipped."""
    taps = []
    for line in (_ROOT / name).read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        value = float(stripped)
        if not math.isfinite(value):
            raise RuntimeError(f"{name} coefficient {len(taps)} is not finite")
        taps.append(value)
    if len(taps) != count:
        raise RuntimeError(f"{name} has {len(taps)} taps, expected {count}")
    return tuple(taps)


# Decimator sum is 1. Upsampler taps are 4 times those, so zero-insertion
# of a baseband tone returns at unity. The CSV files are the lock.
HALFBAND_TAPS = _load_tap_file("halfband_taps.csv", HALFBAND_TAPS_N)
UPSAMPLE_TAPS = _load_tap_file("upsample_taps.csv", HALFBAND_TAPS_N)


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
    """y = C applied `cells` times to g*x. Default cells is 6, default g is 1.

    x is cell-input volts when g is 1. The acceptance curve is this map.
    Plugin audio uses audio_map: v = 5 * d(g) * a, then OUTPUT_GAIN * P(1) / P(g).
    """
    if cells < 0:
        raise ValueError(f"cells must be non-negative, got {cells}")
    if not math.isfinite(g):
        raise ValueError(f"g must be finite, got {g}")
    y = g * x
    for _ in range(cells):
        y = cell(y)
    return y


def drive_scale(g: float) -> float:
    """d(g). Identity on [G_MIN, G_MAX], clamped outside that interval."""
    if not math.isfinite(g):
        raise ValueError(f"g must be finite, got {g}")
    if g < G_MIN:
        return G_MIN
    if g > G_MAX:
        return G_MAX
    return g


def drive_peak(g: float) -> float:
    """P(g), linear in g between DRIVE_PEAKS. g is clamped with d(g)."""
    g = drive_scale(g)
    knots = DRIVE_PEAKS
    if g <= knots[0][0]:
        return knots[0][1]
    last = len(knots) - 1
    if g >= knots[last][0]:
        return knots[last][1]
    lo, hi = 0, last
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if knots[mid][0] <= g:
            lo = mid
        else:
            hi = mid
    g0, p0 = knots[lo]
    g1, p1 = knots[hi]
    t = (g - g0) / (g1 - g0)
    return p0 + t * (p1 - p0)


def output_scale(g: float) -> float:
    """OUTPUT_GAIN * P(1) / P(g). At g = 1 this is OUTPUT_GAIN."""
    return OUTPUT_GAIN * drive_peak(G_DEFAULT) / drive_peak(g)


def audio_map(sample: float, g: float = G_DEFAULT) -> float:
    """One sample before oversampling and before the DC block.

    v = 5 * d(g) * sample, y = C^6(v), u = output_scale(g) * y.
    """
    if not math.isfinite(sample):
        raise ValueError(f"sample must be finite, got {sample}")
    volts = AUDIO_FULL_SCALE_VOLTS * drive_scale(g) * sample
    return output_scale(g) * process(volts)


def _golden_max_abs(lo: float, hi: float) -> tuple[float, float]:
    invphi = (math.sqrt(5.0) - 1.0) / 2.0
    a, b = lo, hi
    c = b - invphi * (b - a)
    d = a + invphi * (b - a)
    fc = abs(process(c))
    fd = abs(process(d))
    for _ in range(80):
        if fc > fd:
            b, d, fd = d, c, fc
            c = b - invphi * (b - a)
            fc = abs(process(c))
        else:
            a, c, fc = c, d, fd
            d = a + invphi * (b - a)
            fd = abs(process(d))
    v = 0.5 * (a + b)
    return v, abs(process(v))


def positive_lobe_peaks(limit: float) -> list[tuple[float, float]]:
    """Interior local maxima of |process| on (0, limit].

    The scan is cached. Plugin audio reads DRIVE_PEAKS instead of searching.
    """
    if limit < 0.0 or not math.isfinite(limit):
        raise ValueError(f"limit must be finite and non-negative, got {limit}")
    cached = getattr(positive_lobe_peaks, "_cache", None)
    if cached is not None and limit <= cached[0]:
        return [(v, peak) for v, peak in cached[1] if v <= limit]
    step = _LOBE_SCAN_STEP
    n = int(round(limit / step))
    if n < 2:
        return []
    vals = [abs(process(i * step)) for i in range(n + 1)]
    found: list[tuple[float, float]] = []
    for i in range(1, n):
        if vals[i] > vals[i - 1] and vals[i] >= vals[i + 1]:
            lo = (i - 1) * step
            hi = min(limit, (i + 1) * step)
            v, peak = _golden_max_abs(lo, hi)
            if found and abs(v - found[-1][0]) <= 0.02:
                if peak > found[-1][1]:
                    found[-1] = (v, peak)
            else:
                found.append((v, peak))
    positive_lobe_peaks._cache = (limit, found)  # type: ignore[attr-defined]
    return list(found)


def max_abs_process(limit: float) -> float:
    """max |process(v)| for v in [0, limit]. The peak is a lobe or the endpoint."""
    if limit < 0.0 or not math.isfinite(limit):
        raise ValueError(f"limit must be finite and non-negative, got {limit}")
    if limit == 0.0:
        return 0.0
    best = 0.0
    for v, peak in positive_lobe_peaks(limit):
        if v <= limit and peak > best:
            best = peak
    end = abs(process(limit))
    if end > best:
        return end
    return best


def dc_block_pole(sample_rate: float) -> float:
    """r = exp(-2 * pi * DC_BLOCK_HZ / sample_rate)."""
    if not math.isfinite(sample_rate) or sample_rate <= 0.0:
        raise ValueError(f"sample_rate must be finite and positive, got {sample_rate}")
    return math.exp(-2.0 * math.pi * DC_BLOCK_HZ / sample_rate)


def dc_block_step(
    x: float, x_prev: float, y_prev: float, pole: float
) -> tuple[float, float, float]:
    """y[n] = x[n] - x[n-1] + pole * y[n-1]. Returns (y, next_x_prev, next_y_prev)."""
    y = x - x_prev + pole * y_prev
    return y, x, y


def dc_block(
    samples: Sequence[float],
    sample_rate: float,
    state: tuple[float, float] = (0.0, 0.0),
) -> tuple[list[float], tuple[float, float]]:
    """Run the 10 Hz one-pole. state is (x_prev, y_prev), initially (0, 0)."""
    pole = dc_block_pole(sample_rate)
    x_prev, y_prev = state
    out: list[float] = []
    for x in samples:
        y, x_prev, y_prev = dc_block_step(x, x_prev, y_prev, pole)
        out.append(y)
    return out, (x_prev, y_prev)


def dc_block_magnitude(frequency: float, sample_rate: float) -> float:
    """|H(e^{jw})| of the one-pole, w = 2 * pi * frequency / sample_rate."""
    if frequency < 0.0 or not math.isfinite(frequency):
        raise ValueError(f"frequency must be finite and non-negative, got {frequency}")
    pole = dc_block_pole(sample_rate)
    w = 2.0 * math.pi * frequency / sample_rate
    cosine = math.cos(w)
    num = 2.0 - 2.0 * cosine
    den = 1.0 + pole * pole - 2.0 * pole * cosine
    return math.sqrt(num / den)


def smooth_coeff(sample_rate: float) -> float:
    """c = exp(-1 / (SMOOTH_SECONDS * sample_rate))."""
    if not math.isfinite(sample_rate) or sample_rate <= 0.0:
        raise ValueError(
            f"sample_rate must be finite and positive, got {sample_rate}"
        )
    return math.exp(-1.0 / (SMOOTH_SECONDS * sample_rate))


def smooth_g(state: float, target: float, coeff: float) -> float:
    """One audio-rate step of the fold-amount smoother.

    g[n] = g[n-1] + (1 - c) * (g_target - g[n-1]).
    The target is clamped with drive_scale. The state starts at the target.
    """
    if not math.isfinite(state):
        raise ValueError(f"state must be finite, got {state}")
    if not math.isfinite(coeff) or coeff < 0.0 or coeff > 1.0:
        raise ValueError(f"coeff must be finite and in [0, 1], got {coeff}")
    target = drive_scale(target)
    return state + (1.0 - coeff) * (target - state)


def hold_phases(g: float, phases: int = OVERSAMPLE) -> tuple[float, ...]:
    """The same g on every oversampled phase of one audio sample."""
    if phases < 1:
        raise ValueError(f"phases must be positive, got {phases}")
    if not math.isfinite(g):
        raise ValueError(f"g must be finite, got {g}")
    return (g,) * phases


def fir_step(
    x: float, taps: Sequence[float], delay: Sequence[float]
) -> tuple[float, tuple[float, ...]]:
    """Direct-form FIR. delay[k] is the input k+1 samples ago.

    len(delay) is len(taps) - 1. The initial delay is zeros.
    """
    if not math.isfinite(x):
        raise ValueError(f"x must be finite, got {x}")
    n_taps = len(taps)
    if len(delay) != n_taps - 1:
        raise ValueError(f"delay length must be {n_taps - 1}, got {len(delay)}")
    acc = taps[0] * x
    for k in range(1, n_taps):
        acc += taps[k] * delay[k - 1]
    if n_taps == 1:
        return acc, ()
    previous = tuple(delay)
    return acc, (x,) + previous[:-1]


def fir_run(
    samples: Sequence[float],
    taps: Sequence[float],
    delay: Sequence[float] | None = None,
) -> tuple[list[float], tuple[float, ...]]:
    """Run fir_step. The default delay is zeros."""
    state = (
        tuple(0.0 for _ in range(len(taps) - 1))
        if delay is None
        else tuple(delay)
    )
    out: list[float] = []
    for sample in samples:
        y, state = fir_step(sample, taps, state)
        out.append(y)
    return out, state


def insert_zeros(samples: Sequence[float], factor: int = OVERSAMPLE) -> list[float]:
    """Insert factor-1 zeros after each sample. factor 4 is the upsampler."""
    if factor < 1:
        raise ValueError(f"factor must be positive, got {factor}")
    if factor == 1:
        return [float(sample) for sample in samples]
    padding = [0.0] * (factor - 1)
    out: list[float] = []
    for sample in samples:
        if not math.isfinite(sample):
            raise ValueError(f"sample must be finite, got {sample}")
        out.append(float(sample))
        out.extend(padding)
    return out


def decimate(
    samples: Sequence[float], factor: int = OVERSAMPLE, phase: int = 0
) -> list[float]:
    """Keep every factor-th sample, starting at phase. Phase 0 is the lock."""
    if factor < 1:
        raise ValueError(f"factor must be positive, got {factor}")
    if phase < 0 or phase >= factor:
        raise ValueError(f"phase must be in [0, {factor}), got {phase}")
    return list(samples[phase::factor])


def fir_magnitude(taps: Sequence[float], cycles_per_sample: float) -> float:
    """|H| of a real FIR at cycles_per_sample, from the tap sum."""
    if not math.isfinite(cycles_per_sample):
        raise ValueError(
            f"cycles_per_sample must be finite, got {cycles_per_sample}"
        )
    omega = 2.0 * math.pi * cycles_per_sample
    real = 0.0
    imag = 0.0
    for index, coef in enumerate(taps):
        real += coef * math.cos(omega * index)
        imag -= coef * math.sin(omega * index)
    return math.hypot(real, imag)


def stopband_peak(
    taps: Sequence[float] | None = None,
    edge: float = HALFBAND_STOP_EDGE,
    points: int = 8192,
) -> float:
    """Max |H| from edge through 0.5 cycles, on points+1 uniform frequencies."""
    if taps is None:
        taps = HALFBAND_TAPS
    if points < 1:
        raise ValueError(f"points must be positive, got {points}")
    if not math.isfinite(edge) or edge < 0.0 or edge > 0.5:
        raise ValueError(f"edge must be finite and in [0, 0.5], got {edge}")
    worst = 0.0
    span = 0.5 - edge
    for index in range(points + 1):
        worst = max(worst, fir_magnitude(taps, edge + span * index / points))
    return worst


def passband_deviation(
    taps: Sequence[float] | None = None,
    edge: float = HALFBAND_PASSBAND_EDGE,
    points: int = 8192,
) -> float:
    """Max | |H| - 1 | from 0 through edge, on points+1 uniform frequencies."""
    if taps is None:
        taps = HALFBAND_TAPS
    if points < 1:
        raise ValueError(f"points must be positive, got {points}")
    if not math.isfinite(edge) or edge < 0.0 or edge > 0.5:
        raise ValueError(f"edge must be finite and in [0, 0.5], got {edge}")
    worst = 0.0
    for index in range(points + 1):
        worst = max(worst, abs(fir_magnitude(taps, edge * index / points) - 1.0))
    return worst


def _bisect_root(lo: float, hi: float, flo: float, fhi: float) -> float:
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        fm = process(mid)
        if flo * fm <= 0.0:
            hi, fhi = mid, fm
        else:
            lo, flo = mid, fm
    return 0.5 * (lo + hi)


def positive_fold_roots() -> list[float]:
    """First root of each zero-crossing cluster on (0.05, 6].

    When one stage passes near 0 V, the later stages cross zero several times
    inside about 2 mV. Those roots are one fold. The returned value is the
    leftmost root of the cluster.
    """
    scan_min = 0.05
    step = 1e-4
    cluster_gap = 0.01
    n = int(round((VIN_MAX - scan_min) / step))
    previous_v = scan_min
    previous_y = process(previous_v)
    roots: list[float] = []
    for i in range(1, n + 1):
        v = scan_min + i * step
        y = process(v)
        if previous_y * y < 0.0:
            root = _bisect_root(previous_v, v, previous_y, y)
            if not roots or root - roots[-1] > cluster_gap:
                roots.append(root)
        previous_v, previous_y = v, y
    return roots


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
