"""Offline structural contract for the native operator menu inventory.

This checks the *actual native MainComponent.cpp implementation*, not an
independent copy of the intended menus. Runtime behavior, devices and audio
engines are also covered by the native Windows CTest suite.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "MainComponent.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "MainComponent.h").read_text(encoding="utf-8")

def between(text: str, start: str, end: str) -> str:
    a = text.index(start)
    z = text.index(end, a + len(start))
    return text[a:z]

MENU = between(CPP, "void MainComponent::showOperatorMenu(",
               "void MainComponent::dispatchOperatorDeviceChoice(")
DISPATCH = between(CPP, "void MainComponent::dispatchOperatorMenu(",
                   "void MainComponent::resized()")
DEVICE = between(CPP, "void MainComponent::dispatchOperatorDeviceChoice(",
                 "void MainComponent::dispatchOperatorMenu(")

def menu_case(index: int) -> str:
    mark = re.search(r"\bcase\s+" + str(index) + r"\s*:", MENU)
    if not mark:
        raise AssertionError(f"Missing operator menu group {index}")
    rest = MENU[mark.end():]
    end = re.search(r"\bcase\s+\d+\s*:|\bdefault\s*:", rest)
    return rest[:end.start()] if end else rest

class NativeOperatorMenuParity(unittest.TestCase):
    def test_no_duplicate_preflight_or_diagnostics(self):
        self.assertEqual(MENU.count('"Pre-flight..."'), 1)
        self.assertEqual(MENU.count('"Diagnostics..."'), 1)
        self.assertIn('"Pre-flight..."', menu_case(2))
        self.assertIn('"Diagnostics..."', menu_case(2))
        self.assertNotIn('"Settings..."', MENU)
        self.assertNotIn('"MIDI / OSC Settings..."', MENU)
        self.assertNotIn('"Diagnostics / I/O activity..."', MENU)

    def test_all_legacy_preferences_settings_are_reachable(self):
        device = menu_case(4)
        view = menu_case(5)
        show = menu_case(2)
        for item in ('"Audio Devices..."', '"MIDI Inputs"', '"MIDI Outputs"',
                     '"OSC Input"', '"Control Mapping..."',
                     '"Receive UDP ports: "', '"Disable OSC input"'):
            self.assertIn(item, device, item)
        for item in ('"Fader Range"', 'faderRangeMenuTitle(6)',
                     'faderRangeMenuTitle(0)', '"Clock"',
                     '"24-hour clock"', '"12-hour clock"'):
            self.assertIn(item, view, item)
        for item in ('"Rename show..."', '"Pre-flight..."',
                     '"Diagnostics..."'):
            self.assertIn(item, show)
        self.assertIn('"Rename active Cuelist"', menu_case(3))
        self.assertIn('10000', device)
        self.assertIn('10001+i', device)
        self.assertIn('11001+i', device)

    def test_live_midi_osc_settings_dispatch_keeps_original_handlers(self):
        for text in ('selectControlInput({})', 'selectControlInput(device)',
                     'toggleAdditionalMidiInput(device)',
                     'engine.setMidiOutputEnabled(device.identifier,enabled)',
                     'appPreferences->setValue("midiOutputEnabled_"+device.identifier,',
                     'show.controls.midiInputId'):
            self.assertIn(text, DEVICE)
        for text in ('configureOscInputPorts(value)',
                     'configureOscInputPorts("")',
                     'openControlPanel(false)', 'showDevices()',
                     'setAudioFaderCeilingDb(command==10 ? 6 : 0)',
                     'appPreferences->setValue("clock12Hour",command==13)',
                     'showPreflight()', 'showDiagnostics()'):
            self.assertIn(text, DISPATCH)

    def test_compact_existing_one_click_actions_not_duplicated(self):
        self.assertNotIn('"MIDI Learn"', MENU)
        self.assertNotIn('"OSC Learn"', MENU)
        self.assertNotIn('collapseAllButton.getButtonText()', MENU)
        for item in ('globalMidiLearn.setBounds(', 'globalOscLearn.setBounds(',
                     'collapseAllButton.setButtonText('):
            self.assertIn(item, CPP)
        self.assertIn('operatorMenus[OperatorNavigationLayout::overflowIndex]',
                      CPP)
        self.assertIn('menu.addSubMenu(OperatorNavigationLayout::title(i),sub)',
                      CPP)

    def test_master_audio_stays_outside_menu_redesign(self):
        self.assertIn('masterGainLearnHotspot.onPick', CPP)
        self.assertIn('masterPanLearnHotspot.onPick', CPP)
        self.assertIn('masterLimiterLearnHotspot.onPick', CPP)
        self.assertIn('masterSlider.setName("masterAudioDb")', CPP)
        self.assertIn('dispatchOperatorDeviceChoice(int choice,', HEADER)

if __name__ == "__main__":
    unittest.main()
