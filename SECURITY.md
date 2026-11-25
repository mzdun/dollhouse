# Security policy

## Supported versions

Dollhouse is still before 1.0. Only the latest release gets security fixes; older releases are not patched, so the fix for a reported problem ships in the next release.

## Reporting a vulnerability

Please do not report security problems in public issues, discussions or pull requests.

Report them privately through GitHub instead: open the [Security tab](https://github.com/mzdun/dollhouse/security) of the repository and press **Report a vulnerability**. Only you and the maintainers can see the report.

If you cannot use GitHub, send an email to [marcin.zdun+dollhouse@gmail.com](mailto:Marcin%20Zdun%20%28dollhouse%20issues%29%20%3Cmarcin.zdun%2Bdollhouse%40gmail.com%3E?subject=%5Bdollhouse%20security%5D%20) with `[dollhouse security]` at the start of the subject.

A useful report includes:

- the version or commit you tested,
- the platform and compiler, if you built dollhouse yourself,
- the steps or the input file that trigger the problem,
- what happens (a crash, a sanitizer report, wrong output) and what an attacker could gain from it.

## What to expect

This is a project maintained in spare time, so the answers are best effort. You should hear back within a week. Once the problem is confirmed, the fix is prepared in private and released together with a GitHub Security Advisory, which credits you unless you ask not to be named.

## Scope

The most interesting parts are the ones reading data from outside:

- loading glTF 2 and GLB files, including the active objects and their rules (`libs/pyxel-io`),
- the `shapes` Python module (`extras/shapes`),
- the `verify` tool (`extras/verify`).

Problems in third-party dependencies should go to their own projects, unless dollhouse uses them in an unsafe way.
