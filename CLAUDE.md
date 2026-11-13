# CLAUDE.md

Open-source, sidebar-first browser inspired by Arc. Read `docs/PLAN.md` first: it is the
source of truth for architecture, features and roadmap.

## Status

Phase 0 (repo + plan). No application code yet. When phase 1 lands, add the real
build/test/lint commands here.

## Decided stack

Electron (castLabs ECS for Widevine) + electron-vite, TypeScript strict, React for the
chrome UI, better-sqlite3, pnpm, Vitest + Playwright.

## Conventions

- Conventional Commits, signed off (`git commit -s`), squash-merge PRs into `main`.
- Main process owns all state; renderer sends intents over typed IPC and renders pushed state.
- Web content: `sandbox: true`, `contextIsolation: true`, no preload, no Node. Any new IPC
  channel, preload API or permission must be called out in the PR.
- Never use the "Arc" name or assets in the product (trademark).
- Rules for contributors live in `CONTRIBUTING.md`; security process in `SECURITY.md`.
