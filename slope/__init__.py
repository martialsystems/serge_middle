"""One universal slope, the first half.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""

from .second import AC_CENTER, SecondHalf, ac_volts, scaled_ac
from .slope import FS, SPAN, VC_SECONDS_PER_VOLT, Slope

__all__ = [
    "AC_CENTER",
    "FS",
    "SPAN",
    "VC_SECONDS_PER_VOLT",
    "SecondHalf",
    "Slope",
    "ac_volts",
    "scaled_ac",
]
