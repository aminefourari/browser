/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

// What the command bar needs from the browser. In the browser this becomes a
// Mojo interface backed by Chromium's tabs, history and search engine; for
// development, FakeBackend answers from sample data.

export interface PageMatch {
  title: string;
  url: string;
}

export interface TabMatch extends PageMatch {
  id: number;
}

export interface SearchResults {
  // Open tabs matching the query; with an empty query, recent tabs.
  tabs: TabMatch[];
  history: PageMatch[];
}

export interface CommandBarBackend {
  search(query: string): Promise<SearchResults>;
  openUrl(url: string): void;
  // Searches with the user's search engine.
  searchWeb(query: string): void;
  switchToTab(id: number): void;
  runCommand(id: string): void;
  close(): void;
}

const SAMPLE_TABS: TabMatch[] = [
  {id: 1, title: 'Inbox', url: 'https://mail.example.com/inbox'},
  {id: 2, title: 'Chromium Code Search', url: 'https://source.chromium.org/chromium'},
  {id: 3, title: 'Lit documentation', url: 'https://lit.dev/docs/'},
  {id: 4, title: 'Design review notes', url: 'https://docs.example.com/design-review'},
];

const SAMPLE_HISTORY: PageMatch[] = [
  {title: 'GitHub', url: 'https://github.com/'},
  {title: 'Lit documentation', url: 'https://lit.dev/docs/'},
  {title: 'MDN Web Docs', url: 'https://developer.mozilla.org/'},
  {title: 'Hacker News', url: 'https://news.ycombinator.com/'},
  {title: 'Wikipedia', url: 'https://en.wikipedia.org/wiki/Main_Page'},
];

function contains(page: PageMatch, query: string): boolean {
  const q = query.toLowerCase();
  return page.title.toLowerCase().includes(q) ||
      page.url.toLowerCase().includes(q);
}

export class FakeBackend implements CommandBarBackend {
  private onAction: (action: string) => void;

  constructor(onAction: (action: string) => void) {
    this.onAction = onAction;
  }

  async search(query: string): Promise<SearchResults> {
    // Random latency, so out-of-order responses get exercised.
    await new Promise(resolve => setTimeout(resolve, Math.random() * 60));
    const q = query.trim();
    if (!q) {
      return {tabs: SAMPLE_TABS.slice(0, 3), history: []};
    }
    return {
      tabs: SAMPLE_TABS.filter(tab => contains(tab, q)),
      history: SAMPLE_HISTORY.filter(page => contains(page, q)),
    };
  }

  openUrl(url: string) {
    this.onAction(`Open ${url}`);
  }

  searchWeb(query: string) {
    this.onAction(`Search the web for "${query}"`);
  }

  switchToTab(id: number) {
    this.onAction(`Switch to tab ${id}`);
  }

  runCommand(id: string) {
    this.onAction(`Run command "${id}"`);
  }

  close() {
    this.onAction('Close command bar');
  }
}
