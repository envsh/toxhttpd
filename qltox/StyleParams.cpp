#include "StyleParams.h"
#include <cstring>
#include <vector>

StyleParams::Palette::Palette()
    : windowBg("#353535"), surfaceBg("#353535"), baseBg("#232323"),
      hoverBg("#404040"), activeBg("#484848"), textPrimary("#dcdcdc"),
      textMuted("#a0a0a0"), textDisabled("#7a7a7a"), accent("#2a82da"),
      accentText("#ffffff"), border("#505050"), borderFocus("#2a82da"),
      link("#2a82da"), scrollbarSlider("#606060"), scrollbarHover("#707070") {}

StyleParams::StyleParams()
    : buttonRadius(6), inputRadius(6), scrollbarWidth(8), spacing(8),
      touchTarget(32), scrollbarMode(AlwaysFaint), buttonStyle(Flat),
      compositingMode(ColorBlend) {}

static std::vector<StyleParams::Definition>& styleRegistry() {
    static std::vector<StyleParams::Definition> reg;
    return reg;
}

void StyleParams::registerStyle(const Definition& def) {
    styleRegistry().push_back(def);
}

const std::vector<StyleParams::Definition>& StyleParams::registeredStyles() {
    return styleRegistry();
}

StyleParams StyleParams::make(const char* id, bool dark) {
    for (const auto& def : styleRegistry()) {
        if (std::strcmp(def.id, id) == 0)
            return def.factory(dark);
    }
    return styleRegistry().front().factory(dark);
}
