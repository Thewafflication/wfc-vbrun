import re, struct, os, collections

T = os.environ['TEMP'] + '/wr/vb6vh/vh.txt'
D = 'C:/Users/jmwau/source/repos/wfc/lib/wfc-vbrun/tests/cases/'
rows = open(T, encoding='utf-8', errors='replace').read().replace('\r', '').splitlines()


def signed64(hexa):
    n = int(hexa, 16)
    return n - (1 << 64) if n >> 63 else n


def case_in(s):
    m = re.match(r'v(\d+)(?::(.*)|(".*"))?$', s)
    vt = int(m.group(1))
    a = m.group(2)
    if vt in (0, 1) or m.group(3):
        return s
    if vt == 4:
        return 'v4:' + repr(struct.unpack('<f', struct.pack('<I', int(a[1:], 16)))[0])
    if vt in (5, 7):
        return 'v%d:' % vt + repr(struct.unpack('<d', struct.pack('<Q', int(a[1:], 16)))[0])
    if vt == 6:
        return 'v6:%d' % signed64(a)
    return s


def case_out(s):
    if s.startswith('v6:'):
        return 'v6:%d' % signed64(s[3:])
    return s


cases = []
exp = []
errs = collections.Counter()
for r in rows:
    m = re.match(r'(\w+) IN (.*?) => (.*)$', r)
    fn, inp, res = m.groups()
    if res.startswith('err'):
        errs[(fn, res)] += 1
        continue
    cases.append('%s o %s => V' % (fn, case_in(inp)))
    exp.append(case_out(res))
head = ('# Abs, Fix, Int, Sgn, Len, LenB, LCase, UCase, Trim, LTrim, RTrim on\n'
        '# Variants, measured from a Visual Basic 6 program that declares the\n'
        '# exports. The cases where the reference raises an error (13 for a text\n'
        '# that is no number) are not cases.\n')
open(D + 'varfuncs.cases', 'w', newline='\n').write(head + '\n'.join(cases) + '\n')
open(D + 'varfuncs.expected', 'w', newline='\n').write('\n'.join(exp) + '\n')
print(len(cases), dict(errs))
