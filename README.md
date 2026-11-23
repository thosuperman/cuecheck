# CueCheck

CueCheck checks a SubRip (`.srt`) caption file before you deliver a cut. It points to the exact line where a cue ends before it starts, overlaps another speaker or breaks your reading-speed limit. Use it on one export in PowerShell or on a batch in CI. It never rewrites captions, opens a video or uploads subtitles. Use local files for offline operation.

## Try it on Windows

1. Get `cuecheck.exe` and `cuecheck.exe.sha256` from the [official releases](https://github.com/thosuperman/cuecheck/releases), or [build from source](#build-and-test). No installer, administrator rights or separate runtime needed for the x64 portable build.
2. In PowerShell, compare `(Get-FileHash .\cuecheck.exe -Algorithm SHA256).Hash` to the digest in the checksum file. A checksum checks file integrity; it **does not** authenticate the publisher. The executable is currently unsigned. Follow your organization's download policy.
3. Run it from the folder containing the executable:

```powershell
.\cuecheck.exe "episode 1.en.srt"
.\cuecheck.exe --max-cps 20 --max-line 32 "episode 1.en.srt" "episode 1.vi.srt"
```

To uninstall, delete the executable. There are no settings, services or registry entries.

## See a finding

In the source tree, try the [rough cut](examples/rough-cut.srt):

```text
> cuecheck examples/rough-cut.srt
"examples/rough-cut.srt":6: warning [overlap] Overlaps the cue timed on line 2.
"examples/rough-cut.srt":10: error [duration] Set the end time after the start time.
"examples/rough-cut.srt": 3 cue(s), 2 issue(s)
```

Compare with the [clean sample](examples/clean.srt). Overlapping dialogue can be intentional, so overlap is a warning, not an error.

A missing blank line can make a cue number indistinguishable from spoken numbers. To recover, CueCheck treats a digits-only caption line as the next cue number only when followed by a right-pointing timing arrow with a full SubRip-shaped endpoint (both endpoints for a malformed arrow). Other numbers and dash-separated ranges stay dialogue. If a literal caption exactly mimics a numbered timing header, split the caption differently. Recovery is not guaranteed for arbitrary damage.

| Check | What it reports |
|---|---|
| Structure | Missing numbers, timestamps, text or blank separators between numbered cues; nonconsecutive numbering |
| Timing | Invalid `HH:MM:SS,mmm` ranges, zero or negative duration, out-of-order starts and overlaps (including nested intervals) |
| Readability | Optional `--max-cps N` (characters per second) and `--max-line N` (characters per line); no editorial thresholds imposed by default |

Readability counts Unicode code points, not bytes, grapheme clusters or screen width. It ignores lowercase `<b>`, `<i>`, `<u>` and their closing tags, trims exterior spaces/tabs, and counts one space between nonempty lines for reading speed. Pick thresholds for the language and delivery spec.

Input is UTF-8 (optional BOM), up to 4 MiB per regular file, with LF, CRLF or CR line endings. Cue timestamps use two-digit hours. WebVTT, positioning extensions and media/audio analysis are outside this version's scope.

## Automation

```sh
cuecheck --strict --json --max-cps 20 cut.en.srt cut.vi.srt > caption-report.json
```

JSON output is an array in input order. Each file has `file`, `cues` (number of cue blocks encountered) and `issues`; each issue has a one-based `line` (0 for input errors), `severity`, `code` and `message`. Exit **0** means no errors, **1** means caption errors (also warnings with `--strict`), **2** means a usage or input-file problem. An unreadable file does not stop other files being checked. Use `--` before filenames beginning with a dash.

## Build and test

Requires a C++17 compiler, CMake 3.16+ (3.20+ for the `ctest --test-dir` command below) and Python 3 for tests; no external application libraries. On Windows, use Visual Studio 2022 with Desktop development with C++ and the Windows SDK. Append `-A x64` to the configure command with the Visual Studio generator.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The executable is `build/Release/cuecheck.exe` with Visual Studio or `build/cuecheck` on Linux. [GitHub Actions](https://github.com/thosuperman/cuecheck/actions/workflows/build.yml) tests the executable on both systems and archives the exact tested Windows build.

## Feedback and security

Caption editors: [report a confusing result](https://github.com/thosuperman/cuecheck/issues) with the CueCheck version, command, expected result and a *synthetic*, minimal caption sample. Don't upload a client's subtitles. Contributions are welcome: add a regression case and open a pull request. See the [security policy](SECURITY.md) for private reports.

## Code signing policy

**Unsigned.** CueCheck does not have a SignPath Foundation certificate or endorsement. See [Code signing policy and release process](SIGNING.md) for scope, privacy and verification. [MIT license](LICENSE).
