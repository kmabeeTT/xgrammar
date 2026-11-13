"""Test that token masks stay exact when the matcher reuses cached parser rows.

Every mask is compared with the tokens a fork of the matcher accepts one by one, which does not
go through the cache.
"""

import json
import random
import sys
from typing import List

import pytest

import xgrammar as xgr
from xgrammar.testing import bitmask_to_bool_mask

# Single characters plus tokens that close one rule and continue in its parent, which are the
# tokens a mask has to check against the parser.
VOCAB = (
    ["<stop>"]
    + list('{}[],:" \n0123456789abetruflsn-.')
    + ['{"', '"}', "},", "}]", '",', '":', '":"', '","', "},{", '"}]', "]}", "],", '"]', "}}", ", "]
    + ["true", "false", "null", '"a"', '"b"', "12", "},\n", "}\n", "]\n", '{"a":', ' "']
)

SCHEMA = {
    "type": "object",
    "properties": {
        "a": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "b": {"type": "string"},
                    "n": {"type": "integer"},
                    "t": {"type": "array", "items": {"type": "boolean"}},
                },
                "required": ["b"],
            },
        },
        "s": {"type": "object", "properties": {"b": {"type": "string"}}},
    },
    "required": ["a"],
}


def _compile() -> xgr.CompiledGrammar:
    tokenizer_info = xgr.TokenizerInfo(VOCAB, stop_token_ids=[0])
    return xgr.GrammarCompiler(tokenizer_info).compile_json_schema(json.dumps(SCHEMA))


def _allowed_by_mask(matcher: xgr.GrammarMatcher) -> List[int]:
    bitmask = xgr.allocate_token_bitmask(1, len(VOCAB))
    matcher.fill_next_token_bitmask(bitmask)
    return bitmask_to_bool_mask(bitmask, len(VOCAB))[0].nonzero().flatten().tolist()


def _allowed_by_accept(matcher: xgr.GrammarMatcher) -> List[int]:
    return [i for i in range(len(VOCAB)) if matcher.fork().accept_token(i)]


def _choose(allowed: List[int], rng: random.Random) -> int:
    # Prefer the tokens that move through the structure over more string content.
    weights = [8 if any(c in VOCAB[i] for c in '{}[],:"') else 1 for i in allowed]
    return rng.choices(allowed, weights)[0]


def _check_random_walk(matcher: xgr.GrammarMatcher, rng: random.Random, steps: int) -> None:
    num_accepted = 0
    for _ in range(steps):
        if matcher.is_terminated():
            matcher.reset()
            num_accepted = 0
        allowed = _allowed_by_mask(matcher)
        assert allowed == _allowed_by_accept(matcher)
        action = rng.random()
        if action < 0.1 and num_accepted > 0:
            num_tokens = rng.randint(1, min(num_accepted, 4))
            matcher.rollback(num_tokens)
            num_accepted -= num_tokens
        elif action < 0.13:
            matcher.reset()
            num_accepted = 0
        elif action < 0.2:
            fork = matcher.fork()
            assert fork.accept_token(_choose(allowed, rng))
            if not fork.is_terminated():
                assert _allowed_by_mask(fork) == _allowed_by_accept(fork)
            assert _allowed_by_mask(matcher) == allowed
        else:
            assert matcher.accept_token(_choose(allowed, rng))
            num_accepted += 1


def test_masks_with_rollback_reset_and_fork():
    _check_random_walk(xgr.GrammarMatcher(_compile()), random.Random(0), 600)


def test_masks_of_later_matchers_of_one_grammar():
    compiled_grammar = _compile()
    rng = random.Random(1)
    # A matcher continues with the rows the matchers before it reached.
    for _ in range(3):
        _check_random_walk(xgr.GrammarMatcher(compiled_grammar), rng, 150)
    matchers = [xgr.GrammarMatcher(compiled_grammar) for _ in range(3)]
    for _ in range(30):
        for matcher in matchers:
            _check_random_walk(matcher, rng, 3)


if __name__ == "__main__":
    pytest.main(sys.argv)
