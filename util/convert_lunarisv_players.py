"""Convert legacy account-ID tags to a LunarisV TF2BD v3 player list."""
import argparse
from collections import Counter
import json
from pathlib import Path

TAG_NAMES = {
    'Cheating/Cheater': 'cheater',
    'Suspected Cheater': 'suspected_cheater',
    'Suspicious': 'suspicious',
    'Exploiter': 'exploiter',
    'Racist/Hostile': 'racist',
    'Blacklisted': 'blacklisted',
    'VAC Banned': 'vac_banned',
    'Game Banned': 'game_banned',
    'SourceBanned': 'sourcebanned',
    'Pedofilia/Pedo Jokes': 'pedophilia',
}
EXCLUDED = {'ignore', 'ignored', 'party', 'friend', 'friends', 'sore losers', 'everyone', 'f2p'}

def convert(source):
    config, tags = source['Config'], source['Tags']
    names = source.get('LastSeenNames', {})
    mapping = {}
    for key, definition in config.items():
        name = definition['Name']
        if name.strip().lower() in EXCLUDED:
            mapping[key] = None
        elif name in TAG_NAMES:
            mapping[key] = TAG_NAMES[name]
        else:
            raise ValueError(f'Unmapped legacy tag {key}: {name!r}; add an explicit mapping first')

    players, counts = [], Counter()
    excluded_assignments = 0
    dropped = 0
    seen = set()
    for raw_id, raw_tags in sorted(tags.items(), key=lambda pair: int(pair[0])):
        account_id = int(raw_id)
        if not 0 < account_id <= 0xFFFFFFFF:
            raise ValueError(f'Expected a nonzero uint32 Steam account ID: {raw_id}')
        if account_id in seen:
            raise ValueError(f'Duplicate normalized account ID: {raw_id}')
        seen.add(account_id)
        if not isinstance(raw_tags, list):
            raise ValueError(f'Tags must be an array for account {raw_id}')
        attributes = set()
        for tag in raw_tags:
            if tag not in mapping:
                raise ValueError(f'Undefined tag {tag!r} for account {raw_id}')
            attribute = mapping[tag]
            if attribute is None:
                excluded_assignments += 1
            else:
                attributes.add(attribute)
        if not attributes:
            dropped += 1
            continue
        player = {'steamid': f'[U:1:{account_id}]', 'attributes': sorted(attributes)}
        # legacy supplies the name but no observation timestamp. TF2BD uses
        # zero for unknown time; never invent a current "last seen" date.
        name = names.get(raw_id)
        if isinstance(name, str) and name:
            player['last_seen'] = {'player_name': name, 'time': 0}
        players.append(player)
        counts.update(attributes)

    result = {
        '$schema': '../schemas/v3/playerlist.schema.json',
        'file_info': {
            'authors': ['LunarisV'],
            'title': 'LunarisV Player List',
            'description': 'LunarisV tags converted from the supplied legacy Players.json. Labels are inherited, not independently verified. Last-seen time 0 means unknown. Excluded tags were removed.',
        },
        'players': players,
    }
    report = {'source_players': len(tags), 'converted_players': len(players),
              'players_without_allowed_tags_omitted': dropped,
              'excluded_tag_assignments_removed': excluded_assignments,
              'players_with_preserved_names': sum('last_seen' in p for p in players),
              'attributes': dict(sorted(counts.items()))}
    return result, report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--force', action='store_true', help='Replace an existing output file')
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error('Input and output must be different files')
    if args.output.exists() and not args.force:
        parser.error('Output exists; use --force to replace it')
    source = json.loads(args.input.read_text(encoding='utf-8-sig'))
    result, report = convert(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=True, indent='\t') + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
