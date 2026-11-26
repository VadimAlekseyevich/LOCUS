#pragma once
#include "core/chart_json.h"
#include "app/local_identity.h"
namespace locus::app {
// New identity only for a new authored difficulty, never Save/Rename.
inline std::string NewDifficultyId() { return NewLocalId(); }
} // namespace locus::app
