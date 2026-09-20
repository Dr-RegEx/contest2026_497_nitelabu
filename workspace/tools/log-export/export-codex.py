#!/usr/bin/env python3
"""Export visible dialogue from a real Codex rollout using the official writer.
No reasoning, system prompts, tool payloads or unrelated sessions are exported.
Usage: export-codex.py OFFICIAL_COLLECTOR_ROOT ROLLOUT_JSONL TEAM_REPO
"""
import hashlib
import json
import pathlib
import sys

collector, source, repo = map(pathlib.Path, sys.argv[1:])
sys.path.insert(0, str(collector / 'adapters/shared'))
import snapshot_core as official

raw = source.read_bytes()
records = [json.loads(line) for line in raw.splitlines() if line]
meta = records[0]['payload']
if 'esp32s31-openvela' not in meta.get('cwd', ''):
    raise SystemExit('Refusing a session outside this project')
sid = meta['id']
events = []
for record in records:
    if record.get('type') != 'response_item':
        continue
    item = record.get('payload', {})
    if item.get('type') != 'message' or item.get('role') not in ('user', 'assistant'):
        continue
    if item.get('channel') in ('analysis', 'summary'):
        continue
    for block in item.get('content', []):
        if block.get('type') in ('input_text', 'output_text') and block.get('text'):
            events.append(dict(ts=record['timestamp'], role=item['role'], text=block['text']))
if not events:
    raise SystemExit('No visible dialogue found')
team = 'contest2026_497_nitelabu'
login = 'Dr-RegEx'
member = repo / 'logs' / login
member.mkdir(parents=True, exist_ok=True)
date = events[0]['ts'][:10]
target = member / date / f'codex__{sid}.jsonl'
if target.exists():
    raise SystemExit(f'Already exported: {sid}')
result = official.append_events(target, events, sid, team, login, 'codex', 1,
                                official.load_redact_rules(member))
manifest = official.read_manifest(member, team, login, 'codex')
manifest['sessions'].append(dict(session_id=sid, tool='codex',
    started_at=events[0]['ts'], last_event_at=events[-1]['ts'],
    event_count=result['written'], raw_event_count=len(records),
    file_path=str(target.relative_to(repo)), collection_mode='cli', health='ok'))
official.write_manifest(member, manifest, team, login, 'codex')
(repo / 'logs' / 'export-provenance.json').write_text(json.dumps(dict(
    session_id=sid, source_sha256=hashlib.sha256(raw).hexdigest(),
    source_bytes=len(raw), selection='Visible user and assistant dialogue only; partial ongoing session snapshot',
    official_collector_commit='10743591d1034480ecee7c8ffffe9bb251d4474d',
    events=result['written']), indent=2) + '\n')
print('Exported', result['written'], 'events from', sid)
