# Java QD upstream

The reference implementation is `devexperts/QD`. The exact baseline is stored in `upstream.lock`; changes to that file are deliberate compatibility events, not routine dependency updates.

## Verified baseline

The lock was verified on 2026-09-20 against the public upstream repository:

| Lock field | Locked value | Upstream evidence |
| --- | --- | --- |
| `repository` | `https://github.com/devexperts/QD.git` | The URL is fetchable as a Git repository, and a clone reports the same URL for `origin`. The public repository contains the release tag and commit cited below. The repository metadata caveat in [Open questions](upstream/OPEN-QUESTIONS.md#provenance) remains recorded. |
| `version` | `3.355` | The locked commit declares `<version>3.355</version>` in the [root `pom.xml`](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/pom.xml), starts [the release notes](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/ReleaseNotes.txt) with `QDS 3.355`, and is the target of annotated tag `qd-qds-3.355`. |
| `commit` | `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` | Peeling annotated tag `qd-qds-3.355` with `git rev-parse "refs/tags/qd-qds-3.355^{commit}"` produces the locked commit. The commit subject is `tagging release 3.355`. |

These three observations establish that the public repository, release identity, and immutable Git object agree. They do not assert that the current branch head will continue to point at this revision.

## How upstream is used

- As a behavioral oracle for wire, schema, encoding, and state-machine questions.
- As a source of tests, resources, and examples that reveal contracts.
- As historical evidence when behavior is undocumented.
- Not as an architecture template that must be copied into C++.

## Traceability convention

Research notes and specifications should cite stable evidence where practical:

- repository and commit;
- module and relative file path;
- class, method, resource, or test name;
- relevant line range or symbol;
- observation command or fixture-generation procedure.

If a conclusion is inferred rather than explicit, label it as an inference and describe how it can be verified.

The top-level modules initially relevant to schema, encodings, and QTP are listed in [the module inventory](upstream/MODULE-INVENTORY.md). The inventory is a navigation aid, not a semantic specification.

## Obtain and inspect the locked revision

The following procedure uses a separate checkout and an explicit detached commit. The directory name is only an example and must remain outside this repository; upstream source is not vendored here.

```text
git clone --filter=blob:none --no-checkout https://github.com/devexperts/QD.git java-qd
git -C java-qd fetch origin refs/tags/qd-qds-3.355:refs/tags/qd-qds-3.355
git -C java-qd checkout --detach 0ddc0a09779fb61d74cc0b37c6449ff47dcb715f
git -C java-qd remote get-url origin
git -C java-qd rev-parse HEAD
git -C java-qd rev-parse "refs/tags/qd-qds-3.355^{commit}"
git -C java-qd show HEAD:pom.xml
git -C java-qd show HEAD:ReleaseNotes.txt
```

The URL command must print `https://github.com/devexperts/QD.git`. Both `rev-parse` commands must print `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Inspect the root POM and the first release-notes section for `3.355`. A later upstream branch movement does not invalidate this checkout because checkout is by full commit ID and the release tag is verified separately.

For a source citation, use a commit-qualified path such as:

```text
https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/<relative-path>
```

## Updating the lock

An upstream update is a reviewed compatibility change, not a floating dependency refresh. Perform it in a dedicated task and:

1. identify the candidate release tag and peel it to an immutable commit;
2. verify the candidate version from the root POM and release notes;
3. review release notes and the diff from the currently locked commit, concentrating on every compatibility surface in [the compatibility policy](COMPATIBILITY.md);
4. update affected specifications, provenance notes, inspection tools, fixtures, and conformance expectations in the same change;
5. record uncertainties in [upstream open questions](upstream/OPEN-QUESTIONS.md) instead of guessing;
6. update all fields in `upstream.lock` only after the evidence is reviewable, then rerun the checkout and tag-peeling verification above; and
7. confirm that no upstream checkout or copied source was added to this repository.

The review should state the old and new commits, the release tag, observed compatibility impact, regenerated evidence, and any deliberate deviation. A newer upstream revision alone is not sufficient reason to change the lock.
