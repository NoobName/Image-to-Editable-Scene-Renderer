#pragma once
#include "ScenePackage/RelightingSession.h"
namespace isr {
// Returns 1 for offline robust fit, 2 for exporting the explicitly applied source calibration.
int DrawLightingPanel(RelightingSession&,bool busy=false);
bool DrawApplySourceButton(LightingSession&);
}
