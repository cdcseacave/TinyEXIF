# Security Policy

TinyEXIF parses metadata out of untrusted JPEG files, so a bug in it is a bug in every
application that links it and opens a file it did not create. Three of the last few reports
against this library have been the same class of issue: an attacker-controlled offset or length
read or allocated without validation. If you have found another one, please read on.

## Supported versions

Only the latest released version and the `master` branch are supported. There is no backport
policy for older tags -- if you are pinned to an old version, please upgrade before reporting,
or say so explicitly in your report if you cannot.

## Reporting a vulnerability

Preferred: open a [GitHub private security advisory](https://github.com/cdcseacave/TinyEXIF/security/advisories/new)
for this repository. If that option is not available to you (advisories are not guaranteed to be
enabled at the time you are reading this), email **cdc.seacave@gmail.com** instead, with `TinyEXIF
security` in the subject line.

Please include, if you have it:
- The smallest input file that reproduces the issue.
- The observed effect (crash, sanitizer report, hang, excessive memory use) and, ideally, a
  stack trace or sanitizer output.
- Whether you have a suggested fix.

Do not open a public issue for a suspected vulnerability before it has been triaged privately.

## What to expect

This is a small library maintained on a best-effort basis, not a project with a funded security
team or a contracted SLA. Realistically: reports have sometimes sat for weeks before getting a
response. There is no guaranteed response or fix timeline, and none is promised here -- but every
report is read, and memory-safety issues in particular are taken seriously and prioritised over
other work once seen. If you have not heard back and it has been a while, a polite follow-up
email is welcome and will bump the report back to the top.

## Credit and CVE requests

Reporters are credited by name (or handle) in the fix's commit message and/or release notes,
unless you ask to remain anonymous. If you need a CVE assigned for disclosure purposes, say so in
your report; GitHub security advisories can mint one directly once a fix is confirmed.

## Scope

In scope: memory-safety bugs (out-of-bounds read/write, use-after-free, unbounded
attacker-controlled allocation), integer overflow affecting parsing correctness or safety, and
similar issues reachable by handing this library a crafted JPEG/EXIF/XMP file. Denial-of-service
via pathological but *bounded* CPU/memory use is welcome to report but may be treated as
lower-severity than an out-of-bounds access.

Out of scope: issues that require the calling application to already trust the input file (this
library assumes JPEG/EXIF/XMP content is untrusted by design), and vulnerabilities in
dependencies (report those upstream, e.g. to [tinyxml2](https://github.com/leethomason/tinyxml2)).
