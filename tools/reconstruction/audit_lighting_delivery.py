"""Local-only delivery audit: docs existence/links, ignored state, and whitespace including new files."""
import json
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote


def main():
    root = Path(__file__).resolve().parents[2]
    def git(*args):
        return subprocess.run(['git', '-c', 'core.quotepath=false', *args], cwd=root, capture_output=True, encoding='utf-8')
    documents = ['docs/learning/19-original-lighting.md', 'docs/interview/19-qa.md', 'docs/validation/19-results.md',
                 'docs/troubleshooting/19-notes.md', 'docs/README.md', 'tools/reconstruction/LightingBaseline.md']
    report = {'documents': {}, 'missingLinks': [], 'missingPriorFiles': [], 'whitespaceErrors': []}
    target = root/'generated/prompt19-delivery-audit.json'
    target.write_text('{}', encoding='utf-8')  # The validation chapter links to this audit's final output.
    check = git('diff', '--check')
    (root/'generated/prompt19-diff-check.txt').write_text(check.stdout+check.stderr+f'\nexit={check.returncode}\n', encoding='utf-8')
    if check.returncode: report['whitespaceErrors'].append(check.stdout+check.stderr)
    paths = git('ls-files', '--others', '--exclude-standard').stdout.splitlines()
    for relative in paths:
        result = git('diff', '--no-index', '--check', '--', 'NUL', relative)
        # --no-index implies --exit-code: 1 also means the new file differs from NUL.
        # --check prints actual whitespace findings on stdout; LF/CRLF advice is stderr only.
        if result.stdout or result.returncode not in (0, 1):
            report['whitespaceErrors'].append({'path': relative, 'output': result.stdout+result.stderr})
    for name in documents:
        path = root/name; text = path.read_text(encoding='utf-8')
        ignored = git('check-ignore', name).returncode == 0
        report['documents'][name] = {'bytes': path.stat().st_size, 'ignored': ignored}
        if name.startswith('docs/'): assert ignored, name
        for link in re.findall(r'\[[^\]]*\]\(([^)]+)\)', text):
            if link.startswith(('https:', 'http:', '#')): continue
            destination = link.split('#')[0].strip('<>')
            if destination and not (path.parent/unquote(destination)).exists():
                report['missingLinks'].append({'document': name, 'target': link})
    qa = (root/documents[1]).read_text(encoding='utf-8'); learning = (root/documents[0]).read_text(encoding='utf-8')
    report['answers'] = len(re.findall(r'^## \d+\.', qa, re.M))
    report['experiments'] = len(re.findall(r'^### 实验\d', learning, re.M))
    for line in (root/'generated/prompt19-initial-status.txt').read_text(encoding='utf-8-sig').splitlines():
        if len(line) > 3 and not (root/line[3:].strip('"')).exists(): report['missingPriorFiles'].append(line)
    report['head'] = git('rev-parse', 'HEAD').stdout.strip()
    report['gitignoreUnchanged'] = not git('diff', '--', '.gitignore').stdout
    report['docsTracked'] = git('ls-files', 'docs').stdout.splitlines()
    report['pythonOutsideReconstruction'] = [p for p in git('ls-files', '--cached', '--others', '--exclude-standard').stdout.splitlines() if p.endswith('.py') and not p.startswith(('tools/reconstruction/', 'external/'))]
    report['passed'] = not any(report[key] for key in ('missingLinks', 'missingPriorFiles', 'whitespaceErrors', 'docsTracked', 'pythonOutsideReconstruction')) and report['answers'] >= 24 and report['experiments'] >= 4 and report['gitignoreUnchanged']
    target.write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    (root/'generated/prompt19-final-status.txt').write_text(git('status', '--short').stdout, encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
