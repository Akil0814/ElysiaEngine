"""Check local Markdown links and heading anchors (Python 3, standard library)."""
from pathlib import Path
import re
import sys
import tempfile
import unicodedata
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
FENCE = re.compile(r"^\s*(`{3,}|~{3,})")
LINK = re.compile(r"!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\)")

def prose_lines(text):
    fence = None
    for number, line in enumerate(text.splitlines(), 1):
        match = FENCE.match(line)
        if match:
            marker = match.group(1)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
            continue
        if fence is None:
            yield number, line

def heading_slug(heading):
    heading = re.sub(r"\[([^]]+)\]\([^)]*\)", r"\1", heading)
    heading = re.sub(r"<[^>]+>", "", heading).strip().lower()
    return ''.join(c for c in heading if c in '-_ ' or unicodedata.category(c)[0] in 'LNM').replace(' ', '-')

def anchors(path):
    text = path.read_text(encoding='utf-8-sig')
    result = set(re.findall(r'<[^>]+\b(?:id|name)=[\"\']([^\"\']+)', text))
    seen = {}
    for _, line in prose_lines(text):
        match = re.match(r'^ {0,3}#{1,6}\s+(.+?)(?:\s+#+)?$', line)
        if match:
            slug = heading_slug(match.group(1))
            index = seen.get(slug, 0)
            seen[slug] = index + 1
            result.add(slug if index == 0 else f'{slug}-{index}')
    return result

def check(paths):
    errors = []
    cache = {}
    for path in paths:
        text = path.read_text(encoding='utf-8-sig')
        for number, raw_line in prose_lines(text):
            # Inline code can contain lambda captures that resemble Markdown links.
            line = re.sub(r'(`+).*?\1', '', raw_line)
            for match in LINK.finditer(line):
                target = match.group(1).strip('<>')
                parts = urlsplit(target)
                if parts.scheme or parts.netloc:
                    continue
                dest = (path.parent / unquote(parts.path)).resolve() if parts.path else path
                if not dest.exists():
                    errors.append((path, number, target, '文件不存在'))
                elif parts.fragment and dest.suffix.lower() == '.md':
                    if dest not in cache:
                        cache[dest] = anchors(dest)
                    if unquote(parts.fragment).lower() not in cache[dest]:
                        errors.append((path, number, target, '标题锚点不存在'))
        # A code fence left open is also a document error.
        fence = None
        for number, line in enumerate(text.splitlines(), 1):
            match = FENCE.match(line)
            if match:
                marker = match.group(1)
                if fence is None:
                    fence = (marker, number)
                elif marker[0] == fence[0][0] and len(marker) >= len(fence[0]):
                    fence = None
        if fence:
            errors.append((path, fence[1], fence[0], '代码围栏未闭合'))
    return errors

def main():
    sys.stdout.reconfigure(encoding='utf-8')
    if sys.argv[1:] == ['--self-test']:
        test_parent = ROOT / 'out/documentation-link-check'
        test_parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=test_parent) as directory:
            base = Path(directory)
            assert base.resolve().is_relative_to(test_parent.resolve())
            target = base / 'target file.md'
            target.write_text('# 中文 `API`\n# 中文 `API`\n', encoding='utf-8')
            caller = base / 'caller.md'
            caller.write_text(
                '[first](target%20file.md#中文-api)\n'
                '[second](<target file.md#中文-api-1>)\n'
                '`[ignored](missing.md)`\n'
                '```cpp\n[ignored](missing.md)\n```\n'
                '[external](https://example.com/missing.md)\n', encoding='utf-8')
            assert not check([caller, target])
            caller.write_text('[bad](missing.md)\n[bad](target%20file.md#absent)\n```cpp\n', encoding='utf-8')
            assert [error[3] for error in check([caller])] == [
                '文件不存在', '标题锚点不存在', '代码围栏未闭合']
        print('Self-test passed: anchors, duplicate headings, URL decoding, code exclusion, and failures.')
        return 0
    if len(sys.argv) > 1:
        paths = [p for arg in sys.argv[1:] for p in ((ROOT / arg).rglob('*.md') if (ROOT / arg).is_dir() else [ROOT / arg])]
    else:
        paths = sorted((ROOT / 'docs/zh-CN').rglob('*.md')) + sorted((ROOT / 'docs/en').rglob('*.md'))
        paths += [ROOT / 'README.md', ROOT / 'README.zh-CN.md']
        # Also verify incoming references from languages outside the supported pair.
        callers = sorted((ROOT / 'docs').rglob('*.md')) + sorted(ROOT.glob('README*.md'))
        for path in callers:
            if path in paths:
                continue
            for number, raw_line in prose_lines(path.read_text(encoding='utf-8-sig')):
                line = re.sub(r'(`+).*?\1', '', raw_line)
                for match in LINK.finditer(line):
                    target = match.group(1).strip('<>')
                    parts = urlsplit(target)
                    if parts.scheme or parts.netloc or not parts.path:
                        continue
                    dest = (path.parent / unquote(parts.path)).resolve()
                    if any(dest.is_relative_to(ROOT / 'docs' / language) for language in ('zh-CN', 'en')):
                        fragment = unquote(parts.fragment).lower()
                        if not dest.exists() or (fragment and dest.suffix == '.md' and fragment not in anchors(dest)):
                            print(f'{path.relative_to(ROOT)}:{number}: invalid incoming link: {target}')
                            return 1
    errors = check(paths)
    for path, number, target, reason in errors:
        print(f'{path.relative_to(ROOT)}:{number}: {reason}: {target}')
    print(f'Checked {len(paths)} Markdown files; {len(errors)} errors.')
    return bool(errors)

if __name__ == '__main__':
    sys.exit(main())
