"""No-JUCE tests for VERSION.txt/CHANGELOG.md invariants and SemVer precedence."""
from __future__ import annotations

import unittest
from check_versioning import compare_version, check_changelog, semver, VersioningError


class ChangelogGateTest(unittest.TestCase):
    def test_version_order(self):
        self.assertLess(compare_version("0.9.0-beta.5", "0.9.0-beta.6"), 0)
        self.assertLess(compare_version("0.8.0-alpha.1", "0.9.0-beta.1"), 0)
        self.assertLess(compare_version("0.9.0-beta.10", "0.9.0"), 0)
        self.assertLess(compare_version("0.9.0-beta.9", "0.9.0-beta.10"), 0)
        self.assertEqual(compare_version("0.9.0+abc", "0.9.0+other"), 0)

    def test_invalid_semver(self):
        for value in ("v0.9", "0.9", "01.9.0", "0.09.0", "0.9.0-beta.01",
                      "0.9.0\n1.0.0", "0.9.0-"):
            with self.subTest(value=value), self.assertRaises(VersioningError):
                semver(value)

    def test_requires_a_matching_newest_entry(self):
        doc = """# Change log
## [0.9.0-beta.6] - 2026-10-31
### Aggiunto
- A substantial and verifiable modification recorded as part of this update.

## [0.9.0-beta.5] - 2026-10-28
### Modificato
- Older history must remain below newer changes and keep the former version.
"""
        check_changelog("0.9.0-beta.6", doc)
        with self.assertRaises(VersioningError):
            check_changelog("0.9.0-beta.7", doc)
        with self.assertRaises(VersioningError):
            check_changelog("0.9.0-beta.6", doc.replace("2026-10-31", "2026-13-31"))
        with self.assertRaises(VersioningError):
            check_changelog("0.9.0-beta.6", doc.replace("[0.9.0-beta.5]", "[0.9.0-beta.6]"))


if __name__ == "__main__":
    unittest.main()
