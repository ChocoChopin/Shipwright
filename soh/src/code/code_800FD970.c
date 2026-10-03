#include "global.h"
#include "soh/NativeSimulationTest.h"

// The latest generated random number, used to generate the next number in the sequence.
static u32 sRandInt = 1;

// Space to store a value to be re-interpreted as a float.
static u32 sRandFloat;

/**
 * Gets the next integer in the sequence of pseudo-random numbers.
 */
u32 Rand_Next(void) {
    sRandInt = (sRandInt * 1664525) + 1013904223;
    NativeSimTest_Rng("gameplay", "Rand_Next", sRandInt);
    return sRandInt;
}

/**
 * Seeds the pseudo-random number generator by providing a starting value.
 */
void Rand_Seed(u32 seed) {
    sRandInt = seed;
    NativeSimTest_Event("rng-seed", "Rand_Seed", seed);
}

/**
 * Returns a pseudo-random floating-point number between 0.0f and 1.0f, by generating
 * the next integer and masking it to an IEEE-754 compliant floating-point number
 * between 1.0f and 2.0f, returning the result subtract 1.0f.
 */
f32 Rand_ZeroOne(void) {
    sRandInt = (sRandInt * 1664525) + 1013904223;
    sRandFloat = ((sRandInt >> 9) | 0x3F800000);
    NativeSimTest_Rng("gameplay", "Rand_ZeroOne", sRandInt);
    return *((f32*)&sRandFloat) - 1.0f;
}

/**
 * Returns a pseudo-random floating-point number between -0.5f and 0.5f by the same
 * manner in which Rand_ZeroOne generates its result.
 */
f32 Rand_Centered(void) {
    sRandInt = (sRandInt * 1664525) + 1013904223;
    sRandFloat = ((sRandInt >> 9) | 0x3F800000);
    NativeSimTest_Rng("gameplay", "Rand_Centered", sRandInt);
    return *((f32*)&sRandFloat) - 1.5f;
}

/**
 * Seeds a pseudo-random number at rndNum with a provided seed.
 */
void Rand_Seed_Variable(u32* rndNum, u32 seed) {
    *rndNum = seed;
    NativeSimTest_Event("rng-seed", "Rand_Seed_Variable", seed);
}

/**
 * Generates the next pseudo-random integer from the provided rndNum.
 */
u32 Rand_Next_Variable(u32* rndNum) {
    *rndNum = (*rndNum * 1664525) + 1013904223;
    NativeSimTest_Rng("variable", "Rand_Next_Variable", *rndNum);
    return *rndNum;
}

/**
 * Generates the next pseudo-random floating-point number between 0.0f and
 * 1.0f from the provided rndNum.
 */
f32 Rand_ZeroOne_Variable(u32* rndNum) {
    u32 next = (*rndNum * 1664525) + 1013904223;

    // clang-format off
    *rndNum = next; sRandFloat = (next >> 9) | 0x3F800000;
    // clang-format on
    NativeSimTest_Rng("variable", "Rand_ZeroOne_Variable", next);
    return *((f32*)&sRandFloat) - 1.0f;
}

/**
 * Generates the next pseudo-random floating-point number between -0.5f and
 * 0.5f from the provided rndNum.
 */
f32 Rand_Centered_Variable(u32* rndNum) {
    u32 next = (*rndNum * 1664525) + 1013904223;

    // clang-format off
    *rndNum = next; sRandFloat = (next >> 9) | 0x3F800000;
    // clang-format on
    NativeSimTest_Rng("variable", "Rand_Centered_Variable", next);
    return *((f32*)&sRandFloat) - 1.5f;
}
