"""Runs the harness of vb6_harness_gen.py and writes the case files.

Usage: python vb6_harness_cases.py <folder with Harness.exe> <family> <cases>

Starts Harness.exe once for every experiment (listed in experiments.txt next
to it), so an export called with a wrong signature only costs its own run, and
writes <cases>/<family>.cases and <cases>/<family>.expected from the lines
that were measured. A call that raised an error is not a case: the errors
are printed.
"""

import collections
import os
import subprocess
import sys


def main(folder, family, cases):
    experiments = [line.split() for line in open(
        os.path.join(folder, 'experiments.txt')).read().splitlines()]
    lines = []
    errors = collections.Counter()
    crashed = []
    for number, export, kind, result in experiments:
        target = os.path.join(folder, 'vh%s.txt' % number)
        if os.path.exists(target):
            os.remove(target)
        try:
            subprocess.run([os.path.join(folder, 'Harness.exe'), number],
                           timeout=8, cwd=folder)
        except subprocess.TimeoutExpired:
            # A fault leaves the error-reporting dialog open.
            subprocess.run(['taskkill', '/F', '/IM', 'WerFault.exe'],
                           capture_output=True)
            crashed.append(export)
            continue
        if (not os.path.exists(target) or
                os.path.getsize(target) == 0):
            crashed.append(export)
            continue
        measured = open(target, encoding='utf-8', errors='replace').read()
        for line in measured.replace('\r', '').splitlines():
            head, _, tail = line.rpartition(' => ')
            if tail.startswith('err'):
                errors[(export, tail)] += 1
                continue
            lines.append((head + ' => ' + result, tail))
    print('measured', len(lines), 'cases; errors', dict(errors),
          '; no result (crashed or hung):', crashed)
    header = ('# Measured from a Visual Basic 6 program that declares the\n'
              '# exports (scripts/vb6_harness_gen.py); the calls that raise an\n'
              '# error are not cases.\n')
    with open(os.path.join(cases, family + '.cases'), 'w',
              newline='\n') as out:
        out.write(header + '\n'.join(l[0] for l in lines) + '\n')
    with open(os.path.join(cases, family + '.expected'), 'w',
              newline='\n') as out:
        out.write('\n'.join(l[1] for l in lines) + '\n')


if __name__ == '__main__':
    main(*sys.argv[1:4])
