"""Regenerate the Jyutping data assets from Unicode Unihan.

Downloads Unihan.zip, extracts kCantonese (LSHK Jyutping), and rebuilds:
  - unihan_kcantonese.txt     <U+codepoint>\t<reading1 reading2 ...>
  - jyutping_syllables.json   attested syllable inventory

Usage:  python regen_unihan_data.py
"""
import io
import json
import os
import urllib.request
import zipfile
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
URL = 'https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip'

# Official jyutping.org (LSHK) scheme tables (incl. 2018 additions).
ONSETS = ['b', 'p', 'm', 'f', 'd', 't', 'n', 'l', 'g', 'k', 'ng', 'h',
          'gw', 'kw', 'w', 'z', 'c', 's', 'j']
FINALS = [
    'aa', 'aai', 'aau', 'aam', 'aan', 'aang', 'aap', 'aat', 'aak',
    'a', 'ai', 'au', 'am', 'an', 'ang', 'ap', 'at', 'ak',
    'e', 'ei', 'eu', 'em', 'en', 'eng', 'ep', 'et', 'ek',
    'eoi', 'eon', 'eot',
    'oe', 'oeng', 'oet', 'oek',
    'o', 'oi', 'ou', 'on', 'ong', 'ot', 'ok',
    'i', 'iu', 'im', 'in', 'ing', 'ip', 'it', 'ik',
    'yu', 'yun', 'yut',
    'u', 'ui', 'um', 'un', 'ung', 'up', 'ut', 'uk',
    'm', 'ng',
]


def decompose(syl):
    for onset in sorted(ONSETS, key=len, reverse=True):
        if syl.startswith(onset):
            rest = syl[len(onset):]
            if rest in FINALS:
                return onset, rest
            if rest == '' and onset in ('m', 'ng'):
                return '', onset
            if rest:
                return None
    if syl in FINALS:
        return '', syl
    return None


def main():
    print('downloading', URL)
    req = urllib.request.Request(URL, headers={'User-Agent': 'Mozilla/5.0'})
    data = urllib.request.urlopen(req, timeout=300).read()
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        text = zf.read('Unihan_Readings.txt').decode('utf-8')

    readings = defaultdict(set)
    char_count = defaultdict(int)
    examples = defaultdict(list)
    undec = set()
    lines_out = []
    for line in text.splitlines():
        if line.startswith('#') or '\tkCantonese\t' not in line:
            continue
        cp, _, value = line.split('\t', 2)
        char = chr(int(cp[2:], 16))
        lines_out.append(f'{cp}\t{value}')
        for reading in value.split(' '):
            if not reading or reading[-1] not in '123456':
                continue
            base, tone = reading[:-1], reading[-1]
            readings[base].add(tone)
            char_count[base] += 1
            if len(examples[base]) < 3:
                examples[base].append(char)
            if decompose(base) is None:
                undec.add(base)

    with open(os.path.join(HERE, 'unihan_kcantonese.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines_out) + '\n')

    inv = {
        'onsets': ONSETS,
        'finals': FINALS,
        'tones': ['1', '2', '3', '4', '5', '6'],
        'syllables': {
            base: {
                'tones': sorted(readings[base]),
                'onset': (decompose(base) or ('?', '?'))[0],
                'final': (decompose(base) or ('?', '?'))[1],
                'char_count': char_count[base],
                'examples': examples[base],
            } for base in sorted(readings)
        },
        'undecomposable': sorted(undec),
    }
    with open(os.path.join(HERE, 'jyutping_syllables.json'), 'w', encoding='utf-8') as f:
        json.dump(inv, f, ensure_ascii=False, indent=1)

    print(f'characters: {len(lines_out)}, base syllables: {len(readings)}, '
          f'undecomposable: {len(undec)}')


if __name__ == '__main__':
    main()
