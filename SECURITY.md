# Security Policy

## Reporting a vulnerability

Open a **private security advisory** on GitHub. Don't open a public
issue for security reports.

What helps: which version you're running, Windows version, and steps
to reproduce. If the issue involves a malicious offsets payload or a
crash PoC, describe it first — attach files only through the advisory.

## What counts

This tool fetches `offsets.json` from a remote service at startup and
parses it. That makes the offset loader and anything touching
network-fetched data the most security-relevant code here. Bugs in the
overlay rendering, config parsing, or memory abstraction are in scope
too.

Not security issues:

- Getting banned from a game. That's the game's anti-cheat doing its
  job, not a vulnerability in this project.
- The offset service being down or serving stale offsets. Report that
  to the service operator.
- Anything in the bundled third-party libraries (`ext/`) — report
  upstream.

## Scope

In scope: this repo's own code — the overlay, feature modules, offset
loader, config system.

Out of scope: third-party libraries, the offset service infrastructure,
Roblox itself, social engineering.

## What to expect

Indie project, no bug bounty, no SLA. I'll acknowledge your report,
fix confirmed issues, and credit you unless you'd rather stay
anonymous. This project will never inject into a game process, and
neither should your PoC — external observation only.
