#include "MainComponent.h"
namespace cuelist {
class Application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "The Cuelist"; }
    const juce::String getApplicationVersion() override { return "0.3.0"; }
    void initialise(const juce::String& arguments) override { window=std::make_unique<Window>(!arguments.contains("--offline")); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { if (window) window->content->requestQuit(); }
    class Window final : public juce::DocumentWindow {
    public:
        explicit Window(bool startAudio) : DocumentWindow("The Cuelist",palette::background,allButtons) {
            setUsingNativeTitleBar(true); content=new MainComponent(startAudio); setContentOwned(content,true);
            setResizable(true,true); setResizeLimits(1180,820,3840,2160); centreWithSize(1440,900); setVisible(true);
        }
        void closeButtonPressed() override { content->requestQuit(); }
        MainComponent* content=nullptr;
    };
private: std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(cuelist::Application)
