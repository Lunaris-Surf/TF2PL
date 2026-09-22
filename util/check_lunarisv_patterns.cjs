// Exercise the shipped ECMAScript patterns without compiling the application.
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const file = path.join(__dirname, '../staging/cfg/rules.official.json');
const rules = JSON.parse(fs.readFileSync(file, 'utf8')).rules.filter(r => r.description.startsWith('LunarisV:'));
const compiled = rules.map(r => ({ ...r, patterns: r.triggers.chatmsg_text_match.patterns.map(p => new RegExp(`^(?:${p})$`, 'i')) }));
function marks(text, source) {
  return [...new Set(compiled.filter(r => r.sources.includes(source) && r.patterns.some(p => p.test(text))).flatMap(r => r.actions.mark))].sort();
}
const cases = [
  ['go kill yourself', 'chat', ['racist']],
  ['KYS', 'chat', ['racist']],
  ['I WILL KILL YOUR FAMILY', 'chat', ['racist']],
  ['white power', 'chat', ['racist']],
  ['white power', 'sourcebans', ['racist']],
  ['racial slurs and harassment', 'sourcebans', ['racist']],
  ['racial slurs and harassment', 'chat', []],
  ['hostile conduct', 'sourcebans', ['racist']],
  ['pedophilia / pedo jokes', 'sourcebans', ['pedophilia']],
  ['pedo jokes', 'chat', []],
  ["I'm a proud pedophile", 'chat', ['pedophilia']],
  ['I like dating minors', 'chat', ['pedophilia']],
  ['age is just a number', 'chat', ['pedophilia']],
  ['age is just a number', 'sourcebans', ['pedophilia']],
  ['hello\nage is just a number\nbye', 'chat', ['pedophilia']],
  ['racism and pedo jokes', 'sourcebans', ['pedophilia', 'racist']],
  ['nice shot, good game', 'chat', []],
  ['I dislike racism', 'chat', []],
  ['please report pedophiles', 'chat', []],
  ['the clock is just a number display', 'chat', []],
  ['skyscraper', 'chat', []],
  ['duplicate account', 'sourcebans', []],
  ['', 'sourcebans', []],
];
for (const [text, source, expected] of cases) assert.deepEqual(marks(text, source), expected, `${source}: ${text}`);
console.log(`PASS: ${cases.length} positive/negative pattern fixtures across ${rules.length} default rules`);
console.log('These are ECMAScript pattern checks, not C++ execution tests.');
