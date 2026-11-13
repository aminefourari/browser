# CLAUDE.md

Open-source, sidebar-first browser inspired by Arc, built on Chromium the way Brave is
(our code as a thin layer on top of upstream Chromium). Read `docs/PLAN.md` first: it is
the source of truth for architecture, features and roadmap.

## Status

Phase 0 (repo + plan). **We are not building Chromium locally yet** (no capable machine).
Write features following `docs/PLAN.md` §5: core logic in plain C++20 with no Chromium
headers (compile and test it with the system `clang++`), WebUI pages in TypeScript tested
in a normal browser against a fake backend, and Chromium integration code kept thin and
checked by reading Chromium's source at `CHROMIUM_VERSION`. Never claim integration code
compiles; say it is unbuilt. Add real build/test commands here as they appear.

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
