"""Inventory every historical validation record and its local evidence; do not infer passes."""
import hashlib
import json
import re
from pathlib import Path


def run(output):
    records=[]
    for path in sorted(Path('docs/validation').glob('*.md')):
        text=path.read_text(encoding='utf-8')
        evidence=[]
        for link in re.findall(r'\]\(([^)]+)\)',text):
            if '://' not in link and not link.startswith('#'):
                target=path.parent/link.split('#')[0]
                evidence.append(dict(path=link,exists=target.exists()))
        records.append(dict(record=path.as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            evidence=evidence,missing=[e['path'] for e in evidence if not e['exists']],
            declaredEvidence=[s for s in text.splitlines() if s.startswith('|')],
            limits=[s for s in text.splitlines() if any(word in s for word in ('未验证','未执行','跳过','人工桌面','没有重新','本阶段没有'))]))
    output.write_text(json.dumps(dict(records=records,scope='historical declarations and evidence presence only; current tests recorded separately'),ensure_ascii=False,indent=2),encoding='utf-8')
    for r in records:
        print(r['record'], 'evidence',len(r['evidence']), 'missing',r['missing'])


if __name__=='__main__':
    run(Path('generated/prompt32-history-audit.json'))
