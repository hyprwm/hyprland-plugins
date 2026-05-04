#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class CBordersPlusPlus;

class CBorderPPPassElement : public IPassElement {
  public:
    struct SBorderPPData {
        CBordersPlusPlus* deco = nullptr;
        float             a    = 1.F;
    };

    CBorderPPPassElement(const SBorderPPData& data_);
    virtual ~CBorderPPPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                          needsLiveBlur() override;
    virtual bool                          needsPrecomputeBlur() override;

    virtual const char*                   passName() override {
        return "CBorderPPPassElement";
    }

    virtual ePassElementType type() override {
        return EK_CUSTOM;
    }

  private:
    SBorderPPData data;
};
