# CI quality and security policy

The required aggregate check is **Quality Summary** in **Apogeo CI Pipeline**.
It requires every build, test and security job to succeed. Dependency Review is
only applicable to pull requests; other skipped or cancelled jobs fail the gate.
Also require **CodeQL Security Gate** from **CodeQL Advanced**. The standalone
CodeQL workflow runs on every branch push, pull requests to `main`/`develop`,
merge queues, manual dispatch, and its weekly schedule. Its reusable entry point
is retained for CI callers. If CI also calls it, separate standalone and reusable
runs are expected; the standalone check protects branches independently of CI edits.

## Limits

- At least 80% line coverage across project `src` and `plugins` code, including
  unexecuted translation units. Generated files and tests are excluded.
- All discovered unit tests pass; each test has a 120 second timeout.
- At most 10 general clang-tidy warnings. Analyzer and CERT diagnostics, compiler
  errors, and failed analyzer execution are blocking regardless of that allowance.
- Zero AddressSanitizer, leak sanitizer, or UndefinedBehaviorSanitizer failures
  in the instrumented project and tests. Prebuilt dependencies are not instrumented.
- Zero secrets detected in reachable Git history by Gitleaks.
- Zero CodeQL security findings with severity 4.0 or higher (medium, high,
  critical). Unclassified warning/error findings also block. Missing or failed
  analysis cannot pass. Each language is checked separately.
- Pull requests cannot introduce dependencies with known moderate or worse
  vulnerabilities identified by GitHub Dependency Review, including development
  dependencies. This covers dependencies supported by GitHub's dependency graph;
  it is not a complete audit of vcpkg, Eigen, or native transitive dependencies.

These are project policy choices, not proof that passing code is secure. Existing
coverage gaps or findings must be fixed to meet the new limits; the workflow does
not add broad exclusions or automatically accept existing security findings.

## Repository settings required for merge enforcement

For both `main` and `develop`, configure branch protection or a ruleset to:

1. Require pull requests and the **Quality Summary** status check from GitHub Actions.
   Also require **CodeQL Security Gate** from the standalone CodeQL workflow.
2. Require branches to be up to date, or enable a merge queue (CI handles `merge_group`).
3. Require review of CI workflows, security policy scripts, scanner configuration,
   and dependency changes by a trusted maintainer. Assign real maintainers in
   CODEOWNERS and require code-owner review if using that mechanism.
4. Block direct pushes and bypasses, including administrator bypass where appropriate.

Enable the dependency graph and code scanning. CodeQL and Dependency Review need
the appropriate GitHub security entitlement for private repositories. Fork pull
requests run CodeQL gates locally without attempting to upload code scanning data.
Scheduled CodeQL scans run weekly on the default branch. Each CodeQL language
has an independent job and the aggregate gate fails on any failed, skipped, or
cancelled analysis. Scanner failures cannot count as a clean result.

CodeQL uses `security-and-quality`, which includes security-extended and
correctness queries. Medium-or-higher security findings and unclassified
warning/error findings block with no baseline allowance. Suppression markers and
unchanged-result flags in SARIF do not bypass this local gate. Low-severity security
findings remain visible in the uploaded reports. Failed scanner invocations, error
notifications, missing reports, invalid severities, and invalid rule references fail.
C++ builds project targets and tests in Debug mode under CodeQL tracing; dependencies
are installed before tracing. Only dependency binary packages are cached, so a
cached project object cannot silently escape extraction. This Linux build does not
cover code compiled exclusively under Windows or macOS platform conditions.

## Testing CodeQL on a feature branch

The CI pipeline also runs on pushes to every branch. Push the current branch to
test its own committed `ci.yml` and the reusable CodeQL workflow from the same
commit; no merge into `develop` is required. A pull request additionally runs
Dependency Review and tests GitHub's proposed merge commit.

Run the gate regression tests and workflow validation locally:

```powershell
python -m unittest discover -s tests -p test_ci_sarif.py -v
actionlint -shellcheck= .github/workflows/codeql.yml
git diff --check
```

These tests exercise report parsing and failure behavior; they do not run CodeQL
queries against the application. A full CodeQL run requires the updated files to
be committed and pushed. The push trigger scans the pushed branch automatically,
including `ci_pipeline_refactor`; no default-branch merge or pull request is needed.
GitHub cannot scan edits that exist only in the local workspace.

In GitHub Actions, open **CodeQL Advanced**, select the run whose commit SHA
matches the pushed branch, and check all four **Analyze** jobs plus
**CodeQL Security Gate**. Download `codeql-actions`, `codeql-c-cpp`,
`codeql-javascript-typescript`, and `codeql-python` for the SARIF reports. Reports
are retained even if the findings gate fails after analysis. To recheck a downloaded
report directory locally, run `python tools/ci/check_sarif.py <report-directory>`.

Once the dispatch-enabled workflow exists on the default branch, **Run workflow**
can also target a selected feature branch. Until then, use its branch push trigger.
Fix findings and push again to rerun the same gates; do not lower limits or add a
blanket baseline to get a green check.

Workflow files cannot configure repository branch protection. These settings must
be applied in GitHub before failing CI reliably prevents merging.

## Maintenance

External actions are pinned to immutable commits and Dependabot proposes weekly
action and Python dependency updates against `develop`. Review native dependency
baseline updates separately. Runners are pinned to Ubuntu 24.04 and macOS 15.
Vcpkg is pinned to the project manifest baseline and recovers if its cache is
unavailable. Security tools installed with Go use fixed versions and Go module
checksum verification; review those version pins regularly as well.

Run `python -m unittest discover -s tests -p test_ci_sarif.py -v` and `actionlint`
when changing the security gate or workflows. Review the first hosted run before
requiring the aggregate check. Passing local syntax tests does not validate hosted
builds, scanners, coverage, or current vulnerability findings.
