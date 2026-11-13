# Security Policy

A browser is a security product. We take reports seriously.

## Reporting a vulnerability

**Do not open a public issue.** Report privately via
[GitHub Security Advisories](https://github.com/aminefourari/browser/security/advisories/new).

Include: affected version, OS, reproduction steps, and impact.

We aim to acknowledge within 72 hours and to ship a fix for critical issues within 14 days.
We credit reporters in the release notes unless you'd rather stay anonymous.

## Supported versions

Only the latest release receives security fixes. The browser auto-updates; please stay current.

## Scope

In scope: bugs in our code or patches — sandbox escapes, Mojo/WebUI privilege abuse,
cross-profile or cross-space data leaks, extension privilege escalation, URL/origin
spoofing in the UI, Shields bypasses, update-channel tampering, and delays in shipping
upstream Chromium security fixes.

Out of scope: vulnerabilities in upstream Chromium that are already public —
we track upstream and ship their fixes; please report those to the Chromium project.
