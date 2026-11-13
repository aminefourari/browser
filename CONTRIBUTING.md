# Contributing

Thanks for helping build an open browser. This file is the rulebook: issues, pull
requests, reviews, and code. If something here is unclear, open a Discussion.

**Security issues never go in public issues or PRs.** See [SECURITY.md](SECURITY.md).

## Quick start

```sh
pnpm install
scripts/test_core.sh   # C++ core logic + tests (needs clang++ and googletest)
pnpm test              # WebUI logic tests
pnpm typecheck
pnpm dev               # command bar in a normal browser, with fake data
```

You don't need to build Chromium to contribute. Most work is core logic (plain C++
with tests) or WebUI (TypeScript). See [docs/PLAN.md](docs/PLAN.md) §5.

---

## Issues

### Before opening one

1. **Search first**, including closed issues. If one exists, add a 👍 reaction, not a
   "+1" comment. Comment only if you have new information (a repro, a log, a use case).
2. **Questions and open-ended ideas go to [Discussions](https://github.com/aminefourari/browser/discussions)**,
   not issues. Issues are for things we can act on.
3. **One problem per issue.** Two bugs = two issues.

### Writing a good issue

- Use a template. Blank issues are disabled.
- **Bugs:** exact steps, what you expected, what happened, version and OS. A bug we
  can't reproduce gets `needs-info`; if there's no reply in **14 days**, it's closed
  (reopen anytime with the details).
- **Feature requests:** start from the problem, not the solution. "I lose track of
  research tabs" beats "add tab groups". Say whether Arc had it and how it worked.
- Titles are short and specific: "Split View loses focus after switching Spaces", not
  "bug" or "Split View broken".

### Issue lifecycle (labels)

| Label | Meaning |
|---|---|
| `triage` | New, not looked at yet. Every issue starts here. |
| `needs-info` | We asked a question. Closed after 14 days without an answer. |
| `accepted` | We want this. **Only `accepted` issues are ready for a PR.** |
| `good first issue` | Small, well-defined, good for a first contribution. |
| `help wanted` | Accepted and open for anyone to take. |
| `blocked` | Waiting on something else (another issue, upstream Chromium, a decision). |
| `duplicate` / `wontfix` / `invalid` | Closed, with a comment saying why. |
| `area:*`, `platform:*` | Which part of the browser and which OS. |

Maintainers aim to triage new issues within a week.

### Claiming an issue

- Comment **"I'd like to work on this"** and wait for a maintainer to assign you.
  Don't start on an unassigned feature: someone else may already be on it.
- One claimed issue at a time if you're new here.
- **Claims expire.** No PR or update in **14 days** → we unassign so someone else can
  pick it up. Just say if you need more time.
- Tiny fixes (typos, obvious one-line bugs) don't need an issue. Just open the PR.

---

## Pull requests

### Rules

1. **Linked issue.** Every PR except tiny fixes links an `accepted` issue with
   `Closes #123`. PRs for features nobody agreed to may be closed, however good the code.
2. **One concern per PR.** A bug fix doesn't also reformat files or rename things.
   Refactors go in their own PR.
3. **Small.** Aim for under ~400 changed lines (excluding tests and lockfiles). Split
   big features into steps that each work on their own: model first, then UI, then glue.
4. **Title = Conventional Commit.** We squash-merge, so the PR title becomes the commit
   on `main`: `feat(sidebar): auto-archive Today tabs`, `fix(importer): handle empty Spaces`.
5. **Signed off.** Every commit has a DCO sign-off (`git commit -s`); see below.
6. **Green CI.** All checks must pass. Don't ask for review on a red PR.
7. **Tests.** New logic comes with tests; bug fixes come with a test that fails
   without the fix.
8. **Screenshots** (or a short recording) for every visible UI change, light and dark mode.
9. **Template filled in.** Including how you tested it.
10. **Work in progress = draft PR.** Mark it ready only when you'd merge it yourself.

### AI-assisted contributions

Welcome, with the same bar as everything else. You must have read, run and understood
every line, and be able to explain it in review. Say in the PR if a large part was
generated. PRs that look unreviewed by their author get closed.

### Review

- Every PR needs **one maintainer approval**. Changes to security-sensitive areas
  (Mojo interfaces, WebUI bindings, permissions, Chromium patches, the updater, Shields)
  need two once the project has two maintainers.
- We aim for a first review within a week. A polite ping after that is fine.
- **Reply to review comments** with a change or a reason. The person who opened a
  thread resolves it.
- Push fixes as new commits during review (easier to re-review); we squash on merge.
  Rebase on `main` only when there are conflicts.
- **Inactive PRs:** no response to review for **30 days** → closed. Reopen when you're back.
- Maintainers may push small fixes to your branch to get it merged. Leave "Allow edits
  by maintainers" checked.

### Branch names

`feat/split-view`, `fix/tab-crash`, `docs/readme`, `refactor/sidebar-model`.
Branch from `main`, in your fork.

---

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/):

```
feat(sidebar): auto-archive unpinned tabs after 12h
fix(split-view): keep focus on the active pane
docs: explain profile partitions
```

Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`.
Scopes: `sidebar`, `spaces`, `split-view`, `command-bar`, `importer`, `shields`, `boosts`,
`build`, …

## Sign-off (DCO)

We use the [Developer Certificate of Origin](https://developercertificate.org/) instead of
a CLA. Sign every commit:

```
git commit -s -m "feat: ..."
```

This adds `Signed-off-by: Your Name <you@example.com>`, certifying you wrote the code or
have the right to submit it under the project license (MPL-2.0).

## Code rules

- **Touch Chromium as little as possible.** Prefer, in order: new code in this repo →
  a `chromium_src/` override → a small `.patch`. PRs that add patches explain why the
  other two options don't work.
- **Core logic stays Chromium-free.** `components/*/core/` uses the C++ standard library
  only (no `base::`, no `<chrono>`), so it builds and tests without Chromium.
- C++ follows the [Chromium style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
  (`clang-format`, `gn format`). TypeScript is `strict`; Lit elements use
  `static properties`, no decorators.
- **Security is not optional.** Every new Mojo interface, WebUI binding or permission is
  called out in the PR description. Code that parses files or web data rejects bad input
  instead of crashing.
- New source files start with the MPL-2.0 header:
  `This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.`
- Simple beats clever. Don't add a dependency for what a few lines can do.
- Never use Arc's or Brave's names, logos or assets in the product (trademarks).

## Conduct

Everyone follows the [Code of Conduct](CODE_OF_CONDUCT.md): in issues, PRs, reviews and
Discussions. Be kind, assume good faith, critique code rather than people.
