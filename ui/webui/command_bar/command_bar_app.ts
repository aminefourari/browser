/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import {css, html, LitElement, nothing, type TemplateResult} from 'lit';

import type {CommandBarBackend} from './backend.ts';
import {buildResults, type Result} from './command_bar_logic.ts';

const SEARCH_ICON = html`<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="8.5" cy="8.5" r="5.5"/><path d="m13 13 4 4"/></svg>`;
const GLOBE_ICON = html`<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="10" cy="10" r="7"/><path d="M3 10h14M10 3c2 2.2 2.8 4.5 2.8 7s-.8 4.8-2.8 7c-2-2.2-2.8-4.5-2.8-7S8 5.2 10 3Z"/></svg>`;
const COMMAND_ICON = html`<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M7 7h6v6H7zM7 7H5.5A2.5 2.5 0 1 1 7 5.5zm6 0V5.5A2.5 2.5 0 1 1 14.5 7zm0 6h1.5a2.5 2.5 0 1 1-1.5 1.5zm-6 0v1.5A2.5 2.5 0 1 1 5.5 13z"/></svg>`;

function hostOf(url: string): string {
  try {
    return new URL(url).host || url;
  } catch {
    return url;
  }
}

// Arc-style command bar: one box for URLs, search, open tabs, commands and
// history. ↑/↓ choose, Enter opens, Esc closes.
export class CommandBarAppElement extends LitElement {
  static override properties = {
    query: {state: true},
    results: {state: true},
    selected: {state: true},
  };

  declare query: string;
  declare results: Result[];
  declare selected: number;
  backend: CommandBarBackend|null = null;
  private requestId = 0;

  constructor() {
    super();
    this.query = '';
    this.results = [];
    this.selected = 0;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.search('');
  }

  override firstUpdated() {
    this.shadowRoot?.querySelector('input')?.focus();
  }

  private async search(query: string) {
    if (!this.backend) {
      return;
    }
    // Responses can arrive out of order; only the latest one counts.
    const id = ++this.requestId;
    const found = await this.backend.search(query);
    if (id !== this.requestId) {
      return;
    }
    this.results = buildResults(query, found);
    this.selected = 0;
  }

  private onInput(e: Event) {
    this.query = (e.target as HTMLInputElement).value;
    this.search(this.query);
  }

  private onKeyDown(e: KeyboardEvent) {
    const count = this.results.length;
    switch (e.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        e.preventDefault();
        if (count) {
          const step = e.key === 'ArrowDown' ? 1 : -1;
          this.selected = (this.selected + step + count) % count;
          this.scrollSelectedIntoView();
        }
        break;
      case 'Enter': {
        e.preventDefault();
        const result = this.results[this.selected];
        if (result) {
          this.activate(result);
        }
        break;
      }
      case 'Escape':
        e.preventDefault();
        this.backend?.close();
        break;
    }
  }

  private async scrollSelectedIntoView() {
    await this.updateComplete;
    this.shadowRoot?.getElementById(`result-${this.selected}`)
        ?.scrollIntoView({block: 'nearest'});
  }

  private activate(result: Result) {
    const backend = this.backend;
    if (!backend) {
      return;
    }
    switch (result.kind) {
      case 'url':
        backend.openUrl(result.url);
        break;
      case 'search':
        backend.searchWeb(result.query);
        break;
      case 'tab':
        backend.switchToTab(result.tab.id);
        break;
      case 'command':
        backend.runCommand(result.command.id);
        break;
      case 'history':
        backend.openUrl(result.page.url);
        break;
    }
    backend.close();
  }

  private renderResult(result: Result): TemplateResult {
    switch (result.kind) {
      case 'url':
        return this.renderRow(GLOBE_ICON, result.url, '', 'Open');
      case 'search':
        return this.renderRow(SEARCH_ICON, result.query, '', 'Search');
      case 'tab':
        return this.renderRow(this.renderLetter(result.tab.url),
                              result.tab.title, hostOf(result.tab.url),
                              'Switch to Tab');
      case 'command':
        return this.renderRow(COMMAND_ICON, result.command.title, '',
                              result.command.shortcut ?? '');
      case 'history':
        return this.renderRow(this.renderLetter(result.page.url),
                              result.page.title, hostOf(result.page.url), '');
    }
  }

  // ponytail: first letter of the host until the browser layer serves
  // favicons (chrome://favicon2).
  private renderLetter(url: string): TemplateResult {
    const letter = hostOf(url).replace(/^www\./, '').charAt(0).toUpperCase();
    return html`<span class="letter">${letter}</span>`;
  }

  private renderRow(icon: TemplateResult, title: string, subtitle: string,
                    hint: string): TemplateResult {
    return html`
      <span class="icon">${icon}</span>
      <span class="text">
        <span class="title">${title}</span>
        ${subtitle ? html`<span class="subtitle">${subtitle}</span>` : nothing}
      </span>
      ${hint ? html`<span class="hint">${hint}</span>` : nothing}`;
  }

  override render() {
    const open = this.results.length > 0;
    return html`
      <div class="panel" role="dialog" aria-label="Command bar">
        <div class="input-row">
          <span class="icon">${SEARCH_ICON}</span>
          <input type="text" .value=${this.query}
              placeholder="Search or enter URL…"
              spellcheck="false" autocomplete="off"
              role="combobox" aria-expanded=${open}
              aria-controls="results" aria-autocomplete="list"
              aria-activedescendant=${open ? `result-${this.selected}` : ''}
              @input=${this.onInput} @keydown=${this.onKeyDown}>
        </div>
        ${open ? html`
          <ul id="results" role="listbox" aria-label="Results">
            ${this.results.map((result, i) => html`
              <li id="result-${i}" role="option"
                  aria-selected=${i === this.selected}
                  @pointermove=${() => this.selected = i}
                  @mousedown=${(e: Event) => e.preventDefault()}
                  @click=${() => this.activate(result)}>
                ${this.renderResult(result)}
              </li>`)}
          </ul>` : nothing}
      </div>`;
  }

  static override styles = css`
    :host {
      --cb-accent: rgb(106 86 218);
      --cb-bg: rgb(255 255 255 / 0.92);
      --cb-text: rgb(24 24 27);
      --cb-muted: rgb(113 113 122);
      --cb-row-selected: color-mix(in srgb, var(--cb-accent) 14%, transparent);
      --cb-border: rgb(0 0 0 / 0.08);
      display: block;
      width: min(640px, calc(100vw - 32px));
      font: 14px/1.4 system-ui, -apple-system, sans-serif;
      color: var(--cb-text);
    }
    @media (prefers-color-scheme: dark) {
      :host {
        --cb-bg: rgb(30 30 34 / 0.92);
        --cb-text: rgb(244 244 245);
        --cb-muted: rgb(161 161 170);
        --cb-row-selected: color-mix(in srgb, var(--cb-accent) 30%, transparent);
        --cb-border: rgb(255 255 255 / 0.1);
      }
    }
    .panel {
      background: var(--cb-bg);
      backdrop-filter: blur(24px) saturate(1.4);
      border: 1px solid var(--cb-border);
      border-radius: 14px;
      box-shadow: 0 24px 64px rgb(0 0 0 / 0.25), 0 2px 8px rgb(0 0 0 / 0.1);
      overflow: hidden;
    }
    .input-row {
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 14px 16px;
    }
    input {
      flex: 1;
      min-width: 0;
      border: 0;
      outline: 0;
      background: transparent;
      color: inherit;
      font: inherit;
      font-size: 18px;
    }
    input::placeholder {
      color: var(--cb-muted);
    }
    ul {
      list-style: none;
      margin: 0;
      padding: 6px;
      max-height: min(420px, 60vh);
      overflow-y: auto;
      border-top: 1px solid var(--cb-border);
    }
    li {
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 8px 10px;
      border-radius: 8px;
      cursor: default;
    }
    li[aria-selected='true'] {
      background: var(--cb-row-selected);
    }
    .icon {
      flex: none;
      display: grid;
      place-items: center;
      width: 22px;
      height: 22px;
      color: var(--cb-muted);
    }
    svg {
      width: 18px;
      height: 18px;
      fill: none;
      stroke: currentColor;
      stroke-width: 1.6;
      stroke-linecap: round;
      stroke-linejoin: round;
    }
    .letter {
      display: grid;
      place-items: center;
      width: 22px;
      height: 22px;
      border-radius: 6px;
      background: var(--cb-accent);
      color: white;
      font-size: 12px;
      font-weight: 600;
    }
    .text {
      flex: 1;
      min-width: 0;
      display: flex;
      align-items: baseline;
      gap: 8px;
    }
    .title,
    .subtitle {
      overflow: hidden;
      white-space: nowrap;
      text-overflow: ellipsis;
    }
    .subtitle,
    .hint {
      color: var(--cb-muted);
      font-size: 12px;
    }
    .subtitle {
      flex: 0 1 auto;
    }
    .hint {
      flex: none;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'command-bar-app': CommandBarAppElement;
  }
}

customElements.define('command-bar-app', CommandBarAppElement);
