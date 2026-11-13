# CLAUDE.md

Open-source, sidebar-first browser inspired by Arc, built on Chromium the way Brave is
(our code as a thin layer on top of upstream Chromium). Read `docs/PLAN.md` first: it is
the source of truth for architecture, features and roadmap.

## Status

Phase 0 (repo + plan). No code yet. When phase 1 lands, add the real init/build/test/lint
commands here (planned: `scripts/` wrapping depot_tools, gclient, GN and Ninja).

## Stack

Chromium (version pinned in `CHROMIUM_VERSION`), C++ for browser UI (Views) and services,
TypeScript for WebUI pages, Mojo for IPC, GN/Ninja, Python/Node build scripts, adblock-rust
for Shields.

## Conventions

- Change Chromium in this order of preference: new code in `src/` → `chromium_src/`
  override → small `.patch` in `patches/`. Patch count is a cost; keep it low.
- C++ follows the Chromium style guide (`clang-format`, `gn format`).
- Conventional Commits, signed off (`git commit -s`), squash-merge PRs into `main`.
- Any new Mojo interface, WebUI binding or permission must be called out in the PR for security review.
- Never use the "Arc" or "Brave" names or assets in the product (trademarks).
- License is MPL-2.0; new source files carry the MPL-2.0 header.
- Contributor rules: `CONTRIBUTING.md`. Security process: `SECURITY.md`.
