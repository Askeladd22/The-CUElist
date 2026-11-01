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
            "mixerScroll", "strips",
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
        self.assertIn("CONCEPT V2.3 — NON OPERATIVO", src)
        self.assertIn("GO disabilitato", src)
        self.assertIn("PANIC non operativo", src)
        for forbidden in (
            "fetch(", "XMLHttpRequest", "WebSocket", "localStorage",
            "sessionStorage", "navigator.sendBeacon", "<iframe",
            "<link rel=", "<script src=", "AudioContext(", "eval(",
        ):
            self.assertNotIn(forbidden, src,
                             "Prototype must be self-contained and inert: " + forbidden)

    def test_mixer_is_one_continuous_column_grid(self):
        source = self.source
        js = self.parsed.script
        self.assertIn(
            ".mixer-grid{display:grid;grid-template-columns:"
            "var(--cue) repeat(11,var(--cell))", source
        )
        self.assertIn(
            ".matrix-row{display:grid;grid-template-columns:"
            "var(--cue) repeat(11,var(--cell))", source
        )
        self.assertIn(
            'id="mixerScroll"><div class="mixer-grid" id="strips"',
            source,
        )
        self.assertIn("scrollLeft=grid.scrollLeft", js,
                      "Mixer must follow the exact column scroll offset")
        self.assertIn('panel.appendChild(master)', js)
        self.assertIn('for(let i=0;i<labels.length;i++)', js)
        self.assertIn('channel.dataset.columnIndex=String(i)', js)
        self.assertIn('channel.appendChild(audioControls())', js)
        self.assertIn('type==="midi" || type==="osc"', js)
        self.assertIn('Priorità · Medium', js)
        self.assertIn('position:sticky;left:0;z-index:4', source,
                      "Master must stay beneath the pinned cue-label column")
        self.assertIn('class="mixer-metrics"', source,
                      "The fixed BPM/Auto Stop band continues beside mixer")
        self.assertNotIn('class="dock-content"', source)
        self.assertNotIn('class="strip"', source)
        self.assertNotIn('dock-top', source)
        self.assertIn(
            ".matrix::after{left:var(--cue);right:0;"
            "background:repeating-linear-gradient", source,
            "The empty workspace must visually continue the same channel lanes"
        )
        self.assertIn('top:calc(52px + 13 * var(--rowh))', source)

    def test_v2_3_type_specific_strips_without_feature_loss(self):
        js, source = self.parsed.script, self.source
        # Audio has real type-appropriate controls (demo only), not generic
        # MIDI/OSC faders or extra fake hardware operations.
        for token in (
            'function audioControls()', 'Direct out', 'POST', '0.0 dB',
            'Pan · C', 'Invert phase', 'Master L/R send', 'Mute Audio',
            '127.0.0.1:9000', 'MIDI OUT', 'Porta di uscita',
            'Destinazione UDP', 'TX · In attesa', 'TX · Idle', 'Mute TX',
            'CONTROL SET', 'Priorità · Medium', 'Edit set',
        ):
            self.assertIn(token, js)
        self.assertIn('else if(type==="midi" || type==="osc")', js)
        self.assertIn('const edit=btn("Edit set"', js)
        self.assertIn('mute.setAttribute("aria-pressed","false")', js)
        self.assertIn('type-status', source)
        self.assertIn('.data-channel', source)
        self.assertIn('.audio-action-row button:hover', source)
        self.assertIn('button:focus-visible', source)
        # Concrete native JUCE controls remain the authority on parity.
        native = (ROOT / "src" / "ColumnUtilities.cpp").read_text(
            encoding="utf-8"
        )
        for symbol in (
            'columnDataOutputMute', 'columnOscDestination',
            'controlColumnPriority', 'controlColumnEditSet',
            'masterSend', 'gainLearnHotspot', 'panLearnHotspot',
            'phaseLearnHotspot',
        ):
            self.assertIn(symbol, native,
                          'Existing native functionality was lost: ' + symbol)

    def test_sidebar_has_operator_context_not_temporary_guidance(self):
        source = self.source
        self.assertIn('class="sidebar-overview"', source)
        for label in ('SESSIONE', 'Cue list', 'Cue nella lista', 'Colonne'):
            self.assertIn(label, source)
        self.assertNotIn('Settings, Devices e Control Mapping nei menu superiori',
                         source)
        self.assertIn('id="go"', source)
        self.assertIn('id="panic"', source)

    def test_full_native_master_control_inventory_is_present_in_preview(self):
        src = self.parsed.script
        html = self.source
        # These map one-to-one to JUCE's actual MainComponent/ColumnUtilities.
        for control in (
            'masterGain', 'masterPan', 'masterMute', 'masterOutput',
            'masterLimiter', 'masterLearnToggle',
        ):
            self.assertIn(control, src, "Missing native feature: " + control)
        # JS creates the three learn IDs at runtime from a grounded list;
        # do not require the assembled names to appear as static strings.
        self.assertIn('for(const name of ["Gain","Pan","Limiter"])', src)
        self.assertIn('target.id="masterLearn"+name', src)
        self.assertIn('master-peak left', src)
        self.assertIn('master-peak right', src)
        self.assertIn('Gain, Pan and Limiter', html)
        self.assertIn('Master Bus only', src)
        self.assertIn('LIMIT · -1 dBFS', src)
        self.assertIn('GR 0.0 dB', src)
        native_main = (ROOT / "src" / "MainComponent.cpp").read_text(encoding="utf-8")
        native_strips = (ROOT / "src" / "ColumnUtilities.cpp").read_text(encoding="utf-8")
        for native_name in (
            "masterSlider", "masterPan", "masterMute", "masterOutput",
            "safetyLimiter", "masterGainLearnHotspot",
            "masterPanLearnHotspot", "masterLimiterLearnHotspot",
        ):
            self.assertIn(native_name, native_main,
                          "Native feature inventory changed: " + native_name)
        self.assertIn("setStereoPeaks", native_strips)
        self.assertIn("setGainReduction", native_strips)
        self.assertIn('gain.min="-60"', src)
        self.assertNotIn('master.appendChild(audioControls(true))', src,
                         "Master cannot be reduced to a generic audio strip")

    def test_no_duplicate_sidebar_routes_and_no_lost_navigation(self):
        html, js = self.source, self.parsed.script
        self.assertIn('id="menuPopover" role="menu"', html)
        for category in (
            "File", "Edit", "Show", "Cuelists", "Devices", "View", "Guide",
        ):
            self.assertIn('data-menu="' + category + '"', html)
        for command in (
            "Settings", "Preferences", "Project info", "Control Mapping",
            "Audio devices", "MIDI routing", "OSC routing", "New cuelist",
            "Rename cuelist", "Monitor messages", "User guide", "About",
        ):
            self.assertIn('"' + command + '"', js)
        self.assertNotIn('Show</div><button>Settings</button>', html)
        self.assertNotIn('>▤ &nbsp; Cuelists</button>', html)
        self.assertNotIn('>◧ &nbsp; Devices</button>', html)
        self.assertNotIn('>⊞ &nbsp; Control Mapping</button>', html)
        self.assertIn('aria-expanded', html)
        self.assertIn('menuPopover.hidden=false', js)
        self.assertNotIn('.menu{display:none}', html,
                         'Top menus cannot disappear at 1280px')
        self.assertIn('@media(max-width:1280px)', html)
        self.assertIn('.menu{display:flex;gap:0}', html)

    def test_top_workspace_and_reversible_panel_controls(self):
        src = self.parsed.script
        self.assertIn('classList.add("hide")', src)
        self.assertIn('classList.toggle("collapsed")', src)
        self.assertIn('--rowh",active?"31px":"37px"', src)
        self.assertIn('aria-pressed', src)
        self.assertIn("CONCEPT V2.3 — NON OPERATIVO", self.source)
        self.assertNotIn("setInterval(", src)


if __name__ == "__main__":
    unittest.main()
