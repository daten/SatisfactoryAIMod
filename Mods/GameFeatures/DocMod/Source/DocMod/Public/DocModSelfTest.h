// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Lightweight self-test harness (no external test framework) that runs
 * automatically whenever a game world finishes loading, so DocMod
 * functionality gets exercised on every launch/PIE session without a
 * human manually working through docs/manual-verification.md.
 *
 * This is trustworthy because the real FactoryGame implementation is
 * what's actually running (see docs/factorygame-binary-provenance.md) -
 * these checks exercise production game logic, not stub placeholders.
 *
 * Deliberately narrow scope: only checks safe to run automatically on
 * every launch, including against a real save -
 *   - read-only telemetry: doesn't crash, returned data is well-formed
 *     (non-empty ids/classes, enum fields are one of the known values),
 *     and JSON serialization round-trips through a real JSON parser.
 *   - write operations: NEGATIVE/validation-path only (e.g. "does an
 *     unknown target id correctly get rejected"). Never a positive-path
 *     mutation against real game state - that still needs deliberate
 *     manual testing on a disposable save, see
 *     docs/manual-verification.md and docs/operations-protocol.md.
 *
 * Add a new Check*() function here whenever new DocMod functionality is
 * added, and call it from RunAll(), rather than only documenting a
 * manual verification step for it.
 *
 * Compiled out of Shipping builds entirely (see DocMod.cpp) - this is a
 * development-time convenience, not something that should run for
 * players of a released mod.
 */
namespace DocModSelfTest
{
	/** Runs all self-tests against the given world and logs a summary via LogDocModAI. Safe to call on any game world, including a real save. No-op if World is null. */
	void RunAll(UWorld* World);
}
