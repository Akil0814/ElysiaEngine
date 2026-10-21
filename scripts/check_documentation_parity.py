"""Validate the Chinese/English documentation mirror using only Python 3's stdlib.

Run without arguments for the repository, or --source DIR --target DIR for a pair.
The link checker supplies the same heading/duplicate-slug rules used for links.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys
import tempfile
from urllib.parse import unquote, urlsplit

sys.dont_write_bytecode = True
from check_documentation_links import FENCE, LINK, ROOT, check, prose_lines


@dataclass
class Block:
    language: str
    text: str
    line: int


def blocks(text):
    result = []
    marker = None
    content = []
    for number, line in enumerate(text.splitlines(), 1):
        match = FENCE.match(line)
        if marker is None and match:
            marker = match.group(1)
            language = line[match.end():].strip()
            start = number + 1
            content = []
        elif marker is not None:
            if match and match.group(1)[0] == marker[0] and len(match.group(1)) >= len(marker):
                result.append(Block(language, '\n'.join(content), start))
                marker = None
            else:
                content.append(line)
    return result


# Longest punctuators matter: '+ +' must not compare equal to '++'. Comments
# are skipped only outside literals, including raw literals and digit separators.
CPP_TOKEN = re.compile(
    r'''(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)|'''
    r'''(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)")|'''
    r'''(?P<string>(?:u8|u|U|L)?(?:"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'))|'''
    r'''(?P<number>(?:\d|\.\d)(?:[eEpP][+-]|[\w.'])*)|'''
    r'''(?P<identifier>[A-Za-z_]\w*)|'''
    r'''(?P<operator><=>|>>=|<<=|->\*|\.\.\.|::|->|\.\*|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|\^=|##|\S)''')


def cpp_tokens(text):
    return [(m.group(), text.count('\n', 0, m.start()) + 1)
            for m in CPP_TOKEN.finditer(text) if m.lastgroup != 'comment']


def json_signature(text):
    # Integer/float distinctions affect strict resource schemas. Ordinary Python
    # equality treats 1 == 1.0 and dict parsing discards duplicate properties.
    # Preserve those distinctions and field order while ignoring whitespace.
    def reject_constant(value):
        raise ValueError(f'non-JSON numeric constant: {value}')
    return json.loads(text, object_pairs_hook=lambda pairs: ('object', pairs),
                      parse_int=lambda value: ('integer', value),
                      parse_float=lambda value: ('number', value),
                      parse_constant=reject_constant)


def headings(text):
    return [(number, len(match[1])) for number, line in prose_lines(text)
            if (match := re.match(r'^ {0,3}(#{1,6})\s+', line))]


def table_shapes(text):
    """Ignore pipes inside inline code when counting Markdown table columns."""
    return [(number, len(re.findall(r'\|', re.sub(r'`[^`]*`', '', line))))
            for number, line in prose_lines(text) if line.lstrip().startswith('|')]


def translation_residuals(text):
    """Allow stable IDs, language names, and literal example values, not prose."""
    chinese = re.compile(r'[\u4e00-\u9fff]')
    for number, line in prose_lines(text):
        clean = re.sub(r'`[^`]*`|<[^>]*>|\]\([^)]*\)', '', line)
        clean = clean.replace('简体中文', '').replace('中文', '')
        if chinese.search(clean):
            yield number
    for block in blocks(text):
        if block.language.lower() == 'json':
            continue  # JSON values are preserved verbatim by the parity check.
        if block.language.lower() in ('cpp', 'c++', 'cxx'):
            # Literal text must remain unchanged; comments must be translated.
            for match in CPP_TOKEN.finditer(block.text):
                if match.lastgroup not in ('raw', 'string') and chinese.search(match.group()):
                    yield block.line + block.text.count('\n', 0, match.start())
        else:
            for offset, line in enumerate(block.text.splitlines()):
                if chinese.search(line):
                    yield block.line + offset


def local_destinations(path):
    for _, raw_line in prose_lines(path.read_text(encoding='utf-8-sig')):
        line = re.sub(r'(`+).*?\1', '', raw_line)
        for match in LINK.finditer(line):
            parts = urlsplit(match[1].strip('<>'))
            if not parts.scheme and not parts.netloc and parts.path:
                yield (path.parent / unquote(parts.path)).resolve()


def validate(source, target):
    source, target = source.resolve(), target.resolve()
    left = {p.relative_to(source): p for p in source.rglob('*.md')}
    right = {p.relative_to(target): p for p in target.rglob('*.md')}
    errors = []
    for root, pages in ((source, left), (target, right)):
        if Path('README.md') not in pages:
            errors.append(f'{root / "README.md"}:1: missing documentation landing page')
    for name in sorted(left.keys() - right.keys()):
        errors.append(f'{target / name}:1: missing English counterpart')
    for name in sorted(right.keys() - left.keys()):
        errors.append(f'{target / name}:1: no Chinese counterpart')
    for name in sorted(left.keys() & right.keys()):
        a, b = (p.read_text(encoding='utf-8-sig') for p in (left[name], right[name]))
        ah, bh = headings(a), headings(b)
        if [h[1] for h in ah] != [h[1] for h in bh]:
            i = next((i for i, pair in enumerate(zip(ah, bh)) if pair[0][1] != pair[1][1]), min(len(ah), len(bh)))
            line = bh[i][0] if i < len(bh) else len(b.splitlines())
            errors.append(f'{right[name]}:{line}: heading hierarchy differs at heading {i + 1} ({len(ah)} vs {len(bh)} headings)')
        ids = lambda text: re.findall(r'<[^>]+\b(?:id|name)=[\"\']([^\"\']+)', text)
        if ids(a) != ids(b):
            errors.append(f'{right[name]}:1: explicit HTML IDs differ')
        at, bt = table_shapes(a), table_shapes(b)
        if [row[1] for row in at] != [row[1] for row in bt]:
            errors.append(f'{right[name]}:1: table row/column structure differs')
        for line in sorted(set(translation_residuals(b))):
            errors.append(f'{right[name]}:{line}: untranslated Chinese prose, diagram label, or comment')
        ab, bb = blocks(a), blocks(b)
        if [x.language for x in ab] != [x.language for x in bb]:
            errors.append(f'{right[name]}:1: fence languages/order differ: {[x.language for x in ab]} vs {[x.language for x in bb]}')
            continue
        for index, (x, y) in enumerate(zip(ab, bb), 1):
            if x.language.lower() in ('cpp', 'c++', 'cxx'):
                xt, yt = cpp_tokens(x.text), cpp_tokens(y.text)
                if [t[0] for t in xt] != [t[0] for t in yt]:
                    i = next((i for i, pair in enumerate(zip(xt, yt)) if pair[0][0] != pair[1][0]), min(len(xt), len(yt)))
                    before = xt[i][0] if i < len(xt) else '<end>'
                    after = yt[i][0] if i < len(yt) else '<end>'
                    line = y.line + (yt[i][1] - 1 if i < len(yt) else len(y.text.splitlines()))
                    errors.append(f'{right[name]}:{line}: C++ block {index}, token {i + 1}: {before!r} != {after!r}; source {left[name]}:{x.line + (xt[i][1] - 1 if i < len(xt) else 0)}')
            elif x.language.lower() == 'json':
                try:
                    xv, yv = json_signature(x.text), json_signature(y.text)
                    if xv != yv:
                        errors.append(f'{right[name]}:{y.line}: JSON block {index} differs from {left[name]}:{x.line}')
                except ValueError as error:
                    errors.append(f'{right[name]}:{y.line}: JSON block {index}: {error}')
    for path, number, destination, reason in check(list(left.values()) + list(right.values())):
        errors.append(f'{path}:{number}: {reason}: {destination}')
    # Every page must be reachable through local links from the English landing.
    pending = [target / 'README.md']
    visited = set()
    while pending:
        path = pending.pop()
        if path in visited or not path.is_file() or not path.is_relative_to(target):
            continue
        visited.add(path)
        pending.extend(p for p in local_destinations(path) if p.suffix.lower() == '.md')
    for path in sorted(set(right.values()) - visited):
        errors.append(f'{path}:1: unreachable from English README.md')
    for name, path in right.items():
        for destination in local_destinations(path):
            if destination.is_relative_to(source) and name != Path('README.md'):
                errors.append(f'{path}:1: English documentation links to Chinese page: {destination}')
    return errors


def self_test():
    with tempfile.TemporaryDirectory(prefix='elysia-doc-parity-') as directory:
        source, target = (Path(directory) / name for name in ('zh-CN', 'en'))
        source.mkdir(); target.mkdir()
        original = '# Heading\n# Heading\n[second](#heading-1)\n```cpp\nconst char* s = "// 中文"; /* old */ int n=1;\n```\n```json\n{"value":1}\n```\n| Key | Value |\n| --- | --- |\n| a | b |\n'
        a, b = source / 'README.md', target / 'README.md'
        a.write_text(original, encoding='utf-8')
        b.write_text(original.replace('/* old */', '// English comment\n'), encoding='utf-8')
        assert not validate(source, target), validate(source, target)
        assert cpp_tokens('R"tag(// /* string */)tag"')[0][0] == 'R"tag(// /* string */)tag"'
        assert [x[0] for x in cpp_tokens("1'024 + + x")] != [x[0] for x in cpp_tokens("1'024 ++ x")]
        for old, new, message in (
                ('"// 中文"', '"// changed"', 'C++ block'),
                ('int n=1', 'int n=2', 'C++ block'),
                ('# Heading\n# Heading', '# Heading\n## Heading', 'heading hierarchy'),
                ('```cpp', '```text', 'fence languages/order'),
                ('{"value":1}', '{"value":2}', 'JSON block'),
                ('{"value":1}', '{"value":1.0}', 'JSON block'),
                ('| a | b |', '| a | b | c |', 'table row/column'),
                ('# Heading\n# Heading', '# Heading\n# 未翻译标题', 'untranslated Chinese'),
                ('#heading-1', '#absent', '标题锚点不存在')):
            b.write_text(original.replace(old, new), encoding='utf-8')
            assert any(message in e for e in validate(source, target)), (message, validate(source, target))
        b.write_text(original + '[bad](missing.md)\n', encoding='utf-8')
        assert any('文件不存在' in e for e in validate(source, target))
        a.with_name('missing.md').write_text('# Missing\n', encoding='utf-8')
        assert any('missing English counterpart' in e for e in validate(source, target))
        b.with_name('orphan.md').write_text('# Orphan\n', encoding='utf-8')
        assert any('unreachable' in e for e in validate(source, target))
        b.unlink()
        assert any('missing documentation landing page' in e for e in validate(source, target))
        assert json_signature('{"a":1,"a":2}') != json_signature('{"a":0,"a":2}')
    print('Self-test passed: missing pages/landing, links/anchors, duplicate headings, hierarchy, tables, fences, JSON types, comments, raw strings, changed literals/tokens, reachability.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'docs/zh-CN')
    parser.add_argument('--target', type=Path, default=ROOT / 'docs/en')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.source.is_dir() or not args.target.is_dir():
        parser.error('source and target must be existing directories')
    errors = validate(args.source, args.target)
    for error in errors:
        print(error)
    print(f'Checked {len(list(args.source.rglob("*.md")))} Chinese / {len(list(args.target.rglob("*.md")))} English pages; {len(errors)} errors.')
    return bool(errors)


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    sys.exit(main())
