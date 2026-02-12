#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

class CHyprPill;

class CPillPassElement : public IPassElement {
  public:
    struct SPillData {
        CHyprPill* deco = nullptr;
        float      a    = 1.F;
    };

    CPillPassElement(const SPillData& data_);
    virtual ~CPillPassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();

    virtual const char*         passName() {
        return "CPillPassElement";
    }

  private:
    SPillData data;
};
