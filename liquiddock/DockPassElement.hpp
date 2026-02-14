#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

class CLiquidDock;

class CDockPassElement : public IPassElement {
  public:
    struct SDockData {
        CLiquidDock* dock = nullptr;
        float        a    = 1.F;
    };

    CDockPassElement(const SDockData& data_);
    virtual ~CDockPassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();

    virtual const char*         passName() {
        return "CDockPassElement";
    }

  private:
    SDockData data;
};
