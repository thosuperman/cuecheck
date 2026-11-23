# Code signing policy

**Status: unsigned.** No signing provider has accepted this project, and the build does not invoke a signing service. This policy does not authorize signing unrelated software.

| Responsibility | Policy |
|---|---|
| Source owner, reviewer and release approver | [thosuperman](https://github.com/thosuperman), owner of the [official repository](https://github.com/thosuperman/cuecheck). External contributors propose changes by pull request; the maintainer reviews source, tests and workflow changes before release. |
| Artifact eligible for future signing | Only the project's own Windows x64 `cuecheck.exe` built by GitHub Actions from a reviewed release commit. Never local uploads, third-party tools or builds from forks. |
| Privacy | The program reads the files you specify and writes diagnostics to the console. It has no telemetry or network client. Explicit Windows UNC and device paths are rejected; use a local disk for offline operation, since your OS may fetch mounted or cloud-backed files. |

## Publish an unsigned release

1. Review changes and merge through a pull request. Confirm the version in CMake and a successful Windows and Linux build on the exact main commit.
2. From that commit create and push the corresponding version tag (for example, `v0.1.0`). The [workflow](.github/workflows/build.yml) rebuilds, runs the CLI tests on both platforms and verifies the Windows product name, original filename and product version against the executable and tag.
3. The release job downloads the Windows artifact from that same run, verifies its checksum and publishes it to [Releases](https://github.com/thosuperman/cuecheck/releases) with a link to the run. It does not accept a binary URL or local file from the requester. Inspect the public download, digest and notes. Never replace a published binary with a different build at the same version.

## Before requesting a Foundation certificate

The [Foundation's conditions](https://signpath.org/terms) call for an existing release in the form to be signed, active maintenance and a verifiable reputation for downloadable executables. A passing build and license are not evidence of adoption.

- Publish the portable unsigned binary. Give caption editors the example files and fix problems they actually report. Keep links to genuine independent use, feedback, fixes and follow-up releases; do not claim users or endorsements you cannot verify.
- Confirm MFA for everyone with repository or signing access, restrict changes to release branches and tags, and confirm who may review contributions and approve each signing request. These are owner/account controls, not settings this file enforces.
- When there is a track record, the maintainer can [apply](https://signpath.org/apply) with the repository, public download, CI history, this policy and the evidence. Approval is discretionary; this repository is not a submission.
- Only after acceptance, bind SignPath to the trusted CI artifact, restrict PE metadata to product `CueCheck`, original file `cuecheck.exe` and the release version, and require manual approval for each release. Verify Authenticode on the signed result before publishing it and generating its final checksum. Add the Foundation's required attribution to the home/download pages only when it is true.
