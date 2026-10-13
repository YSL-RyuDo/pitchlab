#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class VocalKeyboard : public juce::Component {
public:
    int mask = 0xfff, activeNote = -1;
    std::function<void(int)> onMaskChanged;
    void togglePitchClass(int pc) {
        const int changed = mask ^ (1 << ((pc % 12 + 12) % 12));
        if (changed == 0) return;
        mask = changed; if (onMaskChanged) onMaskChanged(mask); repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        for (bool black : {true, false}) for (int n = 0; n < 24; ++n)
            if (isBlack(n) == black && keyBounds(n).contains(e.position)) { togglePitchClass(n); return; }
    }
    void paint(juce::Graphics& g) override {
        for (bool black : {false, true}) for (int n = 0; n < 24; ++n) {
            if (isBlack(n) != black) continue;
            auto rect = keyBounds(n).reduced(1.0f, 0.5f);
            const bool allowed = (mask & (1 << (n % 12))) != 0;
            const bool active = activeNote >= 0 && activeNote % 12 == n % 12;
            g.setColour(active ? juce::Colour(0xff51d9c1) : black ? juce::Colour(0xff172027) : juce::Colour(allowed ? 0xffdce5e8 : 0xff6c7880));
            g.fillRoundedRectangle(rect, 3);
            g.setColour(juce::Colour(0xff10171d)); g.drawRoundedRectangle(rect, 3, 1);
            const float markY = rect.getBottom() - 9;
            g.setColour(allowed ? juce::Colour(0xff309b89) : juce::Colour(0xff46525a));
            g.fillEllipse(rect.getCentreX() - 2.5f, markY, 5, 5);
            if (!black) {
                const char* names[] = {"C", "", "D", "", "E", "F", "", "G", "", "A", "", "B"};
                g.setFont(juce::Font(juce::FontOptions("Malgun Gothic", 11, juce::Font::bold)));
                g.setColour(juce::Colour(0xff25323b));
                g.drawText(names[n % 12], rect.withY(rect.getBottom() - 26).withHeight(14), juce::Justification::centred);
            }
        }
    }
private:
    static bool isBlack(int n) { return n % 12 == 1 || n % 12 == 3 || n % 12 == 6 || n % 12 == 8 || n % 12 == 10; }
    juce::Rectangle<float> keyBounds(int n) const {
        int whites = 0; for (int i = 0; i < n; ++i) if (!isBlack(i)) ++whites;
        const float width = getWidth() / 14.0f;
        if (isBlack(n)) return {whites * width - width * 0.31f, 0, width * 0.62f, getHeight() * 0.63f};
        return {whites * width, 0, width, float(getHeight())};
    }
};
