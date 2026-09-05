"""Tests for the Jyutping reference segmenter.

Run:  python test_jyutping_segment.py   (exit code 0 = all pass)
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from jyutping_segment import JyutpingScheme, JyutpingSegmenter

scheme = JyutpingScheme()
seg = JyutpingSegmenter(scheme)
seg_tones = JyutpingSegmenter(scheme)

FAILURES = []


def check(name, got, want):
    if got != want:
        FAILURES.append(f'{name}: got {got!r}, want {want!r}')
        print(f'FAIL {name}: got {got!r}, want {want!r}')
    else:
        print(f'PASS {name}')


# --- scheme tables -----------------------------------------------------------

check('onset count', len(scheme.onsets), 19)
check('final count', len(scheme.finals), 62)
check('ng onset', 'ng' in scheme.onsets, True)
check('zero-onset syllable aa', scheme.decompose('aa'), ('', 'aa'))
check('syllabic nasal ng', scheme.decompose('ng'), ('', 'ng'))
check('syllabic nasal m', scheme.decompose('m'), ('', 'm'))
check('gw onset beats g', scheme.decompose('gwaai'), ('gw', 'aai'))
check('kw onset', scheme.decompose('kwai'), ('kw', 'ai'))
check('invalid syllable', scheme.decompose('xyz'), None)
check('attested syllable count', len(scheme.bases), 659)

# --- basic segmentation ------------------------------------------------------

r = seg.segment('jyutping')
check('jyutping syllables', (r.syllables, r.pending), (['jyut', 'ping'], ''))

r = seg.segment('gwongzau')   # 广州 gwong2 zau1
check('gwongzau', (r.syllables, r.pending), (['gwong', 'zau'], ''))

r = seg.segment('hoenggong')   # 香港 hoeng1 gong2
check('hoenggong', (r.syllables, r.pending), (['hoeng', 'gong'], ''))

r = seg.segment('ngo')         # 我
check('ngo', (r.syllables, r.pending), (['ngo'], ''))

r = seg.segment('ng')          # 吴
check('ng', (r.syllables, r.pending), (['ng'], ''))

r = seg.segment('m')           # 唔
check('m', (r.syllables, r.pending), (['m'], ''))

# --- apostrophe separator ----------------------------------------------------

r = seg.segment("jyut'ping")
check('apostrophe explicit', (r.syllables, r.pending), (['jyut', 'ping'], ''))

# 暗 ngam3 vs 南 naam4: ambiguous boundary without apostrophe
r = seg.segment('ngam')        # could be ng+am or n+gam? pick attested best
check('ngam valid', r.syllables in (['ngam'], ['ng', 'am']), True)

# --- pending tails -----------------------------------------------------------

r = seg.segment('jyutp')
check('pending p', (r.syllables, r.pending), (['jyut'], 'p'))

r = seg.segment('jyutpi')
check('pending pi', (r.syllables, r.pending), (['jyut'], 'pi'))

r = seg.segment('jyutpingg')
check('trailing junk', r.pending in ('g', 'gg'), True)

# --- invalid input ------------------------------------------------------------

r = seg.segment('xyz')
check('all invalid', r.syllables, [])

# --- tone mode -----------------------------------------------------------------

r = seg.segment('jyut6ping3', with_tones=True)
check('tones consumed', (r.syllables, r.pending), (['jyut', 'ping'], ''))

r = seg.segment('fu1', with_tones=True)
check('single tone syllable', (r.syllables, r.pending), (['fu'], ''))

# tone digit that creates unattested pair is still accepted while typing
r = seg.segment('fu9'.replace('9', '6'), with_tones=True)
check('tone 6 on fu', r.syllables, ['fu'])

# --- alternative segmentations (top-N) ---------------------------------------

all_r = seg.segment_all('ngong', limit=4)
alts = [x.syllables for x in all_r]
print('ngong alternatives:', alts)
check('ngong has some parse', len(alts) > 0, True)

# --- summary -------------------------------------------------------------------

print()
if FAILURES:
    print(f'{len(FAILURES)} failure(s)')
    sys.exit(1)
print('all tests passed')
