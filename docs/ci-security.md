# CI quality and security policy

The required aggregate check is **Quality Summary** in **Apogeo CI Pipeline**.
It requires every build, test and security job to succeed. Dependency Review is
only applicable to pull requests; other skipped or cancelled jobs fail the gate.

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
2. Require branches to be up to date, or enable a merge queue (CI handles `merge_group`).
3. Require review of CI workflows, security policy scripts, scanner configuration,
   and dependency changes by a trusted maintainer. Assign real maintainers in
   CODEOWNERS and require code-owner review if using that mechanism.
4. Block direct pushes and bypasses, including administrator bypass where appropriate.

Enable the dependency graph and code scanning. CodeQL and Dependency Review need
the appropriate GitHub security entitlement for private repositories. Fork pull
requests run CodeQL gates locally without attempting to upload code scanning data.
Scheduled CodeQL scans run weekly on the default branch; push and PR scans run
through CI on both protected branches.

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
