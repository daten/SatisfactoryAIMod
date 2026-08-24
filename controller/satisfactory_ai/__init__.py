"""External controller skeleton for the Satisfactory AI interface.

PLAN.md Phase 8. Deliberately minimal at this stage: protocol models and
telemetry parsing only. No LLM dependency, no optimization solver, no
game-control intelligence, and no network client yet - Phase 9 defines the
mod-side transport this would eventually talk to; until that exists, this
package only parses telemetry JSON captured/logged from the mod.
"""

__version__ = "0.1.0"
