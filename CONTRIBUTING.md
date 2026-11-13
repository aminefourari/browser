# Contributing

Thanks for helping build an open browser. This file is the rulebook.

## Before you start

- **Bugs**: open an issue with the bug template.
- **Features**: open an issue or Discussion first. Anything bigger than a small fix needs
  a maintainer 👍 before you write code — it saves you a rejected PR.
- **Security**: never in public. See [SECURITY.md](SECURITY.md).
- Check [docs/PLAN.md](docs/PLAN.md) to see where your idea fits in the roadmap.

## Workflow

1. Fork, then branch from `main`: `feat/split-view`, `fix/tab-crash`, `docs/readme`.
2. Keep PRs small and focused — one concern per PR.
3. Make sure lint, typecheck and tests pass.
4. Open the PR against `main` and fill in the template.
5. A maintainer reviews; one approval and green CI are required to merge.
   We squash-merge, so the PR title becomes the commit message.

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/):

```
feat(sidebar): auto-archive unpinned tabs after 12h
fix(split-view): keep focus on the active pane
docs: explain profile partitions
```

Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`.

## Sign-off (DCO)

We use the [Developer Certificate of Origin](https://developercertificate.org/) instead of
a CLA. Sign every commit:

```
git commit -s -m "feat: ..."
```

This adds `Signed-off-by: Your Name <you@example.com>`, certifying you wrote the code or
have the right to submit it under the project license.

## Code rules

- TypeScript, `strict` mode. No `any` without a comment explaining why.
- **Security is not optional.** Web content never gets Node access. Every new IPC channel,
  preload API or permission is called out in the PR description and needs maintainer review.
- Simple beats clever. Don't add a dependency for what a few lines can do.
- UI changes include a screenshot or short recording.
- Logic with branches or parsing comes with a test.

## Labels

`good first issue`, `help wanted`, `bug`, `enhancement`, `triage`, `security`,
`area:sidebar`, `area:spaces`, `area:command-bar`, `area:extensions`, `platform:mac|win|linux`.
