"""Validate LunarisV JSON and schemas without configuring or compiling C++."""
import copy
import json
from pathlib import Path

from jsonschema import Draft7Validator
from referencing import Registry, Resource

ROOT = Path(__file__).resolve().parents[1]
SCHEMAS = ROOT / 'schemas' / 'v3'
registry = Registry()
schemas = {}
for path in SCHEMAS.glob('*.json'):
    schema = json.loads(path.read_text(encoding='utf-8'))
    Draft7Validator.check_schema(schema)
    schema['$id'] = path.as_uri()
    schemas[path.stem] = schema
    registry = registry.with_resource(path.as_uri(), Resource.from_contents(schema))
    assert path.read_bytes() == (ROOT / 'staging/schemas/v3' / path.name).read_bytes()

def validator(name):
    return Draft7Validator(schemas[name + '.schema'], registry=registry)

rules = json.loads((ROOT / 'staging/cfg/rules.official.json').read_text(encoding='utf-8'))
validator('rules').validate(rules)
assert 'update_url' not in rules['file_info'], 'Upstream update would erase LunarisV rules'

builtin = ['cheater', 'suspicious', 'exploiter', 'racist', 'suspected_cheater',
           'blacklisted', 'vac_banned', 'game_banned', 'sourcebanned', 'pedophilia']
example = {'$schema': '../schemas/v3/playerlist.schema.json', 'players': [
    {'steamid': '[U:1:1234]', 'attributes': builtin + ['custom:watchlist'],
     'proof': ['test', {'source': 'test'}], 'last_seen': {'time': 1700000000}}
]}
validator('playerlist').validate(example)
bundled_path = ROOT / 'staging/cfg/playerlist.lunarisv.json'
if bundled_path.exists():
    bundled = json.loads(bundled_path.read_text(encoding='utf-8'))
    validator('playerlist').validate(bundled)
    assert bundled['file_info']['title'] == 'LunarisV Player List'
    assert 'update_url' not in bundled['file_info']
    ids = [player['steamid'] for player in bundled['players']]
    assert len(ids) == len(set(ids)), 'Duplicate converted SteamIDs'
    print(f'PASS: {len(ids)} bundled LunarisV players validate against the extended schema')
for excluded in ['ignore', 'ignored', 'party', 'friend', 'friends', 'everyone', 'f2p',
                 'sore_losers', 'sore-losers', 'sorelosers']:
    for tag in [excluded, 'custom:' + excluded]:
        invalid = copy.deepcopy(example)
        invalid['players'][0]['attributes'] = [tag]
        assert not validator('playerlist').is_valid(invalid), tag
for tag in ['custom:', 'custom:Uppercase', 'custom:a b', 'custom:' + 'a'*65, 'custom:0start']:
    invalid = copy.deepcopy(example)
    invalid['players'][0]['attributes'] = [tag]
    assert not validator('playerlist').is_valid(invalid), tag

shared = {'description': 'fixture', 'sources': ['chat', 'sourcebans'],
          'triggers': {'chatmsg_text_match': {'mode': 'word', 'patterns': ['hostile']}},
          'actions': {'mark': ['racist', 'custom:watchlist'],
                      'transient_mark': ['custom:session'], 'unmark': ['blacklisted']}}
validator('rules').validate({'rules': [shared]})
for sources in [[], ['invalid'], ['chat', 'chat'], 'sourcebans']:
    invalid = dict(shared, sources=sources)
    assert not validator('rules').is_valid({'rules': [invalid]}), sources
legacy = dict(shared)
del legacy['sources']
validator('rules').validate({'rules': [legacy]})
Draft7Validator(schemas['settings.schema']['properties']['general']).validate({'auto_mark_bans': True})

print(f'PASS: {len(schemas)} schemas, staging copies, default rules, expanded tags, custom tags, exclusions, rule sources and settings')
print('C++ compilation and runtime tests are intentionally not performed by this script.')
