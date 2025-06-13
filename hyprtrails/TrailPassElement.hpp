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

    virtual void        draw(const CRegion& damage);
    virtual bool        needsLiveBlur();
    virtual bool        needsPrecomputeBlur();

    virtual const char* passName() {
        return "CTrailPassElement";
    }

  private:
    STrailData data;
};