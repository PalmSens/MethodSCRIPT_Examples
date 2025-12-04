"""
PalmSens MethodSCRIPT module

This module provides functionality to translate and interpret the output of a
MethodSCRIPT (the measurement data).

The most relevant functions are:
  - parse_mscript_data_package(line)
  - parse_result_lines(lines)

-------------------------------------------------------------------------------
Copyright (c) 2021 PalmSens BV
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Neither the name of PalmSens BV nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.
   - This license does not release you from any requirement to obtain separate
     licenses from 3rd party patent holders to use this software.
   - Use of the software either in source or binary form must be connected to,
     run on or loaded to an PalmSens BV component.

DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

# Standard library imports
from enum import Enum
import math
import string
from typing import NamedTuple
import warnings


# Custom types
class VarType(NamedTuple):
    id: str
    name: str
    unit: str


# Dictionary for the conversion of the SI prefixes.
SI_PREFIX_FACTOR = {
    # supported SI prefixes:
    "a": 1e-18,  # atto
    "f": 1e-15,  # femto
    "p": 1e-12,  # pico
    "n": 1e-9,  # nano
    "u": 1e-6,  # micro
    "m": 1e-3,  # milli
    " ": 1e0,
    "k": 1e3,  # kilo
    "M": 1e6,  # mega
    "G": 1e9,  # giga
    "T": 1e12,  # tera
    "P": 1e15,  # peta
    "E": 1e18,  # exa
    # special case:
    "i": 1e0,  # integer
}

# List of MethodSCRIPT variable types.
MSCRIPT_VAR_TYPES_LIST = [
    VarType("aa", "unknown", ""),
    VarType("ab", "WE vs RE potential", "V"),
    VarType("ac", "CE vs GND potential", "V"),
    VarType("ad", "SE vs GND potential", "V"),
    VarType("ae", "RE vs GND potential", "V"),
    VarType("af", "WE vs GND potential", "V"),
    VarType("ag", "WE vs CE potential", "V"),
    VarType("as", "AIN0 potential", "V"),
    VarType("at", "AIN1 potential", "V"),
    VarType("au", "AIN2 potential", "V"),
    VarType("av", "AIN3 potential", "V"),
    VarType("aw", "AIN4 potential", "V"),
    VarType("ax", "AIN5 potential", "V"),
    VarType("ay", "AIN6 potential", "V"),
    VarType("az", "AIN7 potential", "V"),
    VarType("ba", "WE current", "A"),
    VarType("ca", "Phase", "degrees"),
    VarType("cb", "Impedance", "\u2126"),  # NB: '\u2126' = ohm symbol
    VarType("cc", "Z_real", "\u2126"),
    VarType("cd", "Z_imag", "\u2126"),
    VarType("ce", "EIS E TDD", "V"),
    VarType("cf", "EIS I TDD", "A"),
    VarType("cg", "EIS sampling frequency", "Hz"),
    VarType("ch", "EIS E AC", "Vrms"),
    VarType("ci", "EIS E DC", "V"),
    VarType("cj", "EIS I AC", "Arms"),
    VarType("ck", "EIS I DC", "A"),
    VarType("da", "Applied potential", "V"),
    VarType("db", "Applied current", "A"),
    VarType("dc", "Applied frequency", "Hz"),
    VarType("dd", "Applied AC amplitude", "Vrms"),
    VarType("ea", "Channel", ""),
    VarType("eb", "Time", "s"),
    VarType("ec", "Pin mask", ""),
    VarType("ed", "Temperature", "\u00b0 Celsius"),  # NB: '\u00B0' = degrees symbol
    VarType("ee", "Count", ""),
    VarType("ha", "Generic current 1", "A"),
    VarType("hb", "Generic current 2", "A"),
    VarType("hc", "Generic current 3", "A"),
    VarType("hd", "Generic current 4", "A"),
    VarType("ia", "Generic potential 1", "V"),
    VarType("ib", "Generic potential 2", "V"),
    VarType("ic", "Generic potential 3", "V"),
    VarType("id", "Generic potential 4", "V"),
    VarType("ja", "Misc. generic 1", ""),
    VarType("jb", "Misc. generic 2", ""),
    VarType("jc", "Misc. generic 3", ""),
    VarType("jd", "Misc. generic 4", ""),
]

MSCRIPT_VAR_TYPES_DICT = {x.id: x for x in MSCRIPT_VAR_TYPES_LIST}

METADATA_STATUS_FLAGS = [
    (0x1, "TIMING_ERROR"),
    (0x2, "OVERLOAD"),
    (0x4, "UNDERLOAD"),
    (0x8, "OVERLOAD_WARNING"),
]

MSCRIPT_CURRENT_RANGES_EMSTAT_PICO = {
    0: "100 nA",
    1: "2 uA",
    2: "4 uA",
    3: "8 uA",
    4: "16 uA",
    5: "32 uA",
    6: "63 uA",
    7: "125 uA",
    8: "250 uA",
    9: "500 uA",
    10: "1 mA",
    11: "5 mA",
    128: "100 nA (High speed)",
    129: "1 uA (High speed)",
    130: "6 uA (High speed)",
    131: "13 uA (High speed)",
    132: "25 uA (High speed)",
    133: "50 uA (High speed)",
    134: "100 uA (High speed)",
    135: "200 uA (High speed)",
    136: "1 mA (High speed)",
    137: "5 mA (High speed)",
}

MSCRIPT_CURRENT_RANGES_EMSTAT4 = {
    # EmStat4 LR only:
    3: "1 nA",
    6: "10 nA",
    # EmStat4 LR/HR:
    9: "100 nA",
    12: "1 uA",
    15: "10 uA",
    18: "100 uA",
    21: "1 mA",
    24: "10 mA",
    # EmStat4 HR only:
    27: "100 mA",
}

MSCRIPT_POTENTIAL_RANGES_EMSTAT4 = {
    2: "50 mV",
    3: "100 mV",
    4: "200 mV",
    5: "500 mV",
    6: "1 V",
}

MSCRIPT_CURRENT_RANGES_NEXUS = {
    # Potentiostat ranges
    16: "100 pA",
    0: "1 nA",
    1: "10 nA",
    2: "100 nA",
    3: "1 uA",
    4: "10 uA",
    5: "100 uA",
    6: "1 mA (tia)",
    7: "10 mA (tia)",
    8: "1 mA",
    9: "10 mA",
    10: "100 mA",
    11: "1 A",
    # Galvanostat ranges
    32: "1 nA",
    33: "10 nA",
    34: "100 nA",
    35: "1 uA",
    36: "10 uA",
    37: "100 uA",
    38: "1 mA (tia)",
    39: "10 mA (tia)",
    40: "1 mA",
    41: "10 mA",
    42: "100 mA",
    43: "1 A",
}

MSCRIPT_POTENTIAL_RANGES_NEXUS = {
    0: "1 V",
    1: "100 mV",
    2: "10 mV",
    3: "1 mV",
}


def get_variable_type(var_id: str) -> VarType:
    """Get the variable type with the specified id."""
    if var_id in MSCRIPT_VAR_TYPES_DICT:
        return MSCRIPT_VAR_TYPES_DICT[var_id]
    warnings.warn(f'Unsupported VarType id "{var_id}"!')
    return VarType(var_id, "unknown", "")


def metadata_status_to_text(status: int) -> str:
    """Convert a metadata status to text"""
    descriptions = [
        description for mask, description in METADATA_STATUS_FLAGS if status & mask
    ]
    return " | ".join(descriptions) if descriptions else "OK"


def _metadata_current_range_to_text(device_type: str, cr: int) -> str:
    if device_type == "EmStat Pico":
        return MSCRIPT_CURRENT_RANGES_EMSTAT_PICO.get(cr) or "UNKNOWN CURRENT RANGE"
    if "EmStat4" in device_type:
        return MSCRIPT_CURRENT_RANGES_EMSTAT4.get(cr) or "UNKNOWN CURRENT RANGE"
    if device_type == "Nexus":
        return MSCRIPT_CURRENT_RANGES_NEXUS.get(cr) or "UNKNOWN CURRENT RANGE"
    return "UNKNOWN DEVICE TYPE"


def _metadata_potential_range_to_text(device_type: str, cr: int) -> str:
    if "EmStat4" in device_type:
        return MSCRIPT_POTENTIAL_RANGES_EMSTAT4.get(cr) or "UNKNOWN POTENTIAL RANGE"
    if device_type == "Nexus":
        return MSCRIPT_POTENTIAL_RANGES_NEXUS.get(cr) or "UNKNOWN POTENTIAL RANGE"
    return "UNKNOWN DEVICE TYPE"


def metadata_range_to_text(device_type: str, var_type: VarType, cr: int) -> str:
    """Convert a metadata range to text"""
    if var_type.unit == "A" or var_type.id == "cc":
        # Z_real contains the metadata of the current range
        return _metadata_current_range_to_text(device_type, cr)
    if var_type.unit == "V" or var_type.unit == "Vrms" or var_type.id == "cd":
        # Z_imag contains the metadata of the potential range
        return _metadata_potential_range_to_text(device_type, cr)
    return f"UNKNOWN UNIT: {var_type.unit}"


class MScriptVar:
    """Class to store and parse a received MethodSCRIPT variable."""

    def __init__(self, data: str):
        assert len(data) >= 10
        self.data = data[:]
        # Parse the variable type.
        self.id = data[0:2]
        # Check for NaN.
        if data[2:10] == "     nan":
            self.raw_value = math.nan
            self.si_prefix = " "
        else:
            # Parse the (raw) value,
            self.raw_value = self.decode_value(data[2:9])
            # Store the SI prefix.
            self.si_prefix = data[9]
        # Store the (raw) metadata.
        self.raw_metadata = data.split(",")[1:]
        # Parse the metadata.
        self.metadata = self.parse_metadata(self.raw_metadata)

    def __repr__(self):
        return f"MScriptVar({self.data!r})"

    def __str__(self):
        return self.value_string

    @property
    def type(self) -> VarType:
        return get_variable_type(self.id)

    @property
    def si_prefix_factor(self) -> float:
        return SI_PREFIX_FACTOR[self.si_prefix]

    @property
    def value(self) -> float:
        return self.raw_value * self.si_prefix_factor

    @property
    def value_string(self) -> str:
        if self.type.unit:
            if self.si_prefix_factor == 1:
                if math.isnan(self.value):
                    return f"NaN {self.type.unit}"
                return f"{self.raw_value} {self.type.unit}"
            return f"{self.raw_value} {self.si_prefix}{self.type.unit}"
        return f"{self.value:.9g}"

    @staticmethod
    def decode_value(var: str):
        """Decode the raw value of a MethodSCRIPT variable in a data package.

        The input is a 7-digit hexadecimal string (without the variable type
        and/or SI prefix). The output is the converted (signed) integer value.
        """
        assert len(var) == 7
        # Convert the 7 hexadecimal digits to an integer value and
        # subtract the offset.
        return int(var, 16) - (2**27)

    @staticmethod
    def parse_metadata(tokens: list[str]) -> dict[str, int]:
        """Parse the (optional) metadata."""
        metadata: dict[str, int] = {}
        for token in tokens:
            if (len(token) == 2) and (token[0] == "1"):
                value = int(token[1], 16)
                metadata["status"] = value
            if (len(token) == 3) and (token[0] == "2"):
                value = int(token[1:], 16)
                metadata["cr"] = value
        return metadata


class ScopeType(Enum):
    """Which type of scope this is"""

    OUTER = 0
    MEAS_LOOP = 1
    LOOP = 2
    SCAN = 3

    @staticmethod
    def from_char(label: str):
        """Convert a character from MethodSCRIPT output to a ScopeType"""
        return {
            "M": ScopeType.MEAS_LOOP,
            "L": ScopeType.LOOP,
            "C": ScopeType.SCAN,
        }[label]


class Package(tuple[MScriptVar, ...]):
    """A list of MScriptVars"""


class MScriptLoop:
    """A loop in MethodScript, including all nested loops.

    An MScriptLoop is a tree of Packages and MScriptLoops. Each Package is a
    list of MScriptVars.

    The way to access this data depends on the structure of the MethodSCRIPT.
    For example, if there were multiple measurement loops, each variable can be
    accessed as result.loops[loop].packages[row][col]. For example,
    `result.loops[1].packages[2][3]` is the 4th variable of the 3th data point
    of the 2nd measurement loop.

    This is a bidirectional ordered tree, where all leaf nodes are Packages, and
    all non-leaf nodes are MethodSCRIPT loops (`meas_loop`, `loop`, or using
    `nscans()`). The order of child nodes corresponds to the order in which
    MethodSCRIPT data is received.
    """

    def __init__(self, scope_type: ScopeType, parent: "MScriptLoop | None"):
        self.scope_type = scope_type
        self.parent: MScriptLoop | None = parent
        self.data: list[Package | MScriptLoop] = []

    def __len__(self):
        return len(self.data)

    @property
    def packages(self) -> list[Package]:
        """Get only the data from the outermost loop. This ignores any nested
        loops."""
        return [d for d in self.data if isinstance(d, Package)]

    @property
    def loops(self) -> list["MScriptLoop"]:
        """Get only the nested loops. This ignores any top-level Packages."""
        return [d for d in self.data if not isinstance(d, Package)]

    def get_column(self, n: int) -> tuple[MScriptVar, ...]:
        """Get all variables in a single column of data.
        In other words, get the `n`th variable of each package."""
        return tuple(pck[n] for pck in self.packages)

    def get_column_values(self, n: int) -> tuple[float, ...]:
        """Get the values of all variables in a single column of data.
        In other words, get the value of the `n`th variable of each package."""
        return tuple(p.value for p in self.get_column(n))

    def _append_nested_loop(self, scope_type: ScopeType) -> "MScriptLoop":
        loop = MScriptLoop(scope_type, parent=self)
        self.data.append(loop)
        return loop

    def _append_package(self, data: Package) -> None:
        """Append a Package to this loop"""
        self.data.append(data)


def parse_mscript_data_package(line: str) -> Package:
    """Parse a MethodSCRIPT data package.

    The format of a MethodSCRIPT data package is described in the
    MethodSCRIPT documentation. It starts with a 'P' and ends with a
    '\n' character. A package consists of an arbitrary number of
    variables. Each variable consists of a type (describing the
    variable), a value, and optionally one or more metadata values.

    This method returns a list of variables (of type `MScriptVar`)
    found in the line.
    """
    return Package([MScriptVar(var) for var in line[1:].split(";")])


def is_valid_start_of_loop(line: str) -> bool:
    """If the given line is a valid start of an MScript loop (MXXXX, CXXXX, or L)"""
    match line[0]:
        case "M" | "C":
            return len(line) == 5 and all(c in string.hexdigits for c in line[1:])
        case "L":
            return len(line) == 1
        case _:
            return False


def termination_matches_scope(char: str, scope_type: ScopeType) -> bool:
    """If the given termination character matches the given scope type"""
    match char:
        case "+":
            return scope_type is ScopeType.LOOP
        case "-":
            return scope_type is ScopeType.SCAN
        case "*":
            return scope_type is ScopeType.MEAS_LOOP
        case _:
            return False


class InvalidMscriptOutputError(Exception):
    """An error was detected in the MethodSCRIPT output being parsed."""


def parse_result_lines(lines: list[str]) -> MScriptLoop:
    """Parse the output of a MethodSCRIPT run."""
    loop: MScriptLoop = MScriptLoop(ScopeType.OUTER, None)
    for line in [line.rstrip("\n") for line in lines]:
        # NOTE:
        # 'M' = start of measurement loop
        # 'L' = start of loop
        # 'C' = start of scan, within measurement loop, in case nscans(>1)
        # '+' = end of loop
        # '*' = end of measurement loop
        # '-' = end of scan, within measurement loop, in case nscans(>1)
        if line and line[0] in "+*-MCLP":
            # Begin or end of scan or (measurement) loop detected.
            match line[0]:
                case "M" | "L" | "C":
                    if is_valid_start_of_loop(line):
                        loop = loop._append_nested_loop(ScopeType.from_char(line[0]))
                    else:
                        raise InvalidMscriptOutputError(
                            f"Invalid start of loop: {line}"
                        )
                case "+" | "-" | "*":
                    if termination_matches_scope(line[0], loop.scope_type):
                        assert loop.parent is not None, (
                            "Trying to terminate outermost scope"
                        )
                        loop = loop.parent
                        # If the loop we just added (which is the last element of the parent) has no data, remove it.
                        if not loop.data[-1].data:
                            loop.data.pop()
                    else:
                        raise InvalidMscriptOutputError(
                            f"End-of-scope marker {line[0]} "
                            f"does not match current scope type {loop.scope_type}"
                        )
                case "P":
                    loop._append_package(parse_mscript_data_package(line))
    if loop.scope_type != ScopeType.OUTER:
        raise InvalidMscriptOutputError(
            "Invalid nesting. Did not encounter equal start- and end-of-loop lines."
        )
    # Reduce nesting if possible
    if (
        loop.scope_type is ScopeType.OUTER
        and len(loop.loops) == 1
        and len(loop.packages) == 0
    ):
        loop = loop.loops[0]
        loop.parent = None
    # Scope type might not be "Outer" any more here, because there was only a
    # single loop in the MethodSCRIPT output, so that loop has become the
    # outermost loop.
    return loop
