#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class CTrail;

class CTrailPassElement : public IPassElement {
  public:
    struct STrailData {
        CTrail* deco = nullptr;
        float   a    = 1.F;
    };

    CTrailPassElement(const STrailData& data_);
    virtual ~CTrailPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                          needsLiveBlur() override;
    virtual bool                          needsPrecomputeBlur() override;

    virtual const char*                   passName() override {
        return "CTrailPassElement";
    }

    virtual ePassElementType type() override {
        return EK_CUSTOM;
    }

  private:
    STrailData data;
};
