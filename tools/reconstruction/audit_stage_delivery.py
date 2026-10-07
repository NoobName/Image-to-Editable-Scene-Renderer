"""Audit ignored teaching deliverables independently from the committable diff."""
import argparse
import json
import re
import subprocess
from pathlib import Path


def run(stage, chapter):
    files = [Path(f'docs/learning/{stage}-{chapter}.md'), Path(f'docs/interview/{stage}-qa.md'),
             Path(f'docs/validation/{stage}-results.md'), Path('docs/README.md')]
    broken = []
    for path in files:
        text = path.read_text(encoding='utf-8')
        assert len(text) > 1000, path
        for link in re.findall(r'\]\(([^)]+)\)', text):
            if '://' not in link and not link.startswith('#') and not (path.parent / link.split('#')[0]).exists():
                broken.append((str(path), link))
    assert not broken, broken
    qa = files[1].read_text(encoding='utf-8')
    questions = len(re.findall(r'^## \d+\.', qa, re.M))
    experiments = len(re.findall(r'^### 实验\d', files[0].read_text(encoding='utf-8'), re.M))
    assert questions >= 24 and experiments >= 4
    for path in files:
        assert subprocess.run(['git', 'check-ignore', '-q', str(path)]).returncode == 0
    assert not subprocess.check_output(['git', 'diff', '--', '.gitignore'])
    subprocess.run(['git', 'diff', '--check'], check=True)
    untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z']).decode().split('\0')
    for name in filter(None, untracked):
        if Path(name).suffix == '.py':
            assert name.startswith('tools/reconstruction/'), name
        result = subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--no-index', '--check', '--', 'NUL', name], capture_output=True)
        assert result.returncode <= 1 and not result.stdout, (name, result.stdout.decode(errors='replace'))
    result = {'stage': stage, 'files': [str(p) for p in files], 'questions': questions,
              'experiments': experiments, 'brokenLinks': broken, 'docsIgnored': True, 'diffCheck': 'pass'}
    Path(f'generated/prompt{stage}-delivery-audit.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    p = argparse.ArgumentParser();p.add_argument('stage');p.add_argument('chapter');a = p.parse_args();run(a.stage, a.chapter)
