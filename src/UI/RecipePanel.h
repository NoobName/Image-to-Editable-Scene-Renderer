#pragma once
#include "UI/AsyncFileDialog.h"
#include <array>
#include <string>
namespace isr {
enum class RecipeAction {None,Open,SaveNew,Replace,Export};
struct RecipeActions {
    RecipeAction request=RecipeAction::None;
    std::filesystem::path path;
    bool busy=false,cancel=false;
    std::string status="Save outside the source package. Export creates a new directory.";
};
class RecipePanel {
public:
    void Bind(HWND owner,RecipeActions* actions){owner_=owner;actions_=actions;}
    void Draw(bool available,bool reconstructionBusy);
private:
    HWND owner_{};RecipeActions* actions_{};AsyncFileDialog dialog_;
    RecipeAction pending_=RecipeAction::None;
    std::array<char,4096> exportPath_{};
};
}
