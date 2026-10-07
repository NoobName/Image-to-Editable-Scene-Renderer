#pragma once
#include "UI/AsyncFileDialog.h"
#include <array>
#include <string>
#include <utility>
namespace isr {
enum class RecipeAction {None,Open,SaveNew,Replace,Export,Refine};
struct RecipeActions {
    RecipeAction request=RecipeAction::None;
    std::filesystem::path path;
    bool busy=false,cancel=false;
    std::string status="Save outside the source package. Export creates a new directory.";
    bool refinementBusy=false,cancelRefinement=false;
    bool focusRefinement=false;
    float refinementStrength=0.15f;
    int refinementSeed=34;
    std::filesystem::path refinementReport;
    std::string refinementStatus="Optional offline candidate; never runs while dragging lights.";
};
bool DrawRefinementButton(RecipeActions&);
class RecipePanel {
public:
    void Bind(HWND owner,RecipeActions* actions){owner_=owner;actions_=actions;}
    void Draw(bool available,bool reconstructionBusy);
    bool TakeFocus(){if(actions_&&std::exchange(actions_->focusRefinement,false)){openRefinement_=true;return true;}return false;}
private:
    HWND owner_{};RecipeActions* actions_{};AsyncFileDialog dialog_;
    RecipeAction pending_=RecipeAction::None;
    std::array<char,4096> exportPath_{};
    bool openRefinement_=false;
};
}
