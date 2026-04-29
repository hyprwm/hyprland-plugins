#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class COverview;

class COverviewPassElement : public IPassElement {
  public:
    COverviewPassElement();
    virtual ~COverviewPassElement() = default;

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                          needsLiveBlur() override;
    virtual bool                          needsPrecomputeBlur() override;
    virtual std::optional<CBox>           boundingBox() override;
    virtual CRegion                       opaqueRegion() override;

    virtual const char*                   passName() override {
        return "COverviewPassElement";
    }

    virtual ePassElementType type() override {
        return EK_CUSTOM;
    }
};
