# CI layout and policy

The required merge check is **Quality Summary** in **Apogeo CI Pipeline**.
The workflow runs from the pushed branch on every push, and from GitHub's proposed
merge commit for pull requests to main/develop or merge-queue entries.

## Run groups

| Group | Checks |
| --- | --- |
| Validate | JSON, Python syntax, native source whitespace |
| Security | Secret history scan, workflow lint, PR dependency review, CodeQL |
| Test / Native | Linux, macOS, Windows; Debug and Release; C++ and simulator process tests |
| Test / Web API | One independent run per OS, not one per build configuration |
| Test / Web browser Chromium | Required real headless Chromium smoke tests on Linux, macOS, and Windows |
| Analyze | Focused clang-tidy diagnostics and address/undefined-behavior sanitizers |
| Quality Summary | Requires every applicable group to pass |

All six native OS/configuration combinations belong to the single
`build-and-test` matrix. Windows uses the Visual Studio generator; Linux and
macOS use Ninja. Build, CTest, artifacts, and the required aggregate result are
shared across that matrix.

macOS native, web API and browser jobs use `macos-15-intel`, matching CodeQL,
with the `x64-osx` native triplet. GitHub's ARM64 runner pool previously cancelled
jobs before any steps ran because no runner could acquire them. This avoids that
specific pool; it does not guarantee runner availability. macOS coverage is Intel,
not Apple Silicon. Infrastructure cancellations still block the quality gate.

Native tests have separate executables, CTest labels, CI steps and JUnit reports:

| Suite label | Location | Scope |
| --- | --- | --- |
| `core` | `tests/core` | Engine, configuration, state loading, integration algorithms, logging, output, timing, flight computer and IPC helpers |
| `module_structures` | `plugins/structures/tests` | Structures module |
| `module_aerodynamics` | `plugins/aerodynamics/tests` | Aerodynamics module |
| `module_propulsion` | `plugins/propulsion/tests` | Propulsion module |
| `module_example_plugin` | `plugins/example_plugin/tests` | Example plugin physics |
| `integration` | `tests/integration` | Dynamic plugin lifecycle and actual simulator-process protocol |

Web API and browser tests remain under `tools/web_server/tests` and run in their
own jobs. CI policy regression tests remain under `tests/test_ci_*.py`.
CI requires every discovered native test to belong to exactly one suite and every
suite to be nonempty. All suites run even if another suite fails, provided the
build succeeded. Coverage combines the same native runs without duplicate builds.
Deprecated modules remain outside the supported test scope.
GoogleTest discovery runs at CTest time, after all build-time runtime DLL copies
have completed. This also applies to CI's test-inventory validation step.

To run a single suite, replace `core` with any label above:

```sh
ctest --test-dir <build-directory> -C Release -L '^core$' --output-on-failure --no-tests=error
cmake --build <build-directory> --config Release --target core_tests
```

Other executable targets are `structures_tests`, `aerodynamics_tests`,
`propulsion_tests`, `example_plugin_tests`, and `plugin_integration_tests`.

Linux Debug is instrumented for coverage and uses its native test run to produce
coverage reports. There is no duplicate Linux unit-test or coverage build.
FlatBuffers schemas are compiled by every native build rather than checked again
with a potentially different system flatc. A shared setup action bootstraps the
manifest-pinned vcpkg checkout and caches dependency binary packages only.

CodeQL is called once by CI. Its standalone workflow supports weekly scheduled
scans on the default branch and manual dispatch; it has no duplicate push/PR scan.
C++ is extracted on Linux, Intel macOS 15, and Windows Server 2022. JavaScript,
Python, and workflow analysis run on Linux. Report categories include the OS.

Archived directories (`tools/deprecated`, `plugins/deprecated`, `docs/deprecated`,
and `data/deprecated`) are excluded from CodeQL source discovery and routine file
validation. clang-tidy excludes deprecated plugin sources. Manual C++ CodeQL
extraction follows the build; deprecated plugin targets are disabled by default.
The active `tools/web_gui.py` entry point and `tools/web_server` remain covered.
Secret scanning still covers all reachable Git history, including archived files.

## Gates

- All native, Python API, and Chromium smoke tests pass on all three platforms.
  Native tests time out after 120 seconds. CI installs pinned Playwright and its
  Chromium build (including Linux system libraries). Missing dependencies, launch
  failures, missing/empty browser reports, and skipped browser cases fail the gate.
  API jobs exclude the browser module because the browser matrix owns that suite.
  Each browser job retains its JUnit report and per-test Playwright trace archives.
  These smoke tests use a fake IPC simulator and stub external CDN resources;
  they verify panel/backend behavior, not chart rendering or physical simulation.
- Project src/plugins line coverage is at least **80%**, including unexecuted
  translation units and excluding tests and generated/vendor files.
- clang-tidy runs bugprone, CERT, analyzer, portability, and performance checks.
  CERT/analyzer findings and tool/compiler errors are blocking. At most **10**
  other diagnostics are allowed. Cosmetic naming, capitalization, braces and
  legacy C++ modernization suggestions are outside the merge gate.
- AddressSanitizer, leak sanitizer and UndefinedBehaviorSanitizer have no allowed
  failures. Prebuilt dependencies are not instrumented.
- No detected secrets in reachable Git history.
- CodeQL uses **security-extended**. General style/maintainability queries from
  security-and-quality are intentionally not duplicated as security gates.
  Medium-or-higher security findings and selected unclassified warnings/errors
  block. Missing reports, scanner failures, invalid rule references and invalid
  severity values fail closed. SARIF suppression/baseline flags do not bypass this.
- Reviewed false positives require an exact rule ID, file and current source
  statement in .github/codeql/reviewed-findings.json, plus a rationale. The gate
  reports each match and retains the unmodified SARIF. The only current exception
  is the local CLI's explicitly user-selected configuration file. It does not
  exclude the path-injection query or remote web filesystem access.
- PR dependency review blocks newly introduced moderate-or-higher vulnerabilities
  in supported runtime/development dependencies. This is not a complete native
  dependency audit; review vcpkg and Eigen updates separately.

## Repository settings

Protect main and develop with required pull requests, the **Quality Summary**
check from GitHub Actions, up-to-date branches or merge queues, and restricted
bypass/direct-push permissions. Require trusted review of workflow, scanner policy,
reviewed exceptions, dependency and test changes. Configure real maintainers in
CODEOWNERS if using required code-owner review. Workflow files cannot apply these
repository settings. Enable the dependency graph and code scanning, including
appropriate GitHub security entitlements for private repositories.

Dependency review needs only `contents: read`; `pull-requests: write` is needed
only when posting PR comments, which this workflow disables. If its diff API
returns 403, verify the repository's dependency graph is enabled under Settings /
Advanced Security, then check token permissions, rate limits and any required
private-repository entitlement. Enable dependency graph and Dependabot alerts
before rerunning the failed job. Do not bypass an unavailable API with warn-only
mode or skipped checks. CI pins pytest 9.2.0; development installs require at
least 9.0.3 to exclude the vulnerable temporary-directory handling tracked in
[GHSA-6w46-j5rx-g56g](https://github.com/advisories/GHSA-6w46-j5rx-g56g).

Fork PRs enforce CodeQL locally without attempting privileged SARIF upload.
All reports remain available as artifacts after a findings-gate failure.

## Testing and maintenance

Push a feature branch and open its CI run in GitHub Actions. Verify the run's SHA
matches the commit being tested; GitHub cannot read uncommitted workspace edits.
Manual dispatch with a selected branch is available once the dispatch-enabled
workflow exists on the default branch. Open a PR to additionally test dependency
review and the proposed merge commit.

Local checks:

- python -m pip install -r tools/ci/requirements-browser.txt
- python -m playwright install chromium
- python -m pytest tools/web_server/tests -q
- python -m unittest discover -s tests -p test_ci_sarif.py -v
- ctest --test-dir <build-directory> -C <configuration> --output-on-failure
- actionlint -shellcheck= .github/workflows/ci.yml .github/workflows/codeql.yml
- git diff --check

Inspect failures under the group that owns the failed check. Download API JUnit,
native CTest, coverage HTML/LCOV, clang-tidy, and codeql-language-os SARIF artifacts
as applicable. Syntax tests do not establish that hosted builds or security scans
pass. Passing CI reduces risk; it does not prove correctness or security.

The current physics implementation also has explicit limitations: aerodynamics
force/torque output remains neutral, and propulsion engine-cycle and mass-flow
calculations are placeholders. A passing pipeline verifies the implemented
contracts and regression tests; it does not establish that these models are
complete, physically validated, or ready for production flight use.

External actions are pinned to commits. Dependabot updates actions and Python
dependencies weekly against develop. Review pinned Go security tools and native
package baselines regularly.
