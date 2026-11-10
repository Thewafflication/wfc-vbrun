"""Writes the Visual Basic 6 harness that measures exports of MSVBVM60.

Some exports (the rtc*Var functions, ...) read the thread state that a running
Visual Basic program creates, so they fault when they are called from
PowerShell (scripts/Probe-VbrunCases.ps1). The harness declares them (Declare
Function ... Lib "msvbvm60.dll") and calls them from inside a program.

An experiment is an export, its signature, and a grid of arguments. The
program runs one experiment per start (its number is the command line), so a
signature that is wrong faults only that run; the driver
(vb6_harness_cases.py) starts the program for every experiment and turns the
results into tests/cases/<family>.cases and .expected.

Usage: python vb6_harness_gen.py <output folder>
"""

import os
import sys

# Argument kinds: V a Variant by reference, S a String (its BSTR), L a Long,
# O the output Variant (always first for the functions that give a Variant).
# Result kinds: V the output Variant, s a BSTR, i a Long, w an Integer.

VARIANTS = [
    ('CInt(-5)', 'v2:-5'), ('CLng(1234)', 'v3:1234'), ('CLng(0)', 'v3:0'),
    ('CSng(-2.5)', 'v4:-2.5'), ('CDbl(7.9)', 'v5:7.9'),
    ('"-5"', 'v8"-5"'), ('"Hello World"', 'v8"Hello World"'),
    ('""', 'v8""'), ('Empty', 'v0'), ('Null', 'v1'), ('True', 'v11:-1'),
    ('CCur(-3.5)', 'v6:-35000'), ('CByte(7)', 'v17:7'),
    ('CDate(36526.5)', 'v7:36526.5'),
]
TEXTS = [
    ('"hello world"', 's"hello world"'), ('"hello"', 's"hello"'),
    ('""', 's""'), ('"  pad  "', 's"  pad  "'), ('"abc"', 's"abc"'),
    ('"ABC"', 's"ABC"'),
]
COUNTS = ['0', '1', '2', '5', '100']
MODES = [(m, 'i' + m) for m in ('0', '1')]
DATES = [
    ('CDate(36526.5)', 'v7:36526.5'), ('CDate(40000.75)', 'v7:40000.75'),
    ('"2000-01-01 13:45:10"', 'v8"2000-01-01 13:45:10"'), ('Null', 'v1'),
    ('Empty', 'v0'),
]

EXPERIMENTS = []


def experiment(export, args, result, grid, isolate=False):
    """One experiment; with isolate, one for every row (a call that faults
    then costs only its own row)."""
    if isolate:
        for row in grid:
            EXPERIMENTS.append((export, args, result, [row]))
    else:
        EXPERIMENTS.append((export, args, result, grid))


def pair(x, y):
    return [(a, b) for a in x for b in y]


# Left/Right/Mid on Variants: (result, text, count) and (result, text, start,
# length).
# The byte forms count bytes: an odd count leaves a half character, which a
# Variant formatted from Visual Basic cannot show, so only even counts.
for name, counts in (('rtcLeftVar', ('0', '2', '4', '100')),
                     ('rtcRightVar', ('0', '2', '4', '100')),
                     ('rtcLeftCharVar', COUNTS),
                     ('rtcRightCharVar', COUNTS)):
    experiment(name, 'OVL', 'V',
               [(v, (c, 'i' + c)) for v in VARIANTS for c in counts])
for name in ('rtcStrFromVar',):
    experiment(name, 'V', 's', [(v,) for v in VARIANTS])
for name in ('rtcHexVarFromVar', 'rtcOctVarFromVar', 'rtcVarBstrFromChar'):
    experiment(name, 'OV' if 'From' in name and 'Bstr' not in name else 'OL',
               'V',
               [(v,) for v in VARIANTS[:5] + VARIANTS[10:13]]
               if 'Bstr' not in name else [((c, 'i' + c),) for c in
                                           ('65', '97', '9', '200')])
for name in ('rtcGetYear', 'rtcGetMonthOfYear', 'rtcGetDayOfMonth',
             'rtcGetHourOfDay', 'rtcGetMinuteOfHour',
             'rtcGetSecondOfMinute'):
    experiment(name, 'OV', 'V', [(d,) for d in DATES])

# Exports whose signature was read from the disassembly but not confirmed:
# each shape is its own experiment, so a wrong one only fails its own run.
MISSING = ('Miss()', 'v10:x80020004')  # an omitted Optional Variant
STARTS = [(c, 'i' + c) for c in ('1', '3', '7', '20')]
LENGTHS = [('CLng(3)', 'v3:3'), ('CInt(100)', 'v2:100'), MISSING]
TEXTV = [VARIANTS[6], VARIANTS[2], VARIANTS[8], VARIANTS[9]]
for name in ('rtcMidVar', 'rtcMidCharVar'):
    experiment(name, 'OVLV', 'V',
               [(v, st, ln) for v in TEXTV for st in STARTS for ln in LENGTHS])
CHARS = [('"x"', 'v8"x"'), ('65', 'v3:65'), ('"ab"', 'v8"ab"')]
experiment('rtcStringVar', 'OLV', 'V',
           [(('3', 'i3'), c) for c in CHARS] + [(('0', 'i0'), CHARS[0])])
# InStrB and InStr: (result, start, text, find, compare), all Variants; the
# compare argument may be missing. (Found by trying every placement of a
# missing argument, a number and two texts: the first is the start, which
# must be a number, then the text, the text to find and the compare mode.)
for name in ('rtcInStr', 'rtcInStrChar'):
    experiment(name, 'OVVVV', 'V',
               [(st, tx, fd, cm)
                for st in [('CLng(1)', 'v3:1'), ('CLng(6)', 'v3:6'),
                           ('CLng(20)', 'v3:20')]
                for tx in [VARIANTS[6], VARIANTS[7]]
                for fd in [('"o"', 'v8"o"'), ('"zz"', 'v8"zz"'),
                           ('""', 'v8""'), ('"L"', 'v8"L"'),
                           ('"l"', 'v8"l"')]
                for cm in [MISSING, ('CLng(0)', 'v3:0'),
                           ('CLng(1)', 'v3:1')]])
experiment('rtcStrConvVar', 'OVL', 'V',
           [(v, (c, 'i' + c)) for v in (VARIANTS[6], VARIANTS[1])
            for c in ('1', '2', '3', '64')])
experiment('rtcGetDayOfWeek', 'OVL', 'V',
           [(d, (f, 'i' + f)) for d in DATES[:3] for f in ('0', '1', '2')])

# The numeric helpers: a Double by value, a Double result.
DOUBLES = [('0', 'd0'), ('1', 'd1'), ('-1', 'd-1'), ('0.5', 'd0.5'),
           ('2.718281828459045', 'd2.718281828459045'), ('100', 'd100'),
           ('1E-10', 'd1e-10'), ('-7.25', 'd-7.25'),
           ('3.141592653589793', 'd3.141592653589793')]
for name in ('rtcSin', 'rtcCos', 'rtcTan', 'rtcAtn', 'rtcExp', 'rtcLog',
             'rtcSqr'):
    experiment(name, 'D', 'd', [(x,) for x in DOUBLES])
VALS = [('"12"', 's"12"'), ('" 12abc"', 's" 12abc"'), ('"&HFF"', 's"&HFF"'),
        ('"1e3"', 's"1e3"'), ('"abc"', 's"abc"'), ('""', 's""'),
        ('"-.5"', 's"-.5"'), ('"1,5"', 's"1,5"'), ('"12 34"', 's"12 34"'),
        ('"&O17"', 's"&O17"'), ('"3.25.5"', 's"3.25.5"'),
        ('"  -42  "', 's"  -42  "')]
experiment('rtcR8ValFromBstr', 'S', 'd', [(v,) for v in VALS])
experiment('rtcMonthName', 'LL', 's',
           [((str(m), 'i%d' % m), (a, 'i' + a)) for m in range(1, 13)
            for a in ('0', '-1')])
CONDS = [('True', 'v11:-1'), ('False', 'v11:0'), ('CLng(1)', 'v3:1'),
         ('CLng(0)', 'v3:0'), ('Null', 'v1'), ('"x"', 'v8"x"')]
experiment('rtcImmediateIf', 'OVVV', 'V',
           [(c, ('"yes"', 'v8"yes"'), ('CLng(7)', 'v3:7')) for c in CONDS])
ROUNDS = [('CDbl(2.5)', 'v5:2.5'), ('CDbl(3.5)', 'v5:3.5'),
          ('CDbl(-2.5)', 'v5:-2.5'), ('CDbl(1.2345)', 'v5:1.2345'),
          ('CDbl(0.285)', 'v5:0.285'), ('CLng(7)', 'v3:7'),
          ('CCur(-3.5)', 'v6:-35000'), ('"1.5"', 'v8"1.5"'),
          ('Null', 'v1'), ('Empty', 'v0')]
experiment('rtcRound', 'OVL', 'V',
           [(v, (c, 'i' + c)) for v in ROUNDS for c in ('0', '1', '2')])
experiment('rtcPartition', 'OVVVV', 'V',
           [(n, ('CLng(0)', 'v3:0'), ('CLng(99)', 'v3:99'),
             ('CLng(10)', 'v3:10')) for n in
            [('CLng(5)', 'v3:5'), ('CLng(15)', 'v3:15'),
             ('CLng(99)', 'v3:99'), ('CLng(-1)', 'v3:-1'),
             ('CLng(100)', 'v3:100'), ('Null', 'v1')]])


# Financial functions: Doubles by value, optional Variants after them (a
# missing one is an error Variant).
EXPERIMENTS_BEFORE_FINANCE = len(EXPERIMENTS)


def dbl(x):
    return (repr(float(x)), 'd' + repr(float(x)))


experiment('rtcSLN', 'DDD', 'd',
           [(dbl(a), dbl(b), dbl(c)) for a, b, c in
            ((10000, 1000, 5), (1000, 0, 10), (1, 2, 3), (5000.5, 500.25, 7),
             (0, 0, 1), (-1000, 100, 4))])
experiment('rtcSYD', 'DDDD', 'd',
           [(dbl(a), dbl(b), dbl(c), dbl(d)) for a, b, c, d in
            ((10000, 1000, 5, 1), (10000, 1000, 5, 2), (10000, 1000, 5, 5),
             (1000, 0, 10, 3), (5000.5, 500.25, 7, 4))])
FACT = [MISSING, ('CDbl(2)', 'v5:2'), ('CDbl(1.5)', 'v5:1.5'),
        ('CLng(3)', 'v3:3')]
experiment('rtcDDB', 'DDDDV', 'd',
           [(dbl(a), dbl(b), dbl(c), dbl(d), f) for a, b, c, d in
            ((10000, 1000, 5, 1), (10000, 1000, 5, 2), (10000, 1000, 5, 5),
             (5000.5, 500.25, 7, 3)) for f in FACT])
FV = [MISSING, ('CDbl(0)', 'v5:0'), ('CDbl(1000)', 'v5:1000'),
      ('CDbl(-5000)', 'v5:-5000')]
TY = [MISSING, ('CLng(0)', 'v3:0'), ('CLng(1)', 'v3:1')]
for name in ('rtcPMT', 'rtcFV', 'rtcPV', 'rtcNPer'):
    experiment(name, 'DDDVV', 'd',
               [(dbl(a), dbl(b), dbl(c), f, t) for a, b, c in
                ((0.05 / 12, 360, 100000), (0.08, 10, -2000),
                 (0.01, 24, 500.5)) for f in FV for t in TY])

# Strings: WeekdayName, Replace, InStrRev, and the Format functions.
experiment('rtcWeekdayName', 'LLL', 's',
           [((str(d), 'i%d' % d), (a, 'i' + a), (f, 'i' + f))
            for d in range(1, 8) for a in ('0', '-1') for f in ('0', '1')])
SRC = [('"banana"', 's"banana"'), ('"aaa"', 's"aaa"'), ('""', 's""')]
PAIRS = [(('"a"', 's"a"'), ('"o"', 's"o"')),
         (('"an"', 's"an"'), ('""', 's""')),
         (('"A"', 's"A"'), ('"x"', 's"x"'))]
experiment('rtcReplace', 'SSSLLL', 's',
           [(x, f, r, (st, 'i' + st), (cn, 'i' + cn), (cm, 'i' + cm))
            for x in SRC for f, r in PAIRS
            for st in ('1', '3') for cn in ('-1', '1') for cm in ('0', '1')])
experiment('rtcInStrRev', 'SSLL', 'i',
           [(x, f, (st, 'i' + st), (cm, 'i' + cm))
            for x in SRC[:2] for f in (('"a"', 's"a"'), ('"A"', 's"A"'),
                                       ('"an"', 's"an"'), ('""', 's""'))
            for st in ('-1', '2', '4', '10') for cm in ('0', '1')])
NUMS = [('CDbl(1234.5678)', 'v5:1234.5678'), ('CDbl(-0.5)', 'v5:-0.5'),
        ('CLng(7)', 'v3:7'), ('CDbl(0)', 'v5:0'),
        ('CDbl(1234567.891)', 'v5:1234567.891'), ('Null', 'v1')]
for name in ('rtcFormatNumber', 'rtcFormatCurrency', 'rtcFormatPercent'):
    experiment(name, 'VLLLL', 's',
               [(n, (dg, 'i' + dg), (ld, 'i' + ld), (pr, 'i' + pr),
                 (gr, 'i' + gr)) for n in NUMS for dg in ('-1', '2')
                for ld in ('-1', '0') for pr in ('-1', '0') for gr in
                ('-1', '0')])
experiment('rtcFormatDateTime', 'VL', 's',
           [(d, (f, 'i' + f)) for d in DATES[:2] + [
               ('"2000-01-01 13:45:10"', 'v8"2000-01-01 13:45:10"')]
            for f in ('0', '1', '2', '3', '4')])

# Date arithmetic: DateAdd(interval, number, date), DateDiff and DatePart.
IVS = [('"%s"' % i, 's"%s"' % i) for i in
       ('yyyy', 'q', 'm', 'y', 'd', 'w', 'ww', 'h', 'n', 's', 'x', '')]
BASES = [('CDate(36526.5)', 'v7:36526.5'), ('CDate(40000.75)', 'v7:40000.75'),
         ('CDate(36585)', 'v7:36585'), ('"2000-01-31 13:45:10"',
                                          'v8"2000-01-31 13:45:10"')]
experiment('rtcDateAdd', 'OSDV', 'V',
           [(iv, ('%s' % n, 'd%s' % n), b) for iv in IVS for n in
            ('1', '-1', '2.7', '13') for b in BASES])
experiment('rtcDateDiff', 'OSVVVV', 'V',
           [(iv, a, b, ('CLng(1)', 'v3:1'), ('CLng(1)', 'v3:1'))
            for iv in IVS for a in BASES[:2] for b in BASES[1:]])
experiment('rtcDatePart', 'OSVVV', 'V',
           [(iv, b, ('CLng(%s)' % f, 'v3:%s' % f), ('CLng(%s)' % w, 'v3:%s' % w))
            for iv in IVS for b in BASES for f in (1, 2) for w in (1, 2)])

# IPMT, PPMT and Rate: Doubles by value, then optional Variants.
FV2 = [('CDbl(0)', 'v5:0'), ('CDbl(1000)', 'v5:1000'),
       ('CDbl(-5000)', 'v5:-5000')]
TY2 = [('CLng(0)', 'v3:0'), ('CLng(1)', 'v3:1')]
for name in ('rtcIPMT', 'rtcPPMT'):
    experiment(name, 'DDDDVV', 'd',
               [(dbl(r), dbl(p), dbl(n), dbl(v), f, t)
                for r, n, v in ((0.05 / 12, 360, 100000), (0.08, 10, -2000),
                                (0.01, 24, 500.5))
                for p in (1, 2, 10) for f in FV2 for t in TY2])
experiment('rtcRate', 'DDDVVV', 'd',
           [(dbl(n), dbl(m), dbl(v), f, t, g)
            for n, m, v in ((360, -537, 100000), (10, -300, 2000),
                            (24, -25, 500.5))
            for f in FV2[:2] for t in TY2
            for g in (('CDbl(0.1)', 'v5:0.1'), ('CDbl(0.5)', 'v5:0.5'))])

# Arrays of Strings: Split, Join and Filter.
def sarr(items):
    """A Variant holding a String array and its case spelling."""
    if items is None:
        return ('Split("")', 'v8200:0""')
    text = '|'.join(items)
    return ('Split("%s", "|")' % text, 'v8200:%d"%s"' % (len(items), text))


SPLIT_TEXT = [('"a,b,,c"', 's"a,b,,c"'), ('"abc"', 's"abc"'),
              ('""', 's""'), ('"A,a,B"', 's"A,a,B"'),
              ('",a,"', 's",a,"')]
SPLIT_DELIM = [('","', 'v8","'), ('"a"', 'v8"a"'), ('""', 'v8""'),
               ('CLng(5)', 'v3:5'), MISSING]
experiment('rtcSplit', 'OSVLL', 'V',
           [(t, d, (lm, 'i' + lm), (cm, 'i' + cm))
            for t in SPLIT_TEXT for d in SPLIT_DELIM
            for lm in ('-1', '2', '0') for cm in ('0', '1')])
JOIN_ARRAYS = [sarr(['a', 'b', 'c']), sarr(['x']), sarr(['', '']),
               sarr(None)]
JOIN_DELIM = [('","', 'v8","'), ('""', 'v8""'), ('CLng(5)', 'v3:5'),
              ('Null', 'v1'), MISSING]
experiment('rtcJoin', 'VV', 's',
           [(a, d) for a in JOIN_ARRAYS for d in JOIN_DELIM])
FILTER_ARRAYS = [sarr(['apple', 'banana', 'Apricot', 'cherry'])]
experiment('rtcFilter', 'OVSLL', 'V',
           [(a, (m, 's' + m), (inc, 'i' + inc), (cm, 'i' + cm))
            for a in FILTER_ARRAYS for m in ('"ap"', '"AP"', '"an"', '"zz"',
                                             '""')
            for inc in ('-1', '0') for cm in ('0', '1')])

# NPV, IRR, MIRR, Choose, Switch and Array: arrays passed by reference.
STREAMS = [['-1000', '300', '400', '500'], ['-500', '-100', '200'],
           ['100', '200', '300'], ['-1000', '-1000', '2500'],
           ['-70000', '12000', '15000', '18000', '21000', '26000']]
experiment('rtcNPV', 'DA', 'd',
           [(dbl(r), (a, a)) for r in (0.1, 0.0, 0.05, -0.5)
            for a in STREAMS])
GUESS = [MISSING, ('CDbl(0.1)', 'v5:0.1'), ('CDbl(-0.5)', 'v5:-0.5')]
experiment('rtcIRR', 'AV', 'd',
           [((a, a), g) for a in STREAMS for g in GUESS], isolate=True)
experiment('rtcMIRR', 'ADD', 'd',
           [((a, a), dbl(f), dbl(r)) for a in STREAMS
            for f, r in ((0.1, 0.12), (0.05, 0.05), (0.0, 0.1))])
CHOICES = [('"a"', 'v8"a"'), ('CLng(7)', 'v3:7'), ('Null', 'v1')]
experiment('rtcChoose', 'OFP', 'V',
           [((str(i), 'f%s' % i), CHOICES) for i in
            ('0', '1', '2', '3', '4', '1.5', '2.5', '-1')])
SW = [('True', 'v11:-1'), ('False', 'v11:0')]
experiment('rtcSwitch', 'OP', 'V',
           [([a, ('"x"', 'v8"x"'), b, ('"y"', 'v8"y"')],) for a in SW
            for b in SW] + [([('False', 'v11:0'), ('"x"', 'v8"x"')],),
                            ([('Null', 'v1'), ('"x"', 'v8"x"'),
                              ('True', 'v11:-1'), ('"y"', 'v8"y"')],)])
experiment('rtcArray', 'OP', 'V',
           [([],), ([('"a"', 'v8"a"')],),
            ([('CLng(1)', 'v3:1'), ('"b"', 'v8"b"'), ('Null', 'v1')],)])

# Error text, CVErr, Environ, IsMissing and FreeFile.
ERRORS = ['0', '1', '3', '5', '6', '7', '9', '11', '13', '53', '57', '58',
          '70', '76', '91', '94', '380', '429', '438', '1000', '32767',
          '65535', '-1', '40000']
experiment('rtcBstrFromError', 'V', 's',
           [(('CLng(%s)' % e, 'v3:' + e),) for e in ERRORS], isolate=True)
experiment('rtcVarFromError', 'OV', 'V',
           [(('CLng(%s)' % e, 'v3:' + e),) for e in ERRORS], isolate=True)
NAMES = [('"OS"', 'v8"OS"'), ('"os"', 'v8"os"'),
         ('"NO_SUCH_VARIABLE_X"', 'v8"NO_SUCH_VARIABLE_X"'),
         ('""', 'v8""'), ('"OS="', 'v8"OS="'), ('CLng(0)', 'v3:0'),
         ('Null', 'v1')]
experiment('rtcEnvironBstr', 'V', 's', [(n,) for n in NAMES], isolate=True)
experiment('rtcEnvironVar', 'OV', 'V', [(n,) for n in NAMES], isolate=True)
experiment('rtcIsMissing', 'V', 'w',
           [(v,) for v in [MISSING, ('CLng(1)', 'v3:1'), ('Null', 'v1'),
                           ('Empty', 'v0'), ('"x"', 'v8"x"')]])
experiment('rtcFreeFile', 'V', 'w',
           [(v,) for v in [('CLng(0)', 'v3:0'), ('CLng(1)', 'v3:1'),
                           ('CLng(2)', 'v3:2'), MISSING]], isolate=True)

# The file system: every run starts from the same scratch folder (Fixture in
# SUPPORT) and the program works inside it, so the names are relative.
# MUTATORS change the folder: their result is the state of the folder after
# the call (State in SUPPORT; result letter n).
def esc(name):
    return name.replace(chr(92), chr(92) * 2)


MUTATORS = {'rtcKillFiles', 'rtcChangeDir', 'rtcMakeDir', 'rtcRemoveDir',
            'rtcFileCopy', 'rtcSetFileAttr'}
FILES = ['a.txt', 'b.txt', 'c.txt', 'empty.txt', 'ro.txt', 'hid.txt', 'sub',
         'emptydir', 'sub\\in.txt', 'missing.txt', 'A.TXT', 'nodir\\x.txt',
         '', '.', '..', '*.txt', 'a.tx?', 'a.txt ', ' a.txt', 'sub\\',
         'sub\\.']
experiment('rtcFileLen', 'S', 'i',
           [(('"%s"' % f, 's"%s"' % esc(f)),) for f in FILES])
experiment('rtcGetFileAttr', 'S', 'i',
           [(('"%s"' % f, 's"%s"' % esc(f)),) for f in FILES])
experiment('rtcFileDateTime', 'OS', 'V',
           [(('"%s"' % f, 's"%s"' % esc(f)),) for f in FILES])
experiment('rtcSetFileAttr', 'SL', 'n',
           [(('"%s"' % f, 's"%s"' % esc(f)), (a, 'i' + a))
            for f in ('a.txt', 'ro.txt', 'hid.txt', 'sub', 'missing.txt')
            for a in ('0', '1', '2', '4', '7', '32', '33', '16', '17', '8')],
           isolate=True)
experiment('rtcMakeDir', 'S', 'n',
           [(('"%s"' % f, 's"%s"' % esc(f)),) for f in
            ('newdir', 'sub', 'a.txt', 'nodir\\x', 'newdir\\', 'Sub', '',
             'sub\\inner', 'a b', 'x.y')], isolate=True)
experiment('rtcRemoveDir', 'S', 'n',
           [(('"%s"' % f, 's"%s"' % esc(f)),) for f in
            ('emptydir', 'sub', 'a.txt', 'missing', 'EMPTYDIR', '', '.',
             'emptydir\\')], isolate=True)
experiment('rtcChangeDir', 'S', 'n',
           [(('"%s"' % f, 's"%s"' % esc(f)),) for f in
            ('sub', 'emptydir', 'a.txt', 'missing', '.', 'SUB', '',
             'sub\\..', 'sub\\')], isolate=True)
experiment('rtcKillFiles', 'V', 'n',
           [(('"%s"' % f, 'v8"%s"' % esc(f)),) for f in
            ('a.txt', '*.txt', 'ro.txt', 'hid.txt', 'missing.txt', 'sub',
             'sub\\*', 'sub\\in.txt', 'a.tx?', '*', '', 'A.TXT',
             'emptydir', '*.*', 'e*', 'nodir\\x')] +
           [(('Null', 'v1'),), (('CLng(5)', 'v3:5'),)], isolate=True)
experiment('rtcFileCopy', 'SS', 'n',
           [(('"%s"' % f, 's"%s"' % esc(f)), ('"%s"' % t, 's"%s"' % esc(t)))
            for f, t in (('a.txt', 'copy.txt'), ('a.txt', 'b.txt'),
                         ('missing.txt', 'x.txt'), ('a.txt', 'a.txt'),
                         ('a.txt', 'sub\\a.txt'), ('ro.txt', 'rocopy.txt'),
                         ('a.txt', 'emptydir'), ('sub', 'subcopy'),
                         ('a.txt', 'ro.txt'), ('a.txt', 'nodir\\a.txt'),
                         ('empty.txt', 'e2.txt'), ('a.txt', 'A.TXT'),
                         ('hid.txt', 'hidcopy.txt'), ('', 'x.txt'),
                         ('a.txt', ''), ('*.txt', 'all.txt'))],
           isolate=True)
CURDIRS = [MISSING, ('""', 'v8""'), ('Null', 'v1'), ('CLng(5)', 'v3:5')]
experiment('rtcCurrentDirBstr', 'V', 'r', [(d,) for d in CURDIRS],
           isolate=True)
experiment('rtcCurrentDir', 'OV', 'R', [(d,) for d in CURDIRS],
           isolate=True)
# Dir keeps its place between calls: a sequence of calls in one run.
GO = MISSING
for pattern, attr in (('*.txt', '0'), ('*', '0'), ('*', '16'), ('*', '2'),
                      ('*', '7'), ('a.txt', '0'), ('sub', '0'),
                      ('sub', '16'), ('SUB', '16'), ('*.TXT', '0'),
                      ('sub\\*', '0'), ('missing', '0'), ('', '0'),
                      ('a.tx?', '0'), ('*', '32'), ('*', '1'), ('*', '4'),
                      ('*.*', '0'), ('nodir\\*', '0'), ('sub\\', '0'),
                      ('emptydir\\*', '16'), ('*', '23'), ('*', '55')):
    experiment('rtcDir', 'VL', 's',
               [(('"%s"' % pattern, 'v8"%s"' % esc(pattern)), (attr, 'i' + attr))] +
               [(GO, ('0', 'i0'))] * 9)
experiment('rtcDir', 'VL', 's', [(GO, ('0', 'i0'))] * 3)


# Experiments written by hand (name: the Visual Basic statements; each
# `Rec` line becomes a case). They compare Visual Basic's own Dir$ with the
# export.
RAW = {
    'rtcDirNative': '''    rs(0) = Dir$("*.txt")
    rs(1) = Dir$
    rs(2) = Dir$
    rs(3) = Dir$
    rs(4) = Dir$
    rs(5) = Dir$
    Rec "native => " & rs(0) & "," & rs(1) & "," & rs(2) & "," & rs(3) & "," & rs(4) & "," & rs(5)
''',
    'rtcDirMix': '''    v1 = "*.txt"
    p = RawDir(v1, 0)
    rs(0) = BstrFmt(p)
    rs(1) = Dir$
    rs(2) = Dir$
    rs(3) = Dir$
    Rec "mix => " & rs(0) & "," & rs(1) & "," & rs(2) & "," & rs(3)
''',
    'rtcDirMix2': '''    rs(0) = Dir$("*.txt")
    v1 = Miss()
    p = RawDir(v1, 0)
    rs(1) = BstrFmt(p)
    p = RawDir(v1, 0)
    rs(2) = BstrFmt(p)
    rs(3) = Dir$
    Rec "mix2 => " & rs(0) & "," & rs(1) & "," & rs(2) & "," & rs(3)
''',
}
experiment('rtcDirNative', 'x', 'n', [()])
experiment('rtcDirMix', 'x', 'n', [()])
experiment('rtcDirMix2', 'x', 'n', [()])

# The scalar conversions of the compiler: Variant to number, number to text,
# and the String comparison helpers. (Signatures read from the disassembly;
# each is its own experiment so a wrong one only fails its own run.)
SCALARS = VARIANTS + [('CDbl(2.5)', 'v5:2.5'), ('CDbl(-2.5)', 'v5:-2.5'),
                      ('CDbl(1E10)', 'v5:1e10'), ('CDbl(300.7)', 'v5:300.7'),
                      ('"3.7"', 'v8"3.7"'), ('"&H1F"', 'v8"&H1F"'),
                      ('False', 'v11:0'), ('CLng(70000)', 'v3:70000'),
                      ('CByte(255)', 'v17:255')]
for name, result in (('__vbaI2Var', 'w'), ('__vbaI4Var', 'i'),
                     ('__vbaUI1Var', 'i'), ('__vbaR4Var', 'f'),
                     ('__vbaR8Var', 'd'), ('__vbaBoolVar', 'w'),
                     ('__vbaDateVar', 'd'), ('__vbaCyVar', 'c')):
    experiment(name, 'V', result, [(v,) for v in SCALARS])
for name in ('__vbaStrCat', '__vbaStrCmp', '__vbaStrTextCmp',
             '__vbaStrLike', '__vbaStrTextLike'):
    pairs = [(a, b) for a in TEXTS for b in TEXTS]
    if name.endswith('Like'):
        pairs = [(a, b) for a in TEXTS[:2] + [('"hello"', 's"hello"')]
                 for b in [('"h*"', 's"h*"'), ('"?ello"', 's"?ello"'),
                           ('"[a-h]*"', 's"[a-h]*"'), ('"H*"', 's"H*"'),
                           ('""', 's""'), ('"#"', 's"#"')]]
    experiment(name, 'SS', 's' if name == '__vbaStrCat' else 'i', pairs)
experiment('__vbaStrComp', 'LSS', 'i',
           [((m, 'i' + m), a, b) for m in ('0', '1', '2')
            for a in TEXTS[3:5] for b in TEXTS[4:]])
experiment('__vbaStrI2', 'L', 's', [((n, 'i' + n),) for n in
                                    ('0', '5', '-7', '32767', '-32768')])
experiment('__vbaStrBool', 'L', 's', [((n, 'i' + n),) for n in
                                      ('0', '-1', '1')])
experiment('__vbaStrR4', 'F', 's', [(f,) for f in
                                    [('0', 'f0'), ('1.5', 'f1.5'),
                                     ('-2.25', 'f-2.25'), ('1E10', 'f1e10'),
                                     ('0.1', 'f0.1'), ('1E-5', 'f1e-5')]])
experiment('__vbaStrR8', 'D', 's', [(d,) for d in DOUBLES + [
    ('1E+20', 'd1e20'), ('0.1', 'd0.1'), ('123456789.123', 'd123456789.123')]])
experiment('__vbaStrDate', 'D', 's', [(d,) for d in [
    ('36526.5', 'd36526.5'), ('0', 'd0'), ('36526', 'd36526'),
    ('0.75', 'd0.75'), ('40000.25', 'd40000.25')]])
experiment('__vbaR8Str', 'S', 'd', [(v,) for v in VALS])
experiment('__vbaDateStr', 'S', 'd', [(v,) for v in VALS + [
    ('"2000-01-01 13:45:10"', 's"2000-01-01 13:45:10"')]])
experiment('__vbaVarCat', 'OVV', 'V',
           [(a, b) for a in SCALARS[:7] + SCALARS[8:10]
            for b in SCALARS[:3] + SCALARS[5:7] + SCALARS[8:9]])

# Currency arithmetic and conversions (a Currency by value is 8 bytes).
CYS = [('12.3456', 'y123456'), ('-3.5', 'y-35000'), ('0', 'y0'),
       ('0.0001', 'y1'), ('100', 'y1000000'), ('-0.5', 'y-5000'),
       ('7.9999', 'y79999'), ('2.5', 'y25000')]
for name in ('__vbaCyAdd', '__vbaCySub', '__vbaCyMul'):
    experiment(name, 'YY', 'c', [(a, b) for a in CYS for b in CYS[:6]])
for name in ('__vbaCyAbs', '__vbaCyFix', '__vbaCyInt'):
    experiment(name, 'Y', 'c', [(a,) for a in CYS])
experiment('__vbaCySgn', 'Y', 'c', [(a,) for a in CYS])
experiment('__vbaCySgn', 'Y', 'i', [(a,) for a in CYS])
experiment('__vbaCyMulI2', 'YL', 'c',
           [(a, (n, 'i' + n)) for a in CYS for n in ('0', '3', '-2')])
for name in ('__vbaCyI2', '__vbaCyI4', '__vbaCyUI1'):
    experiment(name, 'L', 'c', [((n, 'i' + n),) for n in
                                ('0', '5', '-7', '200')])
experiment('__vbaCyStr', 'S', 'c', [(v,) for v in VALS])
experiment('__vbaStrCy', 'Y', 's', [(a,) for a in CYS])
for name, result in (('__vbaI2Cy', 'w'), ('__vbaI4Cy', 'i'),
                     ('__vbaUI1Cy', 'i'), ('__vbaR4Cy', 'f'),
                     ('__vbaR8Cy', 'd')):
    experiment(name, 'Y', result, [(a,) for a in CYS])


def vb_call(kind, index, row):
    """The Visual Basic statements of one grid row; returns (code, case)."""
    code = []
    case = []
    arguments = []
    cursor = 0
    for letter in kind:
        if letter == 'O':
            arguments.append('r')
            case.append('o')
            continue
        item = row[cursor]
        cursor += 1
        if letter == 'V':
            code.append('    v%d = %s' % (cursor, item[0]))
            arguments.append('v%d' % cursor)
            case.append(item[1])
        elif letter == 'S':
            arguments.append('StrPtr(%s)' % item[0])
            case.append(item[1])
        elif letter == 'D':
            arguments.append('CDbl(%s)' % item[0])
            case.append(item[1])
        elif letter == 'F':
            arguments.append('CSng(%s)' % item[0])
            case.append(item[1])
        elif letter == 'Y':
            arguments.append('CCur(%s)' % item[0])
            case.append(item[1])
        elif letter == 'A':
            numbers = item[0]
            code.append('    ReDim da%d(0 To %d)' % (cursor, len(numbers) - 1))
            for place, number in enumerate(numbers):
                code.append('    da%d(%d) = %s' % (cursor, place, number))
            arguments.append('VarPtrArray(da%d)' % cursor)
            case.append('a:' + ','.join(numbers))
        elif letter == 'P':
            members = item
            code.append('    ReDim pa%d(0 To %d)' % (cursor, len(members) - 1))
            for place, member in enumerate(members):
                code.append('    pa%d(%d) = %s' % (cursor, place, member[0]))
            arguments.append('VarPtrArray(pa%d)' % cursor)
            case.append('p[' + ' '.join(m[1] for m in members) + ']')
        else:
            arguments.append('CLng(%s)' % item[0])
            case.append(item[1])
    return code, arguments, case


def declare(number, export, kind, result):
    parameters = []
    for place, letter in enumerate(kind):
        if letter in 'OV':
            parameters.append('ByRef a%d As Variant' % place)
        elif letter == 'D':
            parameters.append('ByVal a%d As Double' % place)
        elif letter == 'F':
            parameters.append('ByVal a%d As Single' % place)
        elif letter == 'Y':
            parameters.append('ByVal a%d As Currency' % place)
        else:
            parameters.append('ByVal a%d As Long' % place)
    if kind.startswith('O'):
        ret = 'Long'
    else:
        ret = {'s': 'Long', 'i': 'Long', 'w': 'Integer',
               'd': 'Double', 'f': 'Single',
               'c': 'Currency'}.get(result, 'Long')
    return ('Declare Function E%d_%s Lib "msvbvm60.dll" Alias "%s" (%s) '
            'As %s' % (number, export, export, ', '.join(parameters), ret))


PROLOGUE = r'''Attribute VB_Name = "Hmod"
Option Explicit
Declare Sub CopyMemory Lib "kernel32" Alias "RtlMoveMemory" (Dest As Any, Src As Any, ByVal n As Long)
Declare Function VarPtrArray Lib "msvbvm60.dll" Alias "VarPtr" (Var() As Any) As Long
Declare Function CreateFileA Lib "kernel32" (ByVal n As String, ByVal acc As Long, ByVal sh As Long, ByVal sa As Long, ByVal disp As Long, ByVal fl As Long, ByVal t As Long) As Long
Declare Function SetFileTime Lib "kernel32" (ByVal h As Long, c As Any, a As Any, w As Any) As Long
Declare Function CloseHandle Lib "kernel32" (ByVal h As Long) As Long
Declare Function SystemTimeToFileTime Lib "kernel32" (s As Any, f As Any) As Long
Declare Function LocalFileTimeToFileTime Lib "kernel32" (a As Any, b As Any) As Long
Declare Function RawDir Lib "msvbvm60.dll" Alias "rtcDir" (ByRef a0 As Variant, ByVal a1 As Long) As Long
'''

SUPPORT = r'''
Private out As Integer

Private Function Esc(ByVal c As Long) As String
    If c = 92 Then
        Esc = "\\"
    ElseIf c = 34 Then
        Esc = "\" & Chr$(34)
    ElseIf c < 32 Or c > 126 Then
        Esc = "\u" & Right$("0000" & Hex$(c And &HFFFF&), 4)
    Else
        Esc = Chr$(c)
    End If
End Function

Private Function EscText(ByVal s As String) As String
    Dim i As Long
    For i = 1 To Len(s)
        EscText = EscText & Esc(AscW(Mid$(s, i, 1)))
    Next
End Function

Private Function HexOf(ByRef v As Variant, ByVal bytes As Long) As String
    Dim b(0 To 7) As Byte, i As Long, s As String
    CopyMemory b(0), ByVal VarPtr(v) + 8, bytes
    For i = bytes - 1 To 0 Step -1
        s = s & Right$("0" & Hex$(b(i)), 2)
    Next
    HexOf = s
End Function

Private Function Dec64(ByRef v As Variant) As String
    Dim lo As Long, hi As Long, c As Currency
    c = v
    Dec64 = CStr(c * 10000@)
End Function

Private Function FmtArr(ByRef v As Variant) As String
    Dim i As Long, s As String, lo As Long, hi As Long
    lo = LBound(v)
    hi = UBound(v)
    For i = lo To hi
        If i > lo Then s = s & "|"
        s = s & EscText(CStr(v(i)))
    Next
    FmtArr = "v8200:" & CStr(hi - lo + 1) & """" & s & """"
End Function

Public Function Fmt(ByRef v As Variant) As String
    Dim t As Long
    t = VarType(v)
    Select Case t
    Case 0: Fmt = "v0"
    Case 1: Fmt = "v1"
    Case 10: Fmt = "v10:x" & HexOf(v, 4)
    Case 2, 3, 11: Fmt = "v" & t & ":" & CStr(CLng(v))
    Case 17: Fmt = "v17:" & CStr(CLng(v))
    Case 4: Fmt = "v4:x" & HexOf(v, 4)
    Case 5, 7: Fmt = "v" & t & ":x" & HexOf(v, 8)
    Case 6: Fmt = "v6:" & Dec64(v)
    Case 8: Fmt = "v8""" & EscText(CStr(v)) & """"
    Case 8200: Fmt = FmtArr(v)
    Case Else: Fmt = "v" & t & ":?"
    End Select
End Function

Private Function HexD(ByVal d As Double) As String
    Dim b(0 To 7) As Byte, i As Long, s As String
    CopyMemory b(0), d, 8
    For i = 7 To 0 Step -1
        s = s & Right$("0" & Hex$(b(i)), 2)
    Next
    HexD = s
End Function

Private Function BstrFmt(ByVal p As Long) As String
    Dim n As Long, i As Long, b() As Byte, u As Long, s As String
    If p = 0 Then
        BstrFmt = "s0"
        Exit Function
    End If
    CopyMemory n, ByVal p - 4, 4
    s = "s"""
    If n > 0 Then
        ReDim b(0 To n - 1)
        CopyMemory b(0), ByVal p, n
        For i = 0 To n - 2 Step 2
            s = s & Esc(b(i) + 256& * b(i + 1))
        Next
        If n Mod 2 = 1 Then s = s & Esc(b(n - 1))
    End If
    s = s & """"
    If n Mod 2 = 1 Then s = s & "+1"
    BstrFmt = s
End Function

Private Function Root() As String
    Root = Environ$("TEMP") & "\wfcvbrunfx" & Command$
End Function

Private Sub Wipe(ByVal d As String)
    Dim names() As String, n As Long, nm As String, i As Long
    ReDim names(0 To 199)
    On Error Resume Next
    nm = Dir$(d & "\*", 55)
    Do While nm <> ""
        If nm <> "." And nm <> ".." Then
            names(n) = nm
            n = n + 1
        End If
        nm = Dir$
    Loop
    For i = 0 To n - 1
        SetAttr d & "\" & names(i), 0
        If (GetAttr(d & "\" & names(i)) And 16) <> 0 Then
            Wipe d & "\" & names(i)
            RmDir d & "\" & names(i)
        Else
            Kill d & "\" & names(i)
        End If
    Next
End Sub

Private Sub PutFile(ByVal nm As String, ByVal txt As String)
    Dim f As Integer
    f = FreeFile
    Open nm For Binary As #f
    If Len(txt) > 0 Then Put #f, , txt
    Close #f
End Sub

Private Sub Stamp(ByVal nm As String, ByVal y As Long, ByVal mo As Long, ByVal d As Long, ByVal h As Long, ByVal mi As Long, ByVal s As Long)
    Dim st(0 To 7) As Integer, l(0 To 1) As Long, f(0 To 1) As Long, hh As Long
    st(0) = y: st(1) = mo: st(2) = 0: st(3) = d: st(4) = h: st(5) = mi: st(6) = s: st(7) = 0
    SystemTimeToFileTime st(0), l(0)
    LocalFileTimeToFileTime l(0), f(0)
    hh = CreateFileA(nm, &H100, 7, 0, 3, &H2000000, 0)
    If hh <> -1 Then
        SetFileTime hh, ByVal 0&, ByVal 0&, f(0)
        CloseHandle hh
    End If
End Sub

' The scratch folder that every run starts from (the C++ runner builds the
' same one).
Private Sub Fixture()
    Dim r As String
    r = Root()
    On Error Resume Next
    Wipe r
    MkDir r
    On Error GoTo 0
    ChDir r
    PutFile "a.txt", "hello"
    PutFile "b.txt", "abcdefghij"
    PutFile "c.txt", "ccc"
    PutFile "empty.txt", ""
    PutFile "ro.txt", "abc"
    PutFile "hid.txt", "x"
    On Error Resume Next
    MkDir "sub"
    On Error GoTo 0
    PutFile "sub\in.txt", "in"
    MkDir "emptydir"
    Stamp "a.txt", 2000, 1, 1, 13, 45, 10
    Stamp "c.txt", 2000, 1, 1, 13, 45, 11
    Stamp "b.txt", 2001, 2, 3, 4, 5, 6
    Stamp "empty.txt", 2001, 2, 3, 4, 5, 6
    Stamp "ro.txt", 2001, 2, 3, 4, 5, 6
    Stamp "hid.txt", 2001, 2, 3, 4, 5, 6
    Stamp "sub\in.txt", 2001, 2, 3, 4, 5, 6
    Stamp "sub", 2001, 2, 3, 4, 5, 6
    Stamp "emptydir", 2001, 2, 3, 4, 5, 6
    SetAttr "ro.txt", vbReadOnly
    SetAttr "hid.txt", vbHidden
End Sub

Private Function Tree(ByVal d As String) As String
    Dim names() As String, n As Long, nm As String, i As Long, j As Long
    Dim t As String, a As Long, k As String
    ReDim names(0 To 199)
    nm = Dir$(d & "*", 55)
    Do While nm <> ""
        If nm <> "." And nm <> ".." Then
            names(n) = nm
            n = n + 1
        End If
        nm = Dir$
    Loop
    For i = 0 To n - 2
        For j = i + 1 To n - 1
            If UCase$(names(j)) < UCase$(names(i)) Then
                k = names(i)
                names(i) = names(j)
                names(j) = k
            End If
        Next
    Next
    For i = 0 To n - 1
        a = GetAttr(d & names(i)) And 55
        If (a And 16) <> 0 Then
            t = t & names(i) & ":" & a & ":0{" & Tree(d & names(i) & "\") & "};"
        Else
            t = t & names(i) & ":" & a & ":" & FileLen(d & names(i)) & ";"
        End If
    Next
    Tree = t
End Function

Private Function State() As String
    Dim c As String, r As String, w As String
    c = CurDir$
    r = Root()
    If LCase$(c) = LCase$(r) Then
        w = "."
    ElseIf LCase$(Left$(c, Len(r) + 1)) = LCase$(r & "\") Then
        w = Mid$(c, Len(r) + 2)
    Else
        w = "?" & c
    End If
    State = "cwd=" & w & " " & Tree("")
End Function

Private Function RootFmt(ByVal p As Long) As String
    Dim n As Long, i As Long, t As String, s As String
    If p = 0 Then
        RootFmt = "s0"
        Exit Function
    End If
    CopyMemory n, ByVal p - 4, 4
    t = Space$(n \ 2)
    If n > 0 Then CopyMemory ByVal StrPtr(t), ByVal p, n
    t = Replace(t, Root(), "<R>", , , vbTextCompare)
    s = "s"""
    For i = 1 To Len(t)
        s = s & Esc(AscW(Mid$(t, i, 1)))
    Next
    RootFmt = s & """"
End Function

Private Function Miss(Optional m As Variant) As Variant
    Miss = m
End Function

Private Sub Rec(ByVal s As String)
    Print #out, s
End Sub

Private Sub Trace(ByVal s As String)
    Dim t As Integer
    t = FreeFile
    Open App.Path & Chr$(92) & "trace" & Command$ & ".txt" For Append As #t
    Print #t, s
    Close #t
End Sub
'''


def build(folder):
    os.makedirs(folder, exist_ok=True)
    text = PROLOGUE
    for number, (export, kind, result, grid) in enumerate(EXPERIMENTS):
        text += declare(number, export, kind, result) + '\n'
    text += (SUPPORT.replace('Private Function', 'Public Function')
             .replace('Private Sub', 'Public Sub')
             .replace('Private out', 'Public out'))
    body = ('Public Sub Main()\n    Dim v1 As Variant, v2 As Variant, '
            'v3 As Variant, v4 As Variant, r As Variant, p As Long\n'
            '    out = FreeFile\n'
            '    Open App.Path & Chr$(92) & "vh" & Command$ & ".txt" For '
            'Output As #out\n    Select Case Val(Command$)\n')
    plan = []
    for number, (export, kind, result, grid) in enumerate(EXPERIMENTS):
        body += '    Case %d\n        Run%d\n' % (number, number)
    body += '    End Select\n    Close #out\nEnd Sub\n'
    subs = ''
    for number, (export, kind, result, grid) in enumerate(EXPERIMENTS):
        subs += '\nPrivate Sub Run%d()\n' % number
        subs += '    Dim v1 As Variant, v2 As Variant, v3 As Variant, '
        subs += 'v4 As Variant, v5 As Variant, v6 As Variant, r As Variant, p As Long, q As Double, cy As Currency\n'
        subs += '    Dim da1() As Double, da2() As Double, da3() As Double\n'
        subs += '    Dim pa1() As Variant, pa2() As Variant, pa3() As Variant\n'
        subs += '    Dim rs(0 To 99) As String\n'
        subs += '    Fixture\n'
        subs += '    On Error Resume Next\n'
        # Dir keeps its place between calls: nothing else may run between
        # them (no file output), so the results are written afterwards.
        deferred = export == 'rtcDir'
        labels = []
        if export in RAW:
            subs += RAW[export]
            grid = []
        for row in grid:
            code, arguments, case = vb_call(kind, number, row)
            label = '%s %s' % (export, ' '.join(case))
            subs += '\n'.join(code) + '\n' if code else ''
            if deferred:
                call = 'E%d_%s(%s)' % (number, export, ', '.join(arguments))
                subs += '    Err.Clear\n    p = %s\n' % call
                subs += '    If Err.Number <> 0 Then\n'
                subs += '        rs(%d) = "err" & Err.Number\n' % len(labels)
                subs += '    Else\n'
                subs += '        rs(%d) = BstrFmt(p)\n    End If\n' % len(labels)
                labels.append(label)
                continue
            subs += '    Trace "%s"\n' % label.replace('"', '""')
            subs += '    r = Empty\n    Err.Clear\n'
            call = 'E%d_%s(%s)' % (number, export, ', '.join(arguments))
            subs += '    %s = %s\n' % (
                'q' if result in ('d', 'f') else 'cy' if result == 'c'
                else 'p', call)
            subs += '    If Err.Number <> 0 Then\n'
            subs += '        Rec "%s => err" & Err.Number\n' % label.replace(
                '"', '""')
            subs += '    Else\n'
            if result == 'n':
                subs += '        Rec "%s => " & State()\n' % label.replace(
                    '"', '""')
            elif result == 'r':
                subs += '        Rec "%s => " & RootFmt(p)\n' % label.replace(
                    '"', '""')
            elif result == 'R':
                subs += ('        Rec "%s => vt" & VarType(r) & " " & '
                         'RootFmt(StrPtr(CStr(r)))\n' %
                         label.replace('"', '""'))
            elif result == 'V':
                subs += '        Rec "%s => " & Fmt(r)\n' % label.replace(
                    '"', '""')
            elif result == 'c':
                subs += ('        Rec "%s => c" & CStr(CDec(cy) * 10000)'
                         + chr(10)) % label.replace('"', '""')
            elif result in ('d', 'f'):
                subs += '        Rec "%s => d:x" & HexD(q)\n' % label.replace(
                    '"', '""')
            elif result == 's':
                subs += '        Rec "%s => " & BstrFmt(p)\n' % label.replace(
                    '"', '""')
            elif result == 'w':
                subs += '        Rec "%s => w" & CStr(p)\n' % label.replace(
                    '"', '""')
            elif result == 'i':
                subs += '        Rec "%s => i" & CStr(p)\n' % label.replace(
                    '"', '""')
            else:
                subs += '        Rec "%s => " & CStr(p)\n' % label.replace(
                    '"', '""')
            subs += '    End If\n'
        for place, label in enumerate(labels):
            subs += '    Rec "%s => " & rs(%d)\n' % (label.replace('"', '""'),
                                                    place)
        subs += 'End Sub\n'
    text += '\n' + body
    parts = subs.split('\nPrivate Sub Run')
    pieces = ['\nPublic Sub Run' + part for part in parts[1:]]
    modules = []
    current = ''
    for piece in pieces:
        if current and current.count('\n') + piece.count('\n') > 12000:
            modules.append(current)
            current = ''
        current += piece
    if current:
        modules.append(current)
    vbp = 'Type=Exe\r\nStartup="Sub Main"\r\nModule=Hmod; Hmod.bas\r\n'
    for place, code in enumerate(modules):
        name = 'Hr%d' % place
        header = 'Attribute VB_Name = "%s"\nOption Explicit\n' % name
        open(os.path.join(folder, name + '.bas'), 'w', newline='').write(
            (header + code).replace('\n', '\r\n'))
        vbp += 'Module=%s; %s.bas\r\n' % (name, name)
    open(os.path.join(folder, 'Hmod.bas'), 'w', newline='').write(
        text.replace('\n', '\r\n'))
    open(os.path.join(folder, 'Harness.vbp'), 'w', newline='').write(
        vbp + 'Name="Harness"\r\n')
    open(os.path.join(folder, 'experiments.txt'), 'w').write(
        '\n'.join('%d %s %s %s' % (n, e[0], e[1], e[2])
                  for n, e in enumerate(EXPERIMENTS)) + '\n')


if __name__ == '__main__':
    build(sys.argv[1])
