#pragma once
#include <limits>
inline void modelV3Tests() {
    test("v3 column utilities and PauseRestart roundtrip", [] {
        auto show=makeShow(); auto& c=show.columns[0];
        c.gain=0.25f; c.pan=-0.75f; c.muted=true; c.phaseInverted=true;
        show.rows[0].behaviour=Behaviour::pauseRestart;
        show.rows[0].cells[0].behaviour=Behaviour::pauseRestart;
        const auto text=encodeShow(show); near((int)juce::JSON::parse(text)["version"],10);
        Show restored; check(decodeShow(text,restored).wasOk(),"v3 rejected");
        const auto& col=restored.columns[0]; near(col.gain,0.25); near(col.pan,-0.75);
        check(col.muted && col.phaseInverted,"Column flags lost");
        check(restored.rows[0].behaviour==Behaviour::pauseRestart && restored.rows[0].cells[0].behaviour==Behaviour::pauseRestart,"PauseRestart lost");
    });
    test("legacy v1 v2 restart and stop migrate to retrigger with neutral columns", [] {
        for (int version : {1,2}) for (int behaviour : {1,4}) {
            auto root=juce::JSON::parse(encodeShow(makeShow()));
            root.getDynamicObject()->setProperty("version",version);
            auto& col=root["columns"].getArray()->getReference(0);
            for (auto key : {"gain","pan","muted","phaseInverted"}) col.getDynamicObject()->removeProperty(key);
            auto& row=root["rows"].getArray()->getReference(0);
            row.getDynamicObject()->setProperty("behaviour",behaviour);
            row["cells"].getArray()->getReference(0).getDynamicObject()->setProperty("behaviour",behaviour);
            Show restored; check(decodeShow(juce::JSON::toString(root),restored).wasOk(),"Legacy rejected");
            check(restored.rows[0].behaviour==Behaviour::retrigger && restored.rows[0].cells[0].behaviour==Behaviour::retrigger,"Legacy activation migration failed");
            near(restored.columns[0].gain,1); near(restored.columns[0].pan,0);
            check(!restored.columns[0].muted && !restored.columns[0].phaseInverted,"Legacy columns not neutral");
        }
    });
    test("v3 invalid column settings and legacy stop are rejected atomically", [] {
        for (auto key : {"gain","pan","muted","phaseInverted"}) {
            auto root=juce::JSON::parse(encodeShow(makeShow()));
            root["columns"].getArray()->getReference(0).getDynamicObject()->setProperty(key,"invalid");
            Show destination; destination.name="keep";
            check(decodeShow(juce::JSON::toString(root),destination).failed(),"Wrong column type accepted");
            check(destination.name=="keep","Invalid decode replaced destination");
        }
        auto show=makeShow(); show.columns[0].pan=1.01f; check(validateShow(show).failed(),"Invalid pan accepted");
        show.columns[0].pan=0; show.columns[0].gain=std::numeric_limits<float>::infinity();
        check(validateShow(show).failed(),"Infinite gain accepted");
        auto root=juce::JSON::parse(encodeShow(makeShow()));
        root["rows"].getArray()->getReference(0)["cells"].getArray()->getReference(0).getDynamicObject()->setProperty("behaviour",4);
        Show destination; check(decodeShow(juce::JSON::toString(root),destination).failed(),"v3 stop activation accepted");
    });
}
