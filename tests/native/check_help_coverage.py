"""Fail when a registered function or autocomplete command lacks help."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
engine = root / 'app/src/main/cpp/engine'
catalog = (engine / 'help_catalog.h').read_text()
entries = {}
for line in catalog.splitlines():
    if line.startswith('    {'):
        fields = json.loads('[' + line.strip()[1:-2] + ']')
        assert len(fields) == 5 and all(fields[i].strip() for i in range(4)), fields
        assert fields[0] not in entries, fields[0]
        entries[fields[0]] = fields
math = (engine / 'math_engine.cpp').read_text()
unary = math.split('unaryFunctions() {')[1].split('return functions;')[0]
required = set(re.findall(r'\{"([^"]+)"', unary))
binary = math.split('binaryFunctions() {')[1].split('return functions;')[0]
required.update(re.findall(r'\{"([^"]+)"', binary))
variadic = math.split('variadicFunctions() {')[1].split('return functions;')[0]
required.update(re.findall(r'\{"([^"]+)"', variadic))
required.update(re.findall(r'te_variable\{"([^"]+)"', math))
tiny = (engine.parent / 'tinyexpr-plusplus/tinyexpr.cpp').read_text()
tiny = tiny.split('te_parser::m_functions =')[1].split('//--------------------------------------------------')[0]
required.update(re.findall(r'\{\s*"([^"]+)"', tiny))
state = (engine / 'app_state.cpp').read_text().split('builtin_autocomplete_words = {')[1].split('};')[0]
for word in re.findall(r'"([^"]+)"', state):
    name = word.split('(')[0].strip()
    if name in {'fn', 'def', 'const'}:
        name = {'fn': 'scripts', 'def': 'scripts', 'const': 'assignment'}[name]
    required.add(name)
missing = required - entries.keys()
assert not missing, f'Missing help entries: {sorted(missing)}'
print(f'Help coverage verified for {len(required)} registered names and commands, {len(entries)} topics.')
