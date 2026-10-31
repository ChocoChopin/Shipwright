#include <vector>
#include <map>
#include <unordered_map>
#include <math.h>
#include <cstring>
#include <algorithm>
#include <cmath>

#include "frame_interpolation.h"
#include "first_person_view.h"
#include "NativeSimulationTest.h"
#include "soh/OTRGlobals.h"

/*
Frame interpolation.

The idea of this code is to interpolate all matrices.

The code contains two approaches. The first is to interpolate
all inputs in transformations, such as angles, scale and distances,
and then perform the same transformations with the interpolated values.
After evaluation for some reason some animations such rolling look strange.

The second approach is to simply interpolate the final matrices. This will
more or less simply interpolate the world coordinates for movements.
This will however make rotations ~180 degrees get the "paper effect".
The mitigation is to identify this case for actors and interpolate the
matrix but in model coordinates instead, by "removing" the rotation-
translation before interpolating, create a rotation matrix with the
interpolated angle which is then applied to the matrix.

Currently the code contains both methods but only the second one is currently
used.

Both approaches build a tree of instructions, containing matrices
at leaves. Every node is built from OPEN_DISPS/CLOSE_DISPS and manually
inserted FrameInterpolation_OpenChild/FrameInterpolation_Close child calls.
These nodes contain information that should suffice to identify the matrix,
so we can find it in an adjacent frame.

We can interpolate an arbitrary amount of frames between two original frames,
given a specific interpolation factor (0=old frame, 0.5=average of frames,
1.0=new frame).
*/

extern "C" {

void Matrix_Init(struct GameState* gameState);
void Matrix_Push(void);
void Matrix_Pop(void);
void Matrix_Get(MtxF* dest);
void Matrix_Put(MtxF* src);
void Matrix_Mult(MtxF* mf, u8 mode);
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode);
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode);
void Matrix_RotateX(f32 x, u8 mode);
void Matrix_RotateY(f32 y, u8 mode);
void Matrix_RotateZ(f32 z, u8 mode);
void Matrix_RotateZYX(s16 x, s16 y, s16 z, u8 mode);
void Matrix_TranslateRotateZYX(Vec3f* translation, Vec3s* rotation);
void Matrix_SetTranslateRotateYXZ(f32 translateX, f32 translateY, f32 translateZ, Vec3s* rot);
Mtx* Matrix_MtxFToMtx(MtxF* src, Mtx* dest);
Mtx* Matrix_ToMtx(Mtx* dest, char* file, s32 line);
Mtx* Matrix_NewMtx(struct GraphicsContext* gfxCtx, char* file, s32 line);
Mtx* Matrix_MtxFToNewMtx(MtxF* src, struct GraphicsContext* gfxCtx);
void Matrix_MultVec3f(Vec3f* src, Vec3f* dest);
void Matrix_MtxFCopy(MtxF* dest, MtxF* src);
void Matrix_MtxToMtxF(Mtx* src, MtxF* dest);
void Matrix_MultVec3fExt(Vec3f* src, Vec3f* dest, MtxF* mf);
void Matrix_Transpose(MtxF* mf);
void Matrix_ReplaceRotation(MtxF* mf);
void Matrix_MtxFToYXZRotS(MtxF* mf, Vec3s* rotDest, s32 flag);
void Matrix_MtxFToZYXRotS(MtxF* mf, Vec3s* rotDest, s32 flag);
void Matrix_RotateAxis(f32 angle, Vec3f* axis, u8 mode);
MtxF* Matrix_CheckFloats(MtxF* mf, char* file, s32 line);
void Matrix_SetTranslateScaleMtx2(Mtx* mtx, f32 scaleX, f32 scaleY, f32 scaleZ, f32 translateX, f32 translateY,
                                  f32 translateZ);

MtxF* Matrix_GetCurrent(void);

void SkinMatrix_MtxFMtxFMult(MtxF* mfA, MtxF* mfB, MtxF* dest);
}

static bool invert_matrix(const float m[16], float invOut[16]);

using namespace std;

namespace {

bool firstPersonScope = false;
struct FirstPersonMatrix { Mtx* key; MtxF viewSpace; };
std::vector<FirstPersonMatrix> firstPersonMatrices;
std::vector<MtxF> previousFirstPersonMatrices;
bool firstPersonAttachmentScope = false;
std::vector<FirstPersonMatrix> firstPersonAttachments;
std::vector<MtxF> previousFirstPersonAttachments;
const void* firstPersonAttachmentOwner = nullptr;
const void* previousFirstPersonAttachmentOwner = nullptr;
const void* firstPersonOwner = nullptr;
const void* previousFirstPersonOwner = nullptr;
int firstPersonProfile = 0, previousFirstPersonProfile = 0;
int firstPersonEpoch = 0, previousFirstPersonEpoch = 0;
FirstPersonViewStats firstPersonStats{};
Mtx* firstPersonViewKey = nullptr;
MtxF firstPersonView;

enum class Op {
    OpenChild,
    CloseChild,

    MatrixPush,
    MatrixPop,
    MatrixPut,
    MatrixMult,
    MatrixTranslate,
    MatrixScale,
    MatrixRotate1Coord,
    MatrixRotateZYX,
    MatrixTranslateRotateZYX,
    MatrixSetTranslateRotateYXZ,
    MatrixMtxFToMtx,
    MatrixToMtx,
    MatrixReplaceRotation,
    MatrixRotateAxis,
    SkinMatrixMtxFToMtx
};

typedef pair<const void*, int> label;

union Data {
    Data() {
    }

    struct {
        MtxF src;
    } matrix_put;

    struct {
        MtxF mf;
        u8 mode;
    } matrix_mult;

    struct {
        f32 x, y, z;
        u8 mode;
    } matrix_translate, matrix_scale;

    struct {
        u32 coord;
        f32 value;
        u8 mode;
    } matrix_rotate_1_coord;

    struct {
        s16 x, y, z;
        u8 mode;
    } matrix_rotate_zyx;

    struct {
        Vec3f translation;
        Vec3s rotation;
    } matrix_translate_rotate_zyx;

    struct {
        f32 translateX, translateY, translateZ;
        Vec3s rot;
        // MtxF mtx;
        bool has_mtx;
    } matrix_set_translate_rotate_yxz;

    struct {
        MtxF src;
        Mtx* dest;
    } matrix_mtxf_to_mtx;

    struct {
        Mtx* dest;
        MtxF src;
        bool has_adjusted;
    } matrix_to_mtx;

    struct {
        MtxF mf;
    } matrix_replace_rotation;

    struct {
        f32 angle;
        Vec3f axis;
        u8 mode;
    } matrix_rotate_axis;

    struct {
        label key;
        size_t idx;
    } open_child;
};

struct SkinVertices {
    const void* owner;
    const void* resource;
    int limb;
    vector<Vtx> vertices;
    Vtx* output; // Current graph only; never dereference previous-recording outputs.
};

struct Path {
    map<label, vector<Path>> children;
    map<Op, vector<Data>> ops;
    vector<pair<Op, size_t>> items;
    vector<SkinVertices> skin;
};

struct Recording {
    Path root_path;
};

bool is_recording;
vector<Path*> current_path;
uint32_t camera_epoch;
uint32_t previous_camera_epoch;
Recording current_recording;
Recording previous_recording;

struct SkinBinding {
    const SkinVertices* previous;
    SkinVertices* current;
};
vector<SkinBinding> skinBindings;
SkinInterpolationStats skinStats{};
bool recordedSkin;

// Match the same draw occurrence in the existing actor/display-list recording tree.
// Missing/cull-reappearing limbs start at their current pose instead of stale history.
void BindSkin(Path& previous, Path& current) {
    for (size_t i = 0; i < current.skin.size(); ++i) {
        auto& n = current.skin[i];
        const auto* o = i < previous.skin.size() ? &previous.skin[i] : &n;
        if (!o->owner || o->owner != n.owner || o->resource != n.resource || o->limb != n.limb ||
            o->vertices.size() != n.vertices.size()) o = &n;
        skinBindings.push_back({ o, &n });
    }
    for (auto& child : current.children) {
        auto old = previous.children.find(child.first);
        for (size_t i = 0; i < child.second.size(); ++i) {
            auto& n = child.second[i];
            BindSkin(old != previous.children.end() && i < old->second.size() ? old->second[i] : n, n);
        }
    }
}

void ForgetSkin(Path& path, const void* owner) {
    for (auto& skin : path.skin) if (skin.owner == owner) skin.owner = nullptr;
    for (auto& child : path.children) for (auto& path : child.second) ForgetSkin(path, owner);
}

u64 InterpolateSkinBinding(const SkinBinding& binding, float step) {
    const auto& n = *binding.current;
    const auto& o = *binding.previous;
    if (!n.owner || !n.output) return 0;
    // Copy preserves exact current UVs, alpha and flags; only geometry/normals blend.
    std::memcpy(n.output, n.vertices.data(), n.vertices.size() * sizeof(Vtx));
    if (step >= 1.0f || &n == &o) return 0;
    step = std::max(0.0f, step);
    u64 changed = 0;
    for (size_t i = 0; i < n.vertices.size(); ++i) {
        for (unsigned axis = 0; axis < 3; ++axis) {
            n.output[i].n.ob[axis] = static_cast<s16>(std::lround(
                o.vertices[i].n.ob[axis] + step * (n.vertices[i].n.ob[axis] - o.vertices[i].n.ob[axis])));
            n.output[i].n.n[axis] = static_cast<s8>(std::lround(
                o.vertices[i].n.n[axis] + step * (n.vertices[i].n.n[axis] - o.vertices[i].n.n[axis])));
        }
        changed += std::memcmp(n.output[i].n.ob, n.vertices[i].n.ob, sizeof(n.output[i].n.ob)) != 0 &&
                   std::memcmp(n.output[i].n.ob, o.vertices[i].n.ob, sizeof(n.output[i].n.ob)) != 0;
    }
    return changed;
}

bool next_is_actor_pos_rot_matrix;
bool has_inv_actor_mtx;
MtxF inv_actor_mtx;
size_t inv_actor_mtx_path_index;

Data& append(Op op) {
    auto& m = current_path.back()->ops[op];
    current_path.back()->items.emplace_back(op, m.size());
    return m.emplace_back();
}

struct InterpolateCtx {
    float step;
    float w;
    unordered_map<Mtx*, MtxF> mtx_replacements;
    MtxF tmp_mtxf, tmp_mtxf2;
    Vec3f tmp_vec3f;
    Vec3s tmp_vec3s;
    MtxF actor_mtx;

    MtxF* new_replacement(Mtx* addr) {
        return &mtx_replacements[addr];
    }

    void interpolate_mtxf(MtxF* res, MtxF* o, MtxF* n) {
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                res->mf[i][j] = w * o->mf[i][j] + step * n->mf[i][j];
            }
        }
    }

    float lerp(f32 o, f32 n) {
        return w * o + step * n;
    }

    void lerp_vec3f(Vec3f* res, Vec3f* o, Vec3f* n) {
        res->x = lerp(o->x, n->x);
        res->y = lerp(o->y, n->y);
        res->z = lerp(o->z, n->z);
    }

    float interpolate_angle(f32 o, f32 n) {
        if (o == n)
            return n;
        o = fmodf(o, static_cast<f32>(2.0f * M_PI));
        if (o < 0.0f) {
            o += static_cast<f32>(2.0f * M_PI);
        }
        n = fmodf(n, static_cast<f32>(2.0f * M_PI));
        if (n < 0.0f) {
            n += static_cast<f32>(2.0f * M_PI);
        }
        if (fabsf(o - n) > M_PI) {
            if (o < n) {
                o += static_cast<f32>(2.0f * M_PI);
            } else {
                n += static_cast<f32>(2.0f * M_PI);
            }
        }
        if (fabsf(o - n) > M_PI / 2) {
            // return n;
        }
        return lerp(o, n);
    }

    s16 interpolate_angle(s16 os, s16 ns) {
        if (os == ns)
            return ns;
        int o = (u16)os;
        int n = (u16)ns;
        u16 res;
        int diff = o - n;
        if (-0x8000 <= diff && diff <= 0x8000) {
            if (diff < -0x4000 || diff > 0x4000) {
                return ns;
            }
            res = (u16)(w * o + step * n);
        } else {
            if (o < n) {
                o += 0x10000;
            } else {
                n += 0x10000;
            }
            diff = o - n;
            if (diff < -0x4000 || diff > 0x4000) {
                return ns;
            }
            res = (u16)(w * o + step * n);
        }
        if (os / 327 == ns / 327 && (s16)res / 327 != os / 327) {
            int bp = 0;
        }
        return res;
    }

    void interpolate_angles(Vec3s* res, Vec3s* o, Vec3s* n) {
        res->x = interpolate_angle(o->x, n->x);
        res->y = interpolate_angle(o->y, n->y);
        res->z = interpolate_angle(o->z, n->z);
    }

    void interpolate_branch(Path* old_path, Path* new_path) {
        for (auto& item : new_path->items) {
            Data& new_op = new_path->ops[item.first][item.second];

            if (item.first == Op::OpenChild) {
                if (auto it = old_path->children.find(new_op.open_child.key);
                    it != old_path->children.end() && new_op.open_child.idx < it->second.size()) {
                    interpolate_branch(&it->second[new_op.open_child.idx],
                                       &new_path->children.find(new_op.open_child.key)->second[new_op.open_child.idx]);
                } else {
                    interpolate_branch(&new_path->children.find(new_op.open_child.key)->second[new_op.open_child.idx],
                                       &new_path->children.find(new_op.open_child.key)->second[new_op.open_child.idx]);
                }
                continue;
            }

            if (auto it = old_path->ops.find(item.first); it != old_path->ops.end()) {
                if (item.second < it->second.size()) {
                    Data& old_op = it->second[item.second];
                    switch (item.first) {
                        case Op::OpenChild:
                            break;
                        case Op::CloseChild:
                            break;

                        case Op::MatrixPush:
                            Matrix_Push();
                            break;

                        case Op::MatrixPop:
                            Matrix_Pop();
                            break;

                        case Op::MatrixPut:
                            interpolate_mtxf(&tmp_mtxf, &old_op.matrix_put.src, &new_op.matrix_put.src);
                            Matrix_Put(&tmp_mtxf);
                            break;

                        case Op::MatrixMult:
                            interpolate_mtxf(&tmp_mtxf, &old_op.matrix_mult.mf, &new_op.matrix_mult.mf);
                            Matrix_Mult(&tmp_mtxf, new_op.matrix_mult.mode);
                            break;

                        case Op::MatrixTranslate:
                            Matrix_Translate(lerp(old_op.matrix_translate.x, new_op.matrix_translate.x),
                                             lerp(old_op.matrix_translate.y, new_op.matrix_translate.y),
                                             lerp(old_op.matrix_translate.z, new_op.matrix_translate.z),
                                             new_op.matrix_translate.mode);
                            break;

                        case Op::MatrixScale:
                            Matrix_Scale(lerp(old_op.matrix_scale.x, new_op.matrix_scale.x),
                                         lerp(old_op.matrix_scale.y, new_op.matrix_scale.y),
                                         lerp(old_op.matrix_scale.z, new_op.matrix_scale.z), new_op.matrix_scale.mode);
                            break;

                        case Op::MatrixRotate1Coord: {
                            float v = interpolate_angle(old_op.matrix_rotate_1_coord.value,
                                                        new_op.matrix_rotate_1_coord.value);
                            u8 mode = new_op.matrix_rotate_1_coord.mode;
                            switch (new_op.matrix_rotate_1_coord.coord) {
                                case 0:
                                    Matrix_RotateX(v, mode);
                                    break;

                                case 1:
                                    Matrix_RotateY(v, mode);
                                    break;

                                case 2:
                                    Matrix_RotateZ(v, mode);
                                    break;
                            }
                            break;
                        }

                        case Op::MatrixRotateZYX:
                            Matrix_RotateZYX(interpolate_angle(old_op.matrix_rotate_zyx.x, new_op.matrix_rotate_zyx.x),
                                             interpolate_angle(old_op.matrix_rotate_zyx.y, new_op.matrix_rotate_zyx.y),
                                             interpolate_angle(old_op.matrix_rotate_zyx.z, new_op.matrix_rotate_zyx.z),
                                             new_op.matrix_rotate_zyx.mode);
                            break;

                        case Op::MatrixTranslateRotateZYX:
                            lerp_vec3f(&tmp_vec3f, &old_op.matrix_translate_rotate_zyx.translation,
                                       &new_op.matrix_translate_rotate_zyx.translation);
                            interpolate_angles(&tmp_vec3s, &old_op.matrix_translate_rotate_zyx.rotation,
                                               &new_op.matrix_translate_rotate_zyx.rotation);
                            Matrix_TranslateRotateZYX(&tmp_vec3f, &tmp_vec3s);
                            break;

                        case Op::MatrixSetTranslateRotateYXZ:
                            interpolate_angles(&tmp_vec3s, &old_op.matrix_set_translate_rotate_yxz.rot,
                                               &new_op.matrix_set_translate_rotate_yxz.rot);
                            Matrix_SetTranslateRotateYXZ(lerp(old_op.matrix_set_translate_rotate_yxz.translateX,
                                                              new_op.matrix_set_translate_rotate_yxz.translateX),
                                                         lerp(old_op.matrix_set_translate_rotate_yxz.translateY,
                                                              new_op.matrix_set_translate_rotate_yxz.translateY),
                                                         lerp(old_op.matrix_set_translate_rotate_yxz.translateZ,
                                                              new_op.matrix_set_translate_rotate_yxz.translateZ),
                                                         &tmp_vec3s);
                            if (new_op.matrix_set_translate_rotate_yxz.has_mtx &&
                                old_op.matrix_set_translate_rotate_yxz.has_mtx) {
                                actor_mtx = *Matrix_GetCurrent();
                            }
                            break;

                        case Op::MatrixMtxFToMtx:
                            interpolate_mtxf(new_replacement(new_op.matrix_mtxf_to_mtx.dest),
                                             &old_op.matrix_mtxf_to_mtx.src, &new_op.matrix_mtxf_to_mtx.src);
                            break;

                        case Op::MatrixToMtx: {
                            //*new_replacement(new_op.matrix_to_mtx.dest) = *Matrix_GetCurrent();
                            if (old_op.matrix_to_mtx.has_adjusted && new_op.matrix_to_mtx.has_adjusted) {
                                interpolate_mtxf(&tmp_mtxf, &old_op.matrix_to_mtx.src, &new_op.matrix_to_mtx.src);
                                SkinMatrix_MtxFMtxFMult(&actor_mtx, &tmp_mtxf,
                                                        new_replacement(new_op.matrix_to_mtx.dest));
                            } else {
                                interpolate_mtxf(new_replacement(new_op.matrix_to_mtx.dest), &old_op.matrix_to_mtx.src,
                                                 &new_op.matrix_to_mtx.src);
                            }
                            break;
                        }

                        case Op::MatrixReplaceRotation:
                            interpolate_mtxf(&tmp_mtxf, &old_op.matrix_replace_rotation.mf,
                                             &new_op.matrix_replace_rotation.mf);
                            Matrix_ReplaceRotation(&tmp_mtxf);
                            break;

                        case Op::MatrixRotateAxis:
                            lerp_vec3f(&tmp_vec3f, &old_op.matrix_rotate_axis.axis, &new_op.matrix_rotate_axis.axis);
                            Matrix_RotateAxis(
                                interpolate_angle(old_op.matrix_rotate_axis.angle, new_op.matrix_rotate_axis.angle),
                                &tmp_vec3f, new_op.matrix_rotate_axis.mode);
                            break;

                        case Op::SkinMatrixMtxFToMtx:
                            break;
                    }
                }
            }
        }
    }
};

} // anonymous namespace

unordered_map<Mtx*, MtxF> FrameInterpolation_Interpolate(float step) {
    InterpolateCtx ctx;
    ctx.step = step;
    ctx.w = 1.0f - step;
    ctx.interpolate_branch(&previous_recording.root_path, &current_recording.root_path);
    return ctx.mtx_replacements;
}

void FrameInterpolation_StartRecord(void) {
    firstPersonScope = false;
    firstPersonAttachmentScope = false;
    previousFirstPersonAttachments.clear();
    for (const auto& model : firstPersonAttachments) previousFirstPersonAttachments.push_back(model.viewSpace);
    previousFirstPersonAttachmentOwner = firstPersonAttachmentOwner;
    firstPersonAttachmentOwner = nullptr;
    firstPersonAttachments.clear();
    previousFirstPersonMatrices.clear();
    for (const auto& model : firstPersonMatrices) previousFirstPersonMatrices.push_back(model.viewSpace);
    previousFirstPersonOwner = firstPersonOwner;
    previousFirstPersonProfile = firstPersonProfile;
    previousFirstPersonEpoch = firstPersonEpoch;
    firstPersonOwner = nullptr;
    firstPersonMatrices.clear();
    firstPersonViewKey = nullptr;
    skinBindings.clear();
    recordedSkin = false;
    previous_recording = std::move(current_recording);
    current_recording = {};
    current_path.clear();
    current_path.push_back(&current_recording.root_path);
    if (OTRGlobals::Instance->GetInterpolationFPS() != 20) {
        is_recording = true;
    }
}

void FrameInterpolation_StopRecord(void) {
    if (recordedSkin) BindSkin(previous_recording.root_path, current_recording.root_path);
    previous_camera_epoch = camera_epoch;
    is_recording = false;
}

extern "C" int FrameInterpolation_SetRecording(int enabled) {
    const bool previous = is_recording;
    is_recording = enabled != 0;
    return previous;
}

int FrameInterpolation_IsRecording(void) {
    return is_recording;
}

void FrameInterpolation_RecordSkinVertices(const void* owner, const void* resource, int limb,
                                           const Vtx* source, Vtx* output, unsigned count) {
    if (!is_recording || !output || !count) return;
    std::memcpy(output, source, count * sizeof(Vtx));
    current_path.back()->skin.push_back({ owner, resource, limb, vector<Vtx>(source, source + count), output });
    recordedSkin = true;
    ++skinStats.recordedLimbs;
}

void FrameInterpolation_ForgetSkin(const void* owner) {
    ForgetSkin(previous_recording.root_path, owner);
    ForgetSkin(current_recording.root_path, owner);
}

void FrameInterpolation_InterpolateSkin(float step) {
    for (const auto& binding : skinBindings) {
        const auto changed = InterpolateSkinBinding(binding, step);
        skinStats.changedVertices += changed;
        skinStats.intermediateLimbs += changed != 0;
    }
}

SkinInterpolationStats FrameInterpolation_GetSkinStats(void) {
    return skinStats;
}

void FrameInterpolation_EndSkinFrame(void) {
    FrameInterpolation_InterpolateSkin(1.0f);
    for (auto& binding : skinBindings) binding.current->output = nullptr;
    skinBindings.clear(); // No frame-arena output pointer survives graph replay.
}

unsigned FrameInterpolation_TestSkin(unsigned* checks) {
    unsigned failures = 0;
    *checks = 0;
    auto check = [&](bool pass) { ++*checks; failures += !pass; };
    int owner, resource;
    Vtx output{}, before{}, after{};
    before.n.ob[0] = -120; before.n.ob[1] = 300; before.n.n[0] = -100;
    after.n.ob[0] = 120; after.n.ob[1] = -300; after.n.n[0] = 100;
    after.n.tc[0] = 1234; after.n.a = 77; after.n.flag = 9;
    SkinVertices old{ &owner, &resource, 0, {before}, nullptr };
    SkinVertices now{ &owner, &resource, 0, {after}, &output };
    SkinBinding binding{ &old, &now };
    check(InterpolateSkinBinding(binding, 0.5f) == 1);
    check(output.n.ob[0] == 0 && output.n.ob[1] == 0 && output.n.n[0] == 0);
    check(output.n.tc[0] == 1234 && output.n.a == 77 && output.n.flag == 9);
    const Vtx middle = output;
    InterpolateSkinBinding(binding, 0.5f);
    check(std::memcmp(&middle, &output, sizeof(Vtx)) == 0);
    check(std::memcmp(&old.vertices[0], &before, sizeof(Vtx)) == 0);
    check(std::memcmp(&now.vertices[0], &after, sizeof(Vtx)) == 0);
    InterpolateSkinBinding(binding, 1.0f);
    check(std::memcmp(&output, &after, sizeof(Vtx)) == 0);
    InterpolateSkinBinding({ &now, &now }, 0.5f);
    check(std::memcmp(&output, &after, sizeof(Vtx)) == 0);
    InterpolateSkinBinding(binding, 0.0f);
    check(output.n.ob[0] == -120 && output.n.ob[1] == 300 && output.n.n[0] == -100);
    // Reused actor/skin addresses, changed resources, and newly visible draws do not blend.
    auto savedBindings = std::move(skinBindings);
    Path a, b;
    a.skin.push_back(old); b.skin.push_back(now);
    BindSkin(a, b);
    check(skinBindings.size() == 1 && skinBindings[0].previous == &a.skin[0]);
    skinBindings.clear();
    ForgetSkin(a, &owner);
    BindSkin(a, b);
    check(skinBindings[0].previous == &b.skin[0]);
    skinBindings.clear();
    a.skin[0] = old; a.skin[0].resource = nullptr;
    BindSkin(a, b);
    check(skinBindings[0].previous == &b.skin[0]);
    skinBindings.clear();
    a.skin.clear(); BindSkin(a, b);
    check(skinBindings[0].previous == &b.skin[0]);
    FrameInterpolation_EndSkinFrame();
    check(skinBindings.empty() && std::memcmp(&output, &after, sizeof(Vtx)) == 0);
    skinBindings = std::move(savedBindings);
    return failures;
}

void FrameInterpolation_RecordOpenChild(const void* a, int b) {
    if (!is_recording)
        return;
    label key = { a, b };
    auto& m = current_path.back()->children[key];
    append(Op::OpenChild).open_child = { key, m.size() };
    current_path.push_back(&m.emplace_back());
}

void FrameInterpolation_RecordCloseChild(void) {
    if (!is_recording)
        return;
    // append(Op::CloseChild);
    if (has_inv_actor_mtx && current_path.size() == inv_actor_mtx_path_index) {
        has_inv_actor_mtx = false;
    }
    current_path.pop_back();
}

void FrameInterpolation_DontInterpolateCamera(void) {
    camera_epoch = previous_camera_epoch + 1;
}

int FrameInterpolation_GetCameraEpoch(void) {
    return (int)camera_epoch;
}

void FrameInterpolation_RecordActorPosRotMatrix(void) {
    if (!is_recording)
        return;
    next_is_actor_pos_rot_matrix = true;
}

void FrameInterpolation_RecordMatrixPush(void) {
    if (!is_recording)
        return;
    append(Op::MatrixPush);
}

void FrameInterpolation_RecordMatrixPop(void) {
    if (!is_recording)
        return;
    append(Op::MatrixPop);
}

void FrameInterpolation_RecordMatrixPut(MtxF* src) {
    if (!is_recording)
        return;
    append(Op::MatrixPut).matrix_put = { *src };
}

void FrameInterpolation_RecordMatrixMult(MtxF* mf, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixMult).matrix_mult = { *mf, mode };
}

void FrameInterpolation_RecordMatrixTranslate(f32 x, f32 y, f32 z, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixTranslate).matrix_translate = { x, y, z, mode };
}

void FrameInterpolation_RecordMatrixScale(f32 x, f32 y, f32 z, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixScale).matrix_scale = { x, y, z, mode };
}

void FrameInterpolation_RecordMatrixRotate1Coord(u32 coord, f32 value, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixRotate1Coord).matrix_rotate_1_coord = { coord, value, mode };
}

void FrameInterpolation_RecordMatrixRotateZYX(s16 x, s16 y, s16 z, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixRotateZYX).matrix_rotate_zyx = { x, y, z, mode };
}

void FrameInterpolation_RecordMatrixTranslateRotateZYX(Vec3f* translation, Vec3s* rotation) {
    if (!is_recording)
        return;
    append(Op::MatrixTranslateRotateZYX).matrix_translate_rotate_zyx = { *translation, *rotation };
}

void FrameInterpolation_RecordMatrixSetTranslateRotateYXZ(f32 translateX, f32 translateY, f32 translateZ, Vec3s* rot) {
    if (!is_recording)
        return;
    auto& d = append(Op::MatrixSetTranslateRotateYXZ).matrix_set_translate_rotate_yxz = { translateX, translateY,
                                                                                          translateZ, *rot };
    if (next_is_actor_pos_rot_matrix) {
        d.has_mtx = true;
        // d.mtx = *Matrix_GetCurrent();
        invert_matrix((const float*)Matrix_GetCurrent()->mf, (float*)inv_actor_mtx.mf);
        next_is_actor_pos_rot_matrix = false;
        has_inv_actor_mtx = true;
        inv_actor_mtx_path_index = current_path.size();
    }
}

void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* src, Mtx* dest) {
    if (!is_recording)
        return;
    if ((firstPersonScope || firstPersonAttachmentScope) && firstPersonViewKey) {
        MtxF local;
        SkinMatrix_MtxFMtxFMult(&firstPersonView, src, &local);
        (firstPersonAttachmentScope ? firstPersonAttachments : firstPersonMatrices).push_back({dest, local});
    }
    append(Op::MatrixMtxFToMtx).matrix_mtxf_to_mtx = { *src, dest };
}

void FrameInterpolation_RecordMatrixToMtx(Mtx* dest, char* file, s32 line) {
    if (!is_recording)
        return;
    if ((firstPersonScope || firstPersonAttachmentScope) && firstPersonViewKey) {
        MtxF local;
        SkinMatrix_MtxFMtxFMult(&firstPersonView, Matrix_GetCurrent(), &local);
        (firstPersonAttachmentScope ? firstPersonAttachments : firstPersonMatrices).push_back({dest, local});
    }
    auto& d = append(Op::MatrixToMtx).matrix_to_mtx = { dest };
    if (has_inv_actor_mtx) {
        d.has_adjusted = true;
        SkinMatrix_MtxFMtxFMult(&inv_actor_mtx, Matrix_GetCurrent(), &d.src);
    } else {
        d.src = *Matrix_GetCurrent();
    }
}

void FrameInterpolation_RecordMatrixReplaceRotation(MtxF* mf) {
    if (!is_recording)
        return;
    append(Op::MatrixReplaceRotation).matrix_replace_rotation = { *mf };
}

void FrameInterpolation_RecordMatrixRotateAxis(f32 angle, Vec3f* axis, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixRotateAxis).matrix_rotate_axis = { angle, *axis, mode };
}

void FrameInterpolation_RecordSkinMatrixMtxFToMtx(MtxF* src, Mtx* dest) {
    if (!is_recording)
        return;
    FrameInterpolation_RecordMatrixMtxFToMtx(src, dest);
}

// https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
void FrameInterpolation_FirstPersonScope(int enabled, const void* owner, int profile) {
    firstPersonScope = enabled != 0;
    if (enabled) {
        firstPersonOwner = owner;
        firstPersonProfile = profile;
        firstPersonEpoch = camera_epoch;
    }
}

void FrameInterpolation_FirstPersonAttachmentScope(int enabled, const void* parent, const void* attachment) {
    firstPersonAttachmentScope = enabled && parent && parent == firstPersonOwner && !firstPersonMatrices.empty();
    if (firstPersonAttachmentScope) firstPersonAttachmentOwner = attachment;
}

void FrameInterpolation_RecordFirstPersonView(Mtx* key, const MtxF* view) {
    firstPersonViewKey = key;
    firstPersonView = *view;
}

FirstPersonViewStats FrameInterpolation_GetFirstPersonStats() { return firstPersonStats; }

void FrameInterpolation_RebaseFirstPerson(std::unordered_map<Mtx*, MtxF>& replacements,
                                         Mtx* viewingKey, const MtxF& currentView, float alpha) {
    const bool observe = NativeSimTest_IsMeasuring() && NativeSimTest_ConfigInt("observe_aim_presentation", 0);
    if (observe) firstPersonStats.lastAttachmentMatrices = 0;
    if (!viewingKey || viewingKey != firstPersonViewKey || firstPersonMatrices.empty()) return;
    MtxF inverseView;
    if (!invert_matrix(&currentView.mf[0][0], &inverseView.mf[0][0])) return;
    const bool blend = firstPersonOwner && firstPersonOwner == previousFirstPersonOwner &&
        firstPersonProfile == previousFirstPersonProfile && firstPersonEpoch == previousFirstPersonEpoch &&
        firstPersonMatrices.size() == previousFirstPersonMatrices.size();
    if (observe) { ++firstPersonStats.frames; firstPersonStats.blendedFrames += blend; }
    auto present = [&](const auto& models, const auto& previous, bool interpolate) {
        for (size_t index = 0; index < models.size(); ++index) {
            const auto& model = models[index];
            MtxF local = model.viewSpace;
            if (interpolate) for (unsigned row = 0; row < 4; ++row) for (unsigned col = 0; col < 4; ++col)
                local.mf[row][col] = (1.0f - alpha) * previous[index].mf[row][col] +
                                    alpha * model.viewSpace.mf[row][col];
            MtxF adjusted;
            SkinMatrix_MtxFMtxFMult(&inverseView, &local, &adjusted);
            replacements[model.key] = adjusted;
            if (observe) {
                MtxF displayed;
                auto view = currentView;
                SkinMatrix_MtxFMtxFMult(&view, &adjusted, &displayed);
                // Origin and three 1000-model-unit probes cover both translation
                // and orientation. Only aggregate counters survive successful runs.
                for (unsigned axis = 0; axis < 3; ++axis) {
                    float error = std::fabs(displayed.mf[3][axis] - local.mf[3][axis]);
                    for (unsigned probe = 0; probe < 3; ++probe)
                        error = std::max(error, std::fabs((displayed.mf[3][axis] - local.mf[3][axis]) +
                            1000.0f * (displayed.mf[probe][axis] - local.mf[probe][axis])));
                    firstPersonStats.maxViewError = std::max(firstPersonStats.maxViewError, error);
                }
                ++firstPersonStats.matrices;
            }
        }
    };
    present(firstPersonMatrices, previousFirstPersonMatrices, blend);
    const bool attachmentBlend = blend && firstPersonAttachmentOwner &&
        firstPersonAttachmentOwner == previousFirstPersonAttachmentOwner &&
        firstPersonAttachments.size() == previousFirstPersonAttachments.size();
    present(firstPersonAttachments, previousFirstPersonAttachments, attachmentBlend);
    if (observe) {
        firstPersonStats.attachmentMatrices += static_cast<unsigned>(firstPersonAttachments.size());
        firstPersonStats.lastAttachmentMatrices = static_cast<unsigned>(firstPersonAttachments.size());
    }
}

unsigned FrameInterpolation_TestFirstPersonView(void) {
    // A moving camera and breathing hand must preserve the interpolated LOCAL
    // pose on every rendered subframe, including the empty replacement endpoint.
    Mtx viewKey{}, modelKey{}, attachmentKey{};
    MtxF oldLocal{}, newLocal{}, view{};
    for (unsigned i = 0; i < 4; ++i) oldLocal.mf[i][i] = newLocal.mf[i][i] = view.mf[i][i] = 1.0f;
    oldLocal.xw = 12.0f; oldLocal.yw = -8.0f; oldLocal.zw = -30.0f;
    newLocal = oldLocal; newLocal.yw += 0.6f;
    firstPersonMatrices = {{&modelKey, newLocal}};
    previousFirstPersonMatrices = {oldLocal};
    MtxF oldArrow = oldLocal, newArrow = newLocal;
    oldArrow.xw += 3.0f; newArrow.xw += 3.0f;
    firstPersonAttachments = {{&attachmentKey, newArrow}};
    previousFirstPersonAttachments = {oldArrow};
    firstPersonAttachmentOwner = previousFirstPersonAttachmentOwner = &attachmentKey;
    firstPersonOwner = previousFirstPersonOwner = &modelKey;
    firstPersonProfile = previousFirstPersonProfile = 1;
    firstPersonEpoch = previousFirstPersonEpoch = 0;
    firstPersonViewKey = &viewKey;
    unsigned failures = 0;
    for (unsigned frame = 0; frame <= 6; ++frame) {
        const float alpha = frame / 6.0f, angle = alpha * 0.5f;
        view.xx = view.zz = std::cos(angle);
        view.xz = std::sin(angle); view.zx = -view.xz;
        view.xw = -1700.0f - alpha * 50.0f; view.zw = -5400.0f;
        std::unordered_map<Mtx*, MtxF> output;
        FrameInterpolation_RebaseFirstPerson(output, &viewKey, view, alpha);
        MtxF displayed;
        SkinMatrix_MtxFMtxFMult(&view, &output.at(&modelKey), &displayed);
        MtxF displayedArrow;
        SkinMatrix_MtxFMtxFMult(&view, &output.at(&attachmentKey), &displayedArrow);
        failures += std::fabs(displayedArrow.xw - displayed.xw - 3.0f) > 0.002f;
        failures += std::fabs(displayedArrow.yw - displayed.yw) > 0.002f;
        for (unsigned row = 0; row < 4; ++row) for (unsigned col = 0; col < 4; ++col) {
            const float expected = oldLocal.mf[row][col] * (1.0f-alpha) + newLocal.mf[row][col] * alpha;
            failures += std::fabs(displayed.mf[row][col] - expected) > 0.002f;
        }
    }
    // Changed weapon/profile cannot blend unrelated limb layouts.
    firstPersonProfile = 2;
    std::unordered_map<Mtx*, MtxF> output;
    FrameInterpolation_RebaseFirstPerson(output, &viewKey, view, 0.0f);
    MtxF displayed;
    SkinMatrix_MtxFMtxFMult(&view, &output.at(&modelKey), &displayed);
    failures += std::fabs(displayed.yw - newLocal.yw) > 0.002f;
    firstPersonAttachments.clear(); previousFirstPersonAttachments.clear();
    firstPersonAttachmentOwner = previousFirstPersonAttachmentOwner = nullptr;
    output.clear();
    FrameInterpolation_RebaseFirstPerson(output, &viewKey, view, 0.5f);
    failures += output.count(&attachmentKey) != 0;
    firstPersonMatrices.clear(); previousFirstPersonMatrices.clear();
    firstPersonOwner = previousFirstPersonOwner = nullptr; firstPersonViewKey = nullptr;
    return failures;
}

static bool invert_matrix(const float m[16], float invOut[16]) {
    float inv[16], det;
    int i;

    // clang-format off
    inv[0] = m[5]  * m[10] * m[15] -
             m[5]  * m[11] * m[14] -
             m[9]  * m[6]  * m[15] +
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] -
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] +
              m[4]  * m[11] * m[14] +
              m[8]  * m[6]  * m[15] -
              m[8]  * m[7]  * m[14] -
              m[12] * m[6]  * m[11] +
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] -
             m[4]  * m[11] * m[13] -
             m[8]  * m[5] * m[15] +
             m[8]  * m[7] * m[13] +
             m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] +
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] -
               m[8]  * m[6] * m[13] -
               m[12] * m[5] * m[10] +
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] +
              m[1]  * m[11] * m[14] +
              m[9]  * m[2] * m[15] -
              m[9]  * m[3] * m[14] -
              m[13] * m[2] * m[11] +
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] -
             m[0]  * m[11] * m[14] -
             m[8]  * m[2] * m[15] +
             m[8]  * m[3] * m[14] +
             m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] +
              m[0]  * m[11] * m[13] +
              m[8]  * m[1] * m[15] -
              m[8]  * m[3] * m[13] -
              m[12] * m[1] * m[11] +
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] -
              m[0]  * m[10] * m[13] -
              m[8]  * m[1] * m[14] +
              m[8]  * m[2] * m[13] +
              m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] -
             m[1]  * m[7] * m[14] -
             m[5]  * m[2] * m[15] +
             m[5]  * m[3] * m[14] +
             m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] +
              m[0]  * m[7] * m[14] +
              m[4]  * m[2] * m[15] -
              m[4]  * m[3] * m[14] -
              m[12] * m[2] * m[7] +
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] -
              m[0]  * m[7] * m[13] -
              m[4]  * m[1] * m[15] +
              m[4]  * m[3] * m[13] +
              m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] +
               m[0]  * m[6] * m[13] +
               m[4]  * m[1] * m[14] -
               m[4]  * m[2] * m[13] -
               m[12] * m[1] * m[6] +
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
              m[1] * m[7] * m[10] +
              m[5] * m[2] * m[11] -
              m[5] * m[3] * m[10] -
              m[9] * m[2] * m[7] +
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
             m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
               m[0] * m[7] * m[9] +
               m[4] * m[1] * m[11] -
               m[4] * m[3] * m[9] -
               m[8] * m[1] * m[7] +
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
              m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];
    // clang-format on

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) {
        return false;
    }

    det = 1.0f / det;

    for (i = 0; i < 16; i++) {
        invOut[i] = inv[i] * det;
    }

    return true;
}
