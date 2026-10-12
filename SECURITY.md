# Security Policy

## Reporting a vulnerability

Open a **private security advisory** on GitHub. Don't open a public
issue for security reports.

What helps: which component (injector, dumper, menu DLL), Windows
version, and steps to reproduce.

## What counts

The injector runs as administrator and manual-maps a DLL into another
process. That makes its own code worth a look:

- Privilege or handle mishandling in the injector.
- Unsafe parsing in the dumper (it walks untrusted game memory and
  writes files to disk).
- Anything in the menu DLL that corrupts the host process beyond the
  game's own address space.

Not security issues:

- Getting detected by anti-cheat or banned from Combat Master. That's
  the documented risk of running this, not a vulnerability — see the
  Disclaimer in the README.
- Stale offsets after a game patch. That's maintenance, covered in
  `docs/OFFSETS.md`.
- Anything in vendored third-party code (`third_party/`) — report
  upstream.

## Scope

In scope: this repo's own code — injector, dumper, menu DLL.

Out of scope: third-party libraries, the game itself, Steam,
anti-cheat systems.

## What to expect

Indie project, no bug bounty, no SLA. I'll acknowledge your report,
fix confirmed issues, and credit you unless you'd rather stay
anonymous.
