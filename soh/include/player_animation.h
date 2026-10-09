#ifndef PLAYER_ANIMATION_H
#define PLAYER_ANIMATION_H
#include "z64animation.h"

struct PlayState;

/* A synchronous intermediate Player interval owns this queue. The shared-boundary
 * interval uses the original world queue and its original drain location instead.
 * No queued entry or frame-table pointer may outlive this scope. */
typedef struct {
    AnimationContext queue;
    struct PlayState* play;
    u32 savedQueueFlags;
    u32 savedDisabledFlags;
    u8 overflowed;
} PlayerAnimationQueue;

/* Requires the world queue to have drained. A rejected begin changes no state.
 * The caller must already have admitted the complete Player dependency closure. */
s32 PlayerAnimation_BeginQueue(struct PlayState* play, PlayerAnimationQueue* scope);
/* Executes only entries produced inside this scope, then restores queue statics.
 * Returns false on overflow or an invalid scope; never deliberately faults. */
s32 PlayerAnimation_EndQueue(PlayerAnimationQueue* scope);

#endif
