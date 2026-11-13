/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {buildResults, classifyInput, matchesWords} from './command_bar_logic.ts';

test('classifyInput tells URLs from searches', () => {
  const cases: Array<[string, ReturnType<typeof classifyInput>]> = [
    ['', null],
    ['   ', null],
    ['github.com', {kind: 'url', url: 'https://github.com'}],
    ['  news.ycombinator.com/item?id=1 ', {kind: 'url', url: 'https://news.ycombinator.com/item?id=1'}],
    ['localhost:3000/api', {kind: 'url', url: 'http://localhost:3000/api'}],
    ['192.168.1.1', {kind: 'url', url: 'http://192.168.1.1'}],
    ['[::1]:8080', {kind: 'url', url: 'http://[::1]:8080'}],
    ['HTTPS://Example.com', {kind: 'url', url: 'HTTPS://Example.com'}],
    ['about:blank', {kind: 'url', url: 'about:blank'}],
    ['how to bake bread', {kind: 'search', query: 'how to bake bread'}],
    ['github.com is down', {kind: 'search', query: 'github.com is down'}],
    ['1.5', {kind: 'search', query: '1.5'}],
    ['note:', {kind: 'search', query: 'note:'}],
    ['javascript:alert(1)', {kind: 'search', query: 'javascript:alert(1)'}],
  ];
  for (const [input, expected] of cases) {
    assert.deepEqual(classifyInput(input), expected, input);
  }
});

test('matchesWords matches word prefixes in any order', () => {
  assert.ok(matchesWords('re tab', 'Reopen Closed Tab'));
  assert.ok(matchesWords('TAB clo', 'Reopen Closed Tab'));
  assert.ok(matchesWords('url', 'Copy URL'));
  assert.ok(!matchesWords('pen', 'Reopen Closed Tab'));
  assert.ok(!matchesWords('space x', 'New Space'));
});

test('buildResults orders input, tabs, commands, then unopened history', () => {
  const results = buildResults('lit', {
    tabs: [{id: 7, title: 'Lit docs', url: 'https://lit.dev/docs/'}],
    history: [
      {title: 'Lit docs', url: 'https://lit.dev/docs/'},
      {title: 'Lit playground', url: 'https://lit.dev/playground/'},
    ],
  });
  assert.deepEqual(results.map(r => r.kind), ['search', 'tab', 'history']);
  assert.equal(results[2]?.kind === 'history' && results[2].page.title,
               'Lit playground');

  const commands = buildResults('copy', {tabs: [], history: []});
  assert.deepEqual(commands.map(r => r.kind), ['search', 'command']);
});

test('buildResults with an empty query shows only recent tabs', () => {
  const results = buildResults('', {
    tabs: [{id: 1, title: 'A', url: 'https://a.com'}],
    history: [],
  });
  assert.deepEqual(results.map(r => r.kind), ['tab']);
});
