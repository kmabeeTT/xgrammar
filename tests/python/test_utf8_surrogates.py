"""UTF-16 surrogates (U+D800-U+DFFF) are not codepoints UTF-8 may encode, so no character class matches their
three-byte encodings (ED A0-BF xx), while the codepoints on either side still match."""

import pytest

import xgrammar as xgr
from xgrammar.testing import _is_grammar_accept_string

SURROGATES = [b"\xed\xa0\x80", b"\xed\xbd\xb7", b"\xed\xbf\xbf"]  # U+D800, U+DF77, U+DFFF
NEIGHBOURS = [b"\xed\x9f\xbf", b"\xee\x80\x80", "€".encode(), "😀".encode()]  # U+D7FF, U+E000, U+20AC, U+1F600

GRAMMARS = {
    "negated class": 'root ::= "x" [^"\\\\]* "y"',
    "negated class star in a string": 'root ::= "\\"" [^"\\\\\\r\\n]* "\\""',
    "full range": 'root ::= "x" [\\u0000-\\U0010ffff]* "y"',
}


@pytest.mark.parametrize("name", list(GRAMMARS))
@pytest.mark.parametrize("char", SURROGATES)
def test_surrogates_rejected(name, char):
    g = GRAMMARS[name]
    body = b'"' + char + b'"' if "string" in name else b"x" + char + b"y"
    assert not _is_grammar_accept_string(g, body)


@pytest.mark.parametrize("name", list(GRAMMARS))
@pytest.mark.parametrize("char", NEIGHBOURS)
def test_codepoints_around_surrogates_accepted(name, char):
    g = GRAMMARS[name]
    body = b'"' + char + b'"' if "string" in name else b"x" + char + b"y"
    assert _is_grammar_accept_string(g, body)


@pytest.mark.parametrize("char", SURROGATES)
def test_json_strings_and_regex_reject_surrogates(char):
    schema = xgr.Grammar.from_json_schema('{"type": "string"}')
    assert not _is_grammar_accept_string(schema, b'"' + char + b'"')
    assert not _is_grammar_accept_string(xgr.Grammar.builtin_json_grammar(), b'{"a": "' + char + b'"}')
    assert not _is_grammar_accept_string(xgr.Grammar.from_regex('[^"]*'), char)
    assert _is_grammar_accept_string(xgr.Grammar.from_regex('[^"]*'), "€".encode())
