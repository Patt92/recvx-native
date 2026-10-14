#pragma once
#include "ninja_matrix.h"
#include <cstdint>
#include <optional>
#include <vector>
namespace recvx {
// hitchkl.c:735 bhCollisionCheckL2MDL reads the realized chunk model of one
// object directly: mdl->center/r, vlist (count at short 3, NJS_POINT3 from
// int 2) and plist (count at byte 4, index triples from short 3). This is
// the data view of that model, decoded from the MLB as bhMlbBinRealize
// (binfunc.c:4) and bhCnkBinRealize (binfunc.c) relocate it.
struct HitModelObject {
    NJS_POINT3 center{};
    float radius=0;
    std::vector<NJS_POINT3> vertices;
    std::vector<std::int16_t> indices; // three per polygon
};
// Index i is objP[i]; nullopt where objP[i].model is NULL. Throws on an MLB
// whose layout the original realize would not accept (basic models, truncated
// blocks, absent vlist/plist).
std::vector<std::optional<HitModelObject>> decode_hit_model(const std::vector<std::uint8_t>& mlb);
enum class HitModelResult {Miss,Hit,IndexOutside};
// bhCollisionCheckL2MDL(p1, p2, model, mtx): bounding test against the
// infinite line, then every polygon through bhCollisionCheckL2PL, each hit
// shortening `end`. IndexOutside: a polygon index beyond the vertex count,
// where the original reads past its njCalcPoints buffer.
HitModelResult line_hit_model(NJS_POINT3 start,NJS_POINT3& end,const HitModelObject& object,const NJS_MATRIX& matrix);
}
