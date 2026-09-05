"""Reference Jyutping syllable segmenter for the MSIME Cantonese scheme.

This is the algorithmic reference that the C++ engine module
(`MSIME-Engine/yue/`) should mirror. Rules:

1. Alphabet: a-z, apostrophe (explicit syllable separator), and tone digits
   1-6 only when tone input is enabled.
2. A syllable is (onset?)(final)(tone?) where onset/final come from the LSHK
   scheme tables and the syllable must be attested (present in the inventory
   derived from Unihan kCantonese). Unattested-but-phonotactic syllables are
   kept as a configurable extension (`allow_unattested`).
3. Segmentation is a dynamic program over attested syllables:
   - maximize consumed characters,
   - among full covers, prefer fewer syllables (longer-match bias),
   - then prefer splits using more frequent syllables (character-count score).
4. A trailing string that is a prefix of at least one valid syllable is kept
   as a *pending* fragment (the engine keeps it in the composition and shows
   it in the preedit, like quanpin does).
5. Tone digits, when enabled, terminate the current syllable. A digit after a
   complete syllable attaches to it; a digit that would create an unattested
   syllable+tone pair is still accepted while typing (candidate filtering
   handles it) so users are never blocked.

Deliberately NOT handled here (engine-level concerns): candidate lookup,
helpcodes, jianpin (initial-letter) expansion, autocorrect/fuzzy matching.
"""

from __future__ import annotations

import json
import os
from typing import Dict, List, Optional, Sequence, Tuple

_HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_INVENTORY = os.path.join(_HERE, '..', 'data', 'jyutping_syllables.json')


class JyutpingScheme:
    """Static scheme tables + attested syllable inventory."""

    def __init__(self, inventory_path: str = DEFAULT_INVENTORY):
        with open(inventory_path, encoding='utf-8') as f:
            inv = json.load(f)
        self.onsets: List[str] = inv['onsets']
        self.finals: List[str] = inv['finals']
        self.tones: List[str] = inv['tones']
        # base syllable -> {'tones': [...], 'char_count': int}
        self.syllables: Dict[str, dict] = inv['syllables']
        self.bases = set(self.syllables)
        # phonotactic set: every onset+final that parses under the scheme
        self.phonotactic = set()
        for onset in [''] + self.onsets:
            for final in self.finals:
                s = onset + final
                if self.decompose(s) is not None:
                    self.phonotactic.add(s)
        # prefix index for pending detection
        self.base_prefixes = set()
        for b in self.bases:
            for i in range(1, len(b) + 1):
                self.base_prefixes.add(b[:i])

    def decompose(self, syl: str) -> Optional[Tuple[str, str]]:
        for onset in sorted(self.onsets, key=len, reverse=True):
            if syl.startswith(onset):
                rest = syl[len(onset):]
                if rest in self.finals:
                    return onset, rest
                if rest == '' and onset in ('m', 'ng'):
                    return '', onset
                if rest:
                    return None
        if syl in self.finals:
            return '', syl
        return None

    def is_valid_base(self, syl: str, allow_unattested: bool = False) -> bool:
        if syl in self.bases:
            return True
        return allow_unattested and syl in self.phonotactic


class SegmentResult:
    def __init__(self, syllables: List[str], pending: str):
        # syllables: complete base syllables (no tones); pending: trailing fragment
        self.syllables = syllables
        self.pending = pending

    def joined(self) -> str:
        return "'".join(self.syllables)

    def __repr__(self) -> str:
        return f"SegmentResult({self.syllables!r}, pending={self.pending!r})"

    def __eq__(self, other) -> bool:
        return (isinstance(other, SegmentResult) and self.syllables == other.syllables
                and self.pending == other.pending)


class JyutpingSegmenter:
    def __init__(self, scheme: JyutpingScheme, allow_unattested: bool = False,
                 max_syllables: int = 16):
        self.scheme = scheme
        self.allow_unattested = allow_unattested
        self.max_syllables = max_syllables

    # -- public API ---------------------------------------------------------

    def segment(self, raw: str, with_tones: bool = False) -> SegmentResult:
        """Best segmentation of `raw` (lowercase letters/apostrophes/tones)."""
        results = self.segment_all(raw, with_tones, limit=1)
        return results[0] if results else SegmentResult([], raw)

    def segment_all(self, raw: str, with_tones: bool = False,
                    limit: int = 8) -> List[SegmentResult]:
        """Top-N segmentations, best first (mirrors cut_pinyin_by_mode shape).

        Scoring key (ascending = better): (syllable_count, -frequency_weight).
        Complete coverage of the chunk always wins over partial coverage
        because we take the furthest reachable position first.
        """
        text = raw.lower()
        segments = self._split_on_apostrophe(text)
        # Each apostrophe-delimited chunk segments independently; results are
        # combined by cartesian product, best-scoring first.
        chunk_options: List[List[Tuple[List[str], Tuple[int, int]]]] = []
        for chunk in segments:
            opts = self._segment_chunk(chunk, with_tones)
            if not opts:
                return []
            chunk_options.append(opts)

        combined: List[Tuple[List[str], Tuple[int, int]]] = [([], (0, 0))]
        for opts in chunk_options:
            nxt = []
            for acc, (n, w) in combined:
                for sylls, (n2, w2) in opts:
                    nxt.append((acc + sylls, (n + n2, w + w2)))
            nxt.sort(key=lambda t: t[1])
            combined = nxt[:limit]

        out = []
        for sylls, _ in combined[:limit]:
            pending = ''
            if sylls and sylls[-1].startswith('~'):
                pending = sylls[-1][1:]
                sylls = sylls[:-1]
            out.append(SegmentResult(sylls, pending))
        return out

    # -- internals ----------------------------------------------------------

    @staticmethod
    def _split_on_apostrophe(text: str) -> List[str]:
        return [c for c in text.split("'") if c != '']

    def _syllable_valid(self, base: str) -> bool:
        return self.scheme.is_valid_base(base, self.allow_unattested)

    def _tone_valid(self, base: str, tone: str) -> bool:
        info = self.scheme.syllables.get(base)
        return bool(info and tone in info['tones'])

    def _weight(self, base: str) -> int:
        info = self.scheme.syllables.get(base)
        # char_count is a weak frequency proxy; capped so one very common
        # syllable cannot dominate tie-breaking.
        return min(info['char_count'], 256) if info else 0

    def _segment_chunk(self, chunk: str,
                       with_tones: bool) -> List[Tuple[List[str], Tuple[int, int]]]:
        """DP over one apostrophe-free chunk.

        Returns [(syllables, key)] where key = (syllable_count, -weight):
        fewer syllables first, then higher total character-count weight.
        """
        n = len(chunk)
        beam = 8
        # dp[i] = list of (path, key) covering chunk[:i]; transitions only go
        # forward, so dp[i] is final when iteration i starts and can be pruned.
        dp: List[List[Tuple[List[str], Tuple[int, int]]]] = [[] for _ in range(n + 1)]
        dp[0] = [([], (0, 0))]

        for i in range(n):
            if not dp[i]:
                continue
            dp[i].sort(key=lambda t: t[1])
            dp[i] = dp[i][:beam]
            for path, (cnt, weight) in dp[i]:
                if cnt >= self.max_syllables:
                    continue
                # longest attested base is 6 letters (e.g. gwaang)
                for j in range(i + 1, min(i + 6, n) + 1):
                    base = chunk[i:j]
                    if not base.isalpha():
                        break  # tone digit / junk terminates syllable extension
                    if not self._syllable_valid(base):
                        continue
                    key = (cnt + 1, weight - self._weight(base))
                    j2 = j
                    if with_tones and j < n and chunk[j] in '123456':
                        j2 = j + 1
                        if not self._tone_valid(base, chunk[j]):
                            # unattested syllable+tone: keep typing flowing,
                            # candidate lookup will simply find nothing
                            key = (cnt + 1, weight - self._weight(base) + 128)
                    dp[j2].append((path + [base], key))

        best_pos = -1
        for pos in range(n, -1, -1):
            if dp[pos]:
                dp[pos].sort(key=lambda t: t[1])
                dp[pos] = dp[pos][:beam]
                best_pos = pos
                break

        results: List[Tuple[List[str], Tuple[int, int]]] = []
        if best_pos >= 0:
            tail = chunk[best_pos:]
            for path, key in dp[best_pos]:
                if tail == '':
                    results.append((path, key))
                else:
                    results.append((path + ['~' + tail], key))
        return results[:beam]

    def _is_pending(self, tail: str) -> bool:
        if not tail or not tail.isalpha():
            return False
        return tail in self.scheme.base_prefixes


def _selfcheck():
    scheme = JyutpingScheme()
    seg = JyutpingSegmenter(scheme)
    cases = [
        ('jyutping', ['jyut', 'ping'], ''),
        ('gwongzau', ['gwong', 'zau'], ''),
        ('ngo', ['ngo'], ''),
        ('m', ['m'], ''),
        ('ng', ['ng'], ''),
        ("jyut'ping", ['jyut', 'ping'], ''),
        ('jyutp', ['jyut'], 'p'),
        ('xy', [], 'xy'),
    ]
    ok = True
    for raw, want_sylls, want_pending in cases:
        r = seg.segment(raw)
        got = (r.syllables, r.pending)
        status = 'PASS' if got == (want_sylls, want_pending) else 'FAIL'
        if status == 'FAIL':
            ok = False
        print(f'{status}: {raw!r} -> {got}')
    seg_t = JyutpingSegmenter(scheme)
    r = seg_t.segment('jyut6ping3', with_tones=True)
    print('tones:', r.syllables, r.pending, '(expect [jyut, ping])')
    return ok


if __name__ == '__main__':
    import sys
    sys.exit(0 if _selfcheck() else 1)
