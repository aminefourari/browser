/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import type {PageMatch, SearchResults, TabMatch} from './backend.ts';

export interface Command {
  id: string;
  title: string;
  shortcut?: string;
}

export const COMMANDS: Command[] = [
  {id: 'new-space', title: 'New Space'},
  {id: 'new-split-view', title: 'Add Split View', shortcut: '⌃⇧='},
  {id: 'toggle-pin', title: 'Pin or Unpin Tab', shortcut: '⌘D'},
  {id: 'copy-url', title: 'Copy URL', shortcut: '⌘⇧C'},
  {id: 'reopen-closed-tab', title: 'Reopen Closed Tab', shortcut: '⌘⇧T'},
  {id: 'toggle-sidebar', title: 'Toggle Sidebar', shortcut: '⌘S'},
  {id: 'open-archive', title: 'Open Archive'},
  {id: 'open-settings', title: 'Settings', shortcut: '⌘,'},
];

export type Result =
    {kind: 'url', url: string}|
    {kind: 'search', query: string}|
    {kind: 'tab', tab: TabMatch}|
    {kind: 'command', command: Command}|
    {kind: 'history', page: PageMatch};

const MAX_TABS = 5;
const MAX_COMMANDS = 3;
const MAX_HISTORY = 5;

const URL_SCHEMES =
    ['http:', 'https:', 'file:', 'about:', 'chrome:', 'view-source:', 'data:',
     'mailto:'];
// localhost, IPv4, [IPv6], or a dotted host with a letter TLD; optional port
// and path.
const HOST =
    /^(localhost|\d{1,3}(\.\d{1,3}){3}|\[[0-9a-f:]+\]|([a-z0-9-]+\.)+[a-z]{2,})(:\d{1,5})?([/?#].*)?$/i;

// Decides whether typed text is a URL to open or a web search.
// ponytail: no public-suffix list, so "node.js" counts as a URL. In the
// browser, Chromium's AutocompleteClassifier can make this call instead.
export function classifyInput(text: string):
    {kind: 'url', url: string}|{kind: 'search', query: string}|null {
  const input = text.trim();
  if (!input) {
    return null;
  }
  const lower = input.toLowerCase();
  if (URL_SCHEMES.some(scheme => lower.startsWith(scheme))) {
    return {kind: 'url', url: input};
  }
  if (!/\s/.test(input) && HOST.test(input)) {
    const local = /^(localhost|\d|\[)/i.test(input);
    return {kind: 'url', url: `${local ? 'http' : 'https'}://${input}`};
  }
  return {kind: 'search', query: input};
}

// True when every query word starts some word of `text`, ignoring case:
// "re tab" matches "Reopen Closed Tab".
export function matchesWords(query: string, text: string): boolean {
  const words = text.toLowerCase().split(/[^\p{L}\p{N}]+/u).filter(Boolean);
  return query.toLowerCase()
      .split(/\s+/)
      .filter(Boolean)
      .every(q => words.some(word => word.startsWith(q)));
}

// Result order, like Arc: what you typed (open/search), open tabs, commands,
// then history pages that aren't already open.
export function buildResults(text: string, found: SearchResults): Result[] {
  const input = classifyInput(text);
  const results: Result[] = [];
  if (input) {
    results.push(input);
  }
  for (const tab of found.tabs.slice(0, MAX_TABS)) {
    results.push({kind: 'tab', tab});
  }
  if (input) {
    for (const command of COMMANDS.filter(c => matchesWords(text, c.title))
             .slice(0, MAX_COMMANDS)) {
      results.push({kind: 'command', command});
    }
  }
  const open = new Set(found.tabs.map(tab => tab.url));
  for (const page of found.history.filter(page => !open.has(page.url))
           .slice(0, MAX_HISTORY)) {
    results.push({kind: 'history', page});
  }
  return results;
}
