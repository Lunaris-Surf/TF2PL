# LunarisV configuration

LunarisV preserves the TF2BD v3 `players`, `steamid`, `attributes`, `proof`, and
`last_seen` structure. The extended tag vocabulary and rule `sources` field require
LunarisV; older TF2BD builds may reject them. Existing TF2BD lists and rules remain
readable. The supplied legacy player file is converted and bundled as
`cfg/playerlist.lunarisv.json`, titled **LunarisV Player List**.

The bundled list contains 33,472 player records, including 28,599 available
last-seen names. Legacy account IDs are represented as `[U:1:account_id]`.
No observation timestamps exist in the source, so preserved names use the
existing unknown-time value `0`. All allowed tag combinations are retained.
Excluded tags are filtered during conversion; none were assigned in this source.
The original file is not modified. Imported labels are inherited, not independently
verified, and no evidence or observation dates are invented.

The list is stored in `staging/cfg`, which both existing Windows and Linux
workflows package, including the AppImage's accompanying `cfg` folder. LunarisV
loads it automatically as an additional player list, alongside the official list
and the user's editable `cfg/playerlist.json`. It has no remote update URL.
To remove an imported label, edit the bundled list itself; local unmarking does
not override additional lists.

To refresh the conversion, run
`python util/convert_lunarisv_players.py <legacy-Players.json> staging/cfg/playerlist.lunarisv.json --force`.
Unknown tag definitions or invalid IDs stop conversion instead of being silently
dropped. Converter checks run with `python util/test_convert_lunarisv_players.py`.

| JSON attribute | Menu label |
| --- | --- |
| `cheater` | Cheater |
| `suspected_cheater` | Suspected Cheater |
| `suspicious` | Suspicious |
| `exploiter` | Exploiter |
| `racist` | Racist/Hostile |
| `blacklisted` | Blacklisted |
| `vac_banned` | VAC Banned |
| `game_banned` | Game Banned |
| `sourcebanned` | SourceBanned |
| `pedophilia` | Pedofilia/Pedo Jokes |

Ignore/ignored, party, friend/friends, sore losers, everyone, and F2P are excluded
from the tag vocabulary. Existing party and Steam-friend functionality is retained.

Right-click a player and open **Mark** to toggle built-in tags or add a custom tag.
Custom IDs use `custom:` followed by 1–64 lowercase letters, digits, underscores,
or hyphens, starting with a letter, for example `custom:watchlist`. Enter only
`watchlist` in the menu. Tags used in loaded player lists become reusable menu
entries. Custom tags survive saving, list merging, and rule actions; a player
with only custom tags is retained. Checkmarks represent editable saved tags;
subscribed lists are not altered by the local menu. A custom tag disappears from
the reusable menu when no loaded player has it anymore.

## One rule for in-game chat and SourceBan reasons

Place additional rules in `cfg/rules.json` or another `cfg/rules.*.json` file.
This complete example uses a fictional phrase:

```json
{
  "$schema": "../schemas/v3/rules.schema.json",
  "rules": [
    {
      "description": "Example shared review rule",
      "sources": ["chat", "sourcebans"],
      "triggers": {
        "chatmsg_text_match": {
          "mode": "contains",
          "case_sensitive": false,
          "patterns": ["example review phrase"]
        }
      },
      "actions": {"mark": ["custom:review"]}
    }
  ]
}
```

`sources` accepts `chat`, `sourcebans`, or both. Omission preserves legacy in-game
behavior, including existing username/avatar rules. SourceBan scanning feeds the
ban reason into the same `chatmsg_text_match`; that matcher must match before a
ban can trigger the rule. Other supplied triggers retain their `match_all` or
`match_any` semantics and inspect the current player, not a historical username.
`mark`, `transient_mark`, and `unmark` accept built-in and custom attributes.
Transient marks are not serialized; `unmark` removes editable saved attributes.
Rules run in file order. A matching SourceBan/rule pair is applied once while
that player object remains loaded; editing a rule changes its identity and allows
it to run again. New chat messages are evaluated as they arrive.

The shipped defaults include shared Racist/Hostile and Pedofilia/Pedo Jokes
patterns, plus SourceBan-only patterns for administrator reason labels such as
"racial slurs" or "pedo jokes". Patterns are case-insensitive ECMAScript regular
expressions and match the entire input, so substring rules use `[\s\S]*` around
their expression. The defaults cover common English phrases and selected
spellings; context, quotations, negation, other languages, and novel evasion can
cause misses or false positives. Review saved evidence before treating a label
as a conclusion about someone. The combined pedophilia/jokes tag does not
distinguish an actual offense from a joke or an administrator's allegation.

## Ban integration

Keep **Enable Auto-mark** and **Auto-mark ban records** enabled in the moderation
menu. Configure the existing Steam API integration for VAC/game records. For
SourceBans, enable **SteamHistory Integration** in Settings and supply your API
key. No key is embedded or added to this repository.

VAC and game-ban counts create independent history tags. They do not identify
which game the ban concerned or provide a reason to scan. SourceBan history is
loaded through the fork's existing SteamHistory endpoint and all returned ban
reasons are available to the shared rules. Revoked (`Unbanned`) records contribute
to the history tag but are skipped for behavior marking; expired bans remain
historical evidence. Failed, disabled, pending, or malformed responses do not
create tags. Existing batch retry behavior is retained.

Ban tags persist as history until manually removed. A successful empty lookup
does not erase previous or manually assigned tags. Manual removal lasts for the
current loaded player session; reloading configuration or a later session may
find the same ban again.
Saved evidence identifies Steam API or SteamHistory, the matching rule, and the
SourceBan server, state, and reason. Default ban/behavior/custom tags do not
activate the existing cheater-only automatic votekick path. A user-written rule
that explicitly marks `cheater` retains the existing cheater behavior.

SteamHistory documents its SourceBans capability in its
[FAQ](https://steamhistory.net/faq); detailed API documentation requires login.
The existing endpoint and response contract were retained and hardened, but no
authenticated live API check was performed. Steam ban counts follow the existing
[ISteamUser GetPlayerBans integration](https://partner.steamgames.com/doc/webapi/ISteamUser).

## Files and branding

The window, setup text, product resources, and nonportable application data folder
use **LunarisV**. Nonportable installs now use the `LunarisV` app-data folder;
copy any wanted prior configuration there explicitly. Portable installs continue
using their `cfg` directory. CMake target, library, and executable names retain
`tf2_bot_detector` so the existing build scripts still find them.

Bundled schemas are in `staging/schemas/v3` for distribution and `schemas/v3` for
development. Extended player lists/rules serialize their schema reference as
`../schemas/v3/<type>.schema.json`. Old upstream schema URLs remain readable.
The LunarisV default rules no longer carry an upstream `update_url`, preventing
an update from replacing these rules. Other subscribed lists are unchanged.

## Verification and builds

No local configuration, compilation, or application launch was performed.
The existing GitHub Actions were left unchanged as requested. The existing
Windows workflow contains a cached-executable replacement step, so review it
before relying on its artifacts to represent fresh source changes.

Run `python util/validate_lunarisv.py` with `jsonschema` installed and
`node util/check_lunarisv_patterns.cjs` for checks that do not compile the project.
Additional C++ regression cases are in `Tests/PlayerRuleTests.cpp`; these have
not been compiled or executed. The inherited test setup still references Catch2
v2 headers while the dependency manifest requests v3, so that existing test
infrastructure needs updating before enabling it in a build.

After a future build, verify the player menu, save/restart roundtrip, a custom-only
player entry, one shared rule on both sources, delayed API completion, disabled
integrations, and manual unmarking. No workflow was dispatched and no GitHub
release was created during this change.
