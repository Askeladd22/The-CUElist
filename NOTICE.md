# The Cuelist — License Notices

The original application code, scripts, tests, interface, artwork and documentation are **proprietary; all rights reserved**. The [LICENSE](LICENSE) is a conventional copyright notice, **not a public open-source licence or a bespoke commercial EULA**. Repository publication grants no additional source-code reuse rights beyond applicable law and the relevant GitHub Terms of Service. Any separately granted end-user licences continue under their own terms. Third-party licences apply independently.

## JUCE and Third-Party Components

- JUCE: version 8.0.12, commit `29396c22c93392d6738e021b83196283d6e4d850`.
- Source: https://github.com/juce-framework/JUCE/tree/8.0.12
- JUCE licensing is separate from the application's licensing. Distributing The Cuelist as proprietary software requires an appropriate JUCE license under the applicable terms. License availability has not been verified: this notice does not claim that any JUCE license has been purchased, activated, or accepted on behalf of the copyright holder.
- Check JUCE licensing before distributing to third parties: https://juce.com/get-juce/
- `vendor/JUCE/LICENSE.md` describes the framework's and dependencies' terms. Those notices remain applicable and are not superseded by the application's proprietary license.

## Embedded User-Interface Fonts (SIL Open Font License 1.1)

The Cuelist embeds the following Google Fonts under their **own** SIL Open Font License, Version 1.1 (OFL). The fonts are not relicensed under The Cuelist's proprietary copyright notice.

- **Exo 2** — copyright 2013 The Exo 2 Project Authors; small text and instrumentation (minimum 10 pt). Upstream: https://github.com/google/fonts/tree/main/ofl/exo2
- **Hind** — copyright 2014 Indian Type Foundry; larger labels, headings and titles. Upstream: https://github.com/google/fonts/tree/main/ofl/hind
- **Noto Sans Math** — copyright 2022 The Noto Project Authors; used explicitly for mathematical operators (e.g. minus and infinity U+221E in fader readings), not a generic interface-text fallback. Upstream: https://github.com/google/fonts/tree/main/ofl/notosansmath
- **Noto Sans Symbols 2** — copyright 2022 The Noto Project Authors; selected explicitly for Unicode row-reordering triangles and other designated symbols, not a generic interface-text fallback. Upstream: https://github.com/google/fonts/tree/main/ofl/notosanssymbols2
- The exact typeface files are fetched from Google Fonts commit `9862abc066c9b83bf84c3e8ed1fc2d8d48f9bbf0` when configuring CMake, then embedded in the executable. No fonts need to be installed on Windows and no runtime download is required.
- Full upstream notices and OFL terms are preserved at [assets/fonts/Exo2-OFL.txt](assets/fonts/Exo2-OFL.txt), [assets/fonts/Hind-OFL.txt](assets/fonts/Hind-OFL.txt), [assets/fonts/NotoSansMath-OFL.txt](assets/fonts/NotoSansMath-OFL.txt) and [assets/fonts/NotoSansSymbols2-OFL.txt](assets/fonts/NotoSansSymbols2-OFL.txt). Copies are included in portable packages under `fonts/`.

The OFL permits embedding the fonts in proprietary applications, provided the font copyright and license notices are kept with distribution.

## Portable Package

The local package includes one English-language `LICENSE` copyright notice, this `NOTICE.md`, and the separate `JUCE-LICENSE.md` required for third-party terms. It does not include the application's or framework's source code. Packaging does not certify that distribution is covered by the necessary licenses.

User-created or imported WAV/MP3 files and projects retain their respective rights. The fixture `tests/fixtures/tone.mp3` is a synthetic tone generated for project tests.
