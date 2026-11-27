#pragma once
#include "EffectBase.h"

namespace idg
{
std::unique_ptr<EffectModule> createEffect (int fxId);
}
