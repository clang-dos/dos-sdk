#!/usr/bin/env python3
"""Print results.csv as Markdown tables, one per memory model.

Each cell is the file size and, in parentheses, the code size, in bytes.
usage: report.py out/results.csv
"""
import csv
import sys

rows = list(csv.DictReader(open(sys.argv[1])))
progs = list(dict.fromkeys(r['program'] for r in rows))
tcs = list(dict.fromkeys(r['toolchain'] for r in rows))
models = list(dict.fromkeys(r['model'] for r in rows))
for m in models:
    print('\n### %s\n' % m)
    print('| program | ' + ' | '.join(tcs) + ' |')
    print('|---|' + '---:|' * len(tcs))
    for p in progs:
        cells = []
        for t in tcs:
            r = [x for x in rows if x['toolchain'] == t and x['model'] == m and x['program'] == p]
            if not r:
                cells.append('n/a')
            elif r[0]['status'] == 'build-failed':
                cells.append('fails')
            else:
                c = '%s (%s)' % (r[0]['file'], r[0]['code'])
                if r[0]['status'] != 'ok':
                    c += ' ' + r[0]['status']
                cells.append(c)
        print('| %s | %s |' % (p, ' | '.join(cells)))
