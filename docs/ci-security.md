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
| Test / Python API | One independent run per OS, not one per build configuration |
| Analyze | Focused clang-tidy diagnostics and address/undefined-behavior sanitizers |
| Quality Summary | Requires every applicable group to pass |

All six native OS/configuration combinations belong to the single
`build-and-test` matrix. Windows uses the Visual Studio generator; Linux and
macOS use Ninja. Build, CTest, artifacts, and the required aggregate result are
shared across that matrix.

Linux Debug is instrumented for coverage and uses its native test run to produce
coverage reports. There is no duplicate Linux unit-test or coverage build.
FlatBuffers schemas are compiled by every native build rather than checked again
with a potentially different system flatc. A shared setup action bootstraps the
manifest-pinned vcpkg checkout and caches dependency binary packages only.

CodeQL is called once by CI. Its standalone workflow supports weekly scheduled
scans on the default branch and manual dispatch; it has no duplicate push/PR scan.
C++ is extracted on Linux, Intel macOS 15, and Windows Server 2022. JavaScript,
Python, and workflow analysis run on Linux. Report categories include the OS.

## Gates

- All native and Python API tests pass on all three platforms. Native tests time
  out after 120 seconds. Optional browser smoke tests skip without Playwright.
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

Fork PRs enforce CodeQL locally without attempting privileged SARIF upload.
All reports remain available as artifacts after a findings-gate failure.

## Testing and maintenance

Push a feature branch and open its CI run in GitHub Actions. Verify the run's SHA
matches the commit being tested; GitHub cannot read uncommitted workspace edits.
Manual dispatch with a selected branch is available once the dispatch-enabled
workflow exists on the default branch. Open a PR to additionally test dependency
review and the proposed merge commit.

Local checks:

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
