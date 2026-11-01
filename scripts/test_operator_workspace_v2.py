"""Offline regression gate for the standalone 12/13/11 operator UI concept.

It intentionally does not test the JUCE runtime. The HTML is a visually
reviewable design proposal, not a replacement for native Windows tests.
"""
from __future__ import annotations

import re
import unittest
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREVIEW = ROOT / "design" / "operator-workspace-v2.html"


class Markup(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids: set[str] = set()
        self.tags: list[str] = []
        self.script = ""
        self._script = False

    def handle_starttag(self, tag, attrs):
        self.tags.append(tag)
        fields = dict(attrs)
        if "id" in fields:
            assert fields["id"] not in self.ids, "Duplicate DOM ID"
            self.ids.add(fields["id"])
        if tag == "script":
            self._script = True

    def handle_endtag(self, tag):
        if tag == "script":
            self._script = False

    def handle_data(self, value):
        if self._script:
            self.script += value


def fixture_values(script: str, variable: str) -> list[str]:
    match = re.search(r"const\s+" + re.escape(variable) +
                      r"\s*=\s*\[([^\]]+)\]", script)
    if match is None:
        raise AssertionError("Missing fixture: " + variable)
    return re.findall(r'"([^"]+)"', match.group(1))


class OperatorWorkspacePrototypeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = PREVIEW.read_text(encoding="utf-8")
        cls.parsed = Markup()
        cls.parsed.feed(cls.source)

    def test_true_stress_density_fixtures(self):
        js = self.parsed.script
        self.assertEqual(len(fixture_values(js, "names")), 13)
        self.assertEqual(len(fixture_values(js, "types")), 11)
        self.assertEqual(len(fixture_values(js, "labels")), 11)
        self.assertIn("i<=12", js, "Prototype needs twelve cuelist tabs")
        self.assertIn("i<=13", js, "Prototype needs thirteen cues")
        self.assertIn("k<11", js, "Prototype needs eleven operational columns")
        self.assertIn("repeat(11,var(--cell))", self.source)

    def test_pinned_regions_and_keyboard_visible_controls(self):
        ids = self.parsed.ids
        self.assertTrue({
            "tabs", "matrixScroll", "matrix", "metricScroll", "metricRows",
            "inspector", "openInspector", "closeInspector", "dock",
            "dock-toggle", "timeline", "timeline-toggle", "density",
            "go", "panic", "statusText", "current-title", "cueName",
        }.issubset(ids))
        self.assertIn("position:sticky;left:0", self.source)
        self.assertIn("border-left:2px solid var(--teal)", self.source)
        self.assertIn("matrixScroll", self.parsed.script)
        self.assertIn("metricScroll", self.parsed.script)
        self.assertIn(":focus-visible", self.source)
        self.assertIn('role="tablist"', self.source)

    def test_no_production_or_external_execution(self):
        src = self.source
        self.assertIn("CONCEPT UI — NON OPERATIVO", src)
        self.assertIn("GO disabilitato", src)
        self.assertIn("PANIC non operativo", src)
        for forbidden in (
            "fetch(", "XMLHttpRequest", "WebSocket", "localStorage",
            "sessionStorage", "navigator.sendBeacon", "<iframe",
            "<link rel=", "<script src=", "AudioContext(", "eval(",
        ):
            self.assertNotIn(forbidden, src,
                             "Prototype must be self-contained and inert: " + forbidden)

    def test_separation_of_mixer_inspector_and_timeline(self):
        src = self.parsed.script
        self.assertIn('classList.add("hide")', src)
        self.assertIn('classList.toggle("collapsed")', src)
        self.assertIn('--rowh",active?"31px":"37px"', src)
        self.assertIn('aria-pressed', src)
        self.assertNotIn("setInterval(", src)


if __name__ == "__main__":
    unittest.main()
