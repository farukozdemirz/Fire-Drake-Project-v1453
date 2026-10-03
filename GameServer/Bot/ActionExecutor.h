#pragma once

#include <chrono>
#include <string>
#include "BotSession.h"

// One Move/Stop action's outcome. 'reason' is a constant string, never freed.
struct MoveOutcome
{
	enum Kind { NOTHING, SENT, ARRIVED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // "ok", "not_in_game", "dead", "sitting", "bad_target", "speed_field",
	                       // "step_too_long", "handler_noop"
};

// Caller-supplied view of the target (ADR-0017 Ek F4-02). Temporary: the /bot attack test driver fills it
// from the target bot's session; the Perception slice replaces the source, not this struct.
struct AttackTarget
{
	int16 id;      // target's socket id (WIZ_ATTACK 'tid')
	float x;       // target position, metres
	float z;
};

struct AttackOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "hit", "killed", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "sitting", "bad_target", "out_of_range", "too_soon", "rate"
};

// Caller-supplied view of the cast target (ADR-0017 Ek F4-03). Temporary, like AttackTarget: the /bot cast test
// driver fills it from the target bot's session (or from the caster itself for "self"). For an area skill
// (MAGIC.Moral 10) 'x/y/z' is the aim point (the target bot's position or the caster's own for "self") and the
// packet's target id is -1 (ADR-0017 Ek F4-29).
struct CastTarget
{
	int16 id;      // target's id (WIZ_MAGIC_PROCESS 'target')
	float x;       // metres
	float y;
	float z;
	bool isSelf;
};

struct CastOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "casting", "flying", "effected", "missed", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "sitting", "bad_skill", "unsupported_skill", "quest_locked", "bad_target",
	                       // "out_of_range", "not_standing", "no_mana", "recast", "type_gate", "gap", "rate", "too_early",
	                       // "stopping", "cancelled", "dropped", "idle"
	                       // single Type4 (ADR-0017 Ek F4-28): "effected" carries the duration in the echo code, a redundant
	                       // buff gives "srv_fail" (docs/03 MEC-MAG-15), a dead / out-of-range target gives "no_result"
};

struct PotionOutcome
{
	enum Kind { NOTHING, SENT, FINISHED, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "ok", "effected", "srv_fail", "no_result",
	                       // "not_in_game", "dead", "bad_item", "unsupported_item", "no_stock", "pot_cooldown", "rate"
};

struct StanceOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "applied" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "dead", "no_change", "busy", "toggle", "rate"
};

// Caller-supplied view of the HP request target (ADR-0017 Ek F4-06). Temporary, like AttackTarget: the /bot target
// test driver fills it from the target bot's session; the Perception slice replaces the source, not this struct.
struct TargetHpTarget
{
	int16 id;      // target's socket id (WIZ_TARGET_HP 'uid')
	float x;       // metres
	float z;
};

struct TargetHpOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "observed" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "dead", "bad_target", "out_of_view", "poll", "rate"
	int32 hp;              // from the server's reply; valid only when kind == SENT
	int32 maxHp;
};

struct RegeneOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed: "respawned" (SENT), "no_result" (FAILED),
	                       // REFUSED: "not_in_game", "not_dead", "no_np", "dead_wait", "rate"
	float x;               // metres, from the server's reply; valid only when kind == SENT
	float z;
};

// Caller-supplied view of the party invitation target (ADR-0017 Ek F4-08). Temporary, like TargetHpTarget: the
// /bot pinvite test driver fills it from the target bot's session; the Perception slice replaces the source.
struct PartyInviteTarget
{
	int16 id;            // target's socket id (not sent; self / invalid check)
	std::string name;    // target's character name (the PARTY_CREATE / PARTY_INSERT payload)
	float x;             // metres
	float z;
};

struct PartyOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "created" (PartyInvite, PARTY_CREATE confirmed by the bot's own
	                       // leader state broadcast), "sent" (PartyInvite, PARTY_INSERT: no refusal reply), "joined" (PartyAccept),
	                       // "declined" (PartyDecline), "left"/"disbanded" (PartyLeave), "promoted" (PartyPromote),
	                       // "kicked"/"disbanded" (PartyKick).
	                       // FAILED: "refused_target" (-1), "refused_level" (-2), "refused_zone" (-3), "refused_other", "no_result".
	                       // REFUSED: "not_in_game", "dead", "bad_target", "no_invite", "invite_pending", "not_in_party", or a guard
	                       // verdict ("not_leader", "not_member", "out_of_view", "invite_gap", "accept_wait", "decline_wait", "leave_wait",
	                       // "manage_gap", "rate")
	int peerId;            // PartyInvite / PartyPromote / PartyKick: the target's id; PartyAccept/PartyDecline: the inviter's id; PartyLeave: -1 = none
};

// Caller-supplied view of a party member the leader acts on (ADR-0017 Ek F4-10). Temporary like PartyInviteTarget: the
// /bot ppromote and /bot pkick test driver fills it from the two bot sessions; the Perception slice replaces the source.
struct PartyMemberTarget
{
	int16 id;             // target's socket id (the packet payload)
	bool inMyParty;       // the target is in the acting bot's party (the party panel lists it)
};

// Result of ActionExecutor::RequestChatParty (ADR-0017 Ek F4-11).
struct ChatOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "sent" (the bot's own party chat broadcast came back).
	                       // FAILED: "no_result" (no matching broadcast: muted, jailed, no longer in a party...).
	                       // REFUSED: "not_in_game", "dead", "invite_pending", "not_in_party", or a guard verdict
	                       // ("bad_text", "chat_gap", "chat_dup", "chat_minute", "rate")
	int length;            // message length in bytes (0 when refused before the text was looked at)
};

// Result of ActionExecutor::TickUserIn (ADR-0017 Ek F4-13).
struct UserInOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "received" (the reply arrived). FAILED: "no_result" (no reply).
	                       // REFUSED: "bad_count" (guard CLI-19; FAIRNESS_REJECT written). NOTHING: "ok".
	int requested;         // ids in the request (0 when NOTHING)
	int received;          // units the reply carried; valid only when kind == SENT
};

// Result of ActionExecutor::TickNpcIn (ADR-0017 Ek F4-15).
struct NpcInOutcome
{
	enum Kind { NOTHING, SENT, REFUSED, FAILED };
	Kind kind;
	const char * reason;   // constant text, never freed. SENT: "received" (the reply arrived). FAILED: "no_result" (no reply).
	                       // REFUSED: "bad_count" (guard CLI-20; FAIRNESS_REJECT written). NOTHING: "ok".
	int requested;         // ids in the request (0 when NOTHING)
	int received;          // NPCs the reply carried; valid only when kind == SENT
};

// Turns Move/Stop intents into real WIZ_MOVE packets and runs them through CUser::HandlePacket()
// (ADR-0017). IOCP thread only. No logging, no locking, no console output.
class ActionExecutor
{
public:
	// Validates and arms a walk to (tx,tz) at 'speedField'; sends nothing yet (the same Tick()'s TickMove() does).
	static MoveOutcome BeginMove(BotSession * s, float tx, float tz, int16 speedField,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session.
	static MoveOutcome TickMove(BotSession * s, std::chrono::steady_clock::time_point now);

	// Ends an active walk: sends a stop packet at the current position.
	static MoveOutcome StopMove(BotSession * s, std::chrono::steady_clock::time_point now);

	// Clears the walk state without sending anything (despawn).
	static void AbandonMove(BotSession * s);

	// Validates and arms an attack series of 'count' R hits on the target bot 'targetName'; sends nothing yet
	// (the same Tick()'s TickAttack() does). REFUSED (nothing armed) when the session is not in game / dead
	// ("not_in_game", "dead") or count < 1 / targetName empty ("bad_target").
	static AttackOutcome BeginAttack(BotSession * s, const std::string & targetName, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_attackActive and the CLI-01 interval
	// (or the series start) allows a hit. Sends at most one WIZ_ATTACK. 'target' is the caller's current view.
	// SENT: hit sent, series continues. FINISHED: last hit sent or target killed (reason "ok"/"hit"/"killed").
	// REFUSED: guard rejected, series dropped. FAILED: handler produced no result / dead attacker, series dropped.
	static AttackOutcome TickAttack(BotSession * s, const AttackTarget & target,
		std::chrono::steady_clock::time_point now);

	// Clears the attack state without sending anything (stop, despawn, target lost).
	static void EndAttack(BotSession * s);

	// Validates and arms a cast series of 'count' cycles of skill 'skillId'; sends nothing yet (the same Tick()'s
	// TickCast() does). 'targetName' empty = self. REFUSED (nothing armed): "not_in_game", "dead",
	// "bad_skill" (unknown id, other class, level too low, count < 1), "unsupported_skill" (see 5.4 rules;
	// flying Type3 single-typed skills are supported, ADR-0017 Ek F4-25; single Type3 + Type4 (dual-typed) skills are
	// supported, ADR-0017 Ek F4-26; single Type4 (buff/debuff; Moral 1, 2, 7; ADR-0017 Ek F4-28) is supported;
	// area Moral 10 (non-flying; aim point = the target's position, within MAGIC.Range of the caster, CLI-07) is
	// supported, and flying area skills (Fire/Ice/Thunder burst: Moral 10 + Type3 FlyingEffect, run as
	// CASTING -> FLYING -> EFFECTING with target id -1) are supported too, ADR-0017 Ek F4-29/F4-30),
	// "quest_locked" (the skill's MAGIC.Etc quest is not completed; docs/03 MEC-MAG-14),
	// "bad_target" (moral does not match the target kind).
	static CastOutcome BeginCast(BotSession * s, uint32 skillId, const std::string & targetName, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_castPhase != CAST_IDLE and the timing
	// rules (CastWaitMs / CastDurationMs) allow the next packet. Sends at most one WIZ_MAGIC_PROCESS.
	// SENT: a packet went out and the series continues ("casting" = CASTING accepted, "flying" = FLYING accepted,
	// "effected"/"missed"/"srv_fail" = one cycle finished). FINISHED: last cycle done. REFUSED: guard rejected, series
	// dropped. FAILED: handler produced no result / dead caster / unknown skill, series dropped.
	// dual-typed: the EFFECTING echo comes from the Type4 part (code = duration), "missed" is never reported,
	// no echo = "no_result".
	// single Type4: the EFFECTING echo carries the duration in "code" (never "missed"); a buff whose BuffType is already
	// on the target fails with "srv_fail" (docs/03 MEC-MAG-15); out-of-range or dead target gives "no_result".
	// area: the EFFECTING echo is the last packet the server sent (Type3: target -1 broadcast, code 0; {3, 4}/{4, 0}:
	// last victim's Type4 packet, code = duration); an empty area still gives "effected" and costs MP; "victims" in
	// ACTION_RESULT counts the per-victim EFFECTING packets (docs/03 MEC-MAG-16).
	// flying area: FLYING and EFFECTING carry the same target id -1 and aim point; MP is charged at FLYING and again at
	// EFFECTING (2 x Msp, docs/03 MEC-MAG-12/-17); "victims" counts the per-victim EFFECTING packets of the EFFECTING
	// step only.
	static CastOutcome TickCast(BotSession * s, const CastTarget & target,
		std::chrono::steady_clock::time_point now);

	// Clears the cast state without sending anything (stop, despawn, target lost). Keeps the reuse timers.
	static void EndCast(BotSession * s);

	// Ends the cast series. Only a series whose CASTING packet is in flight (m_castPhase == CAST_CASTING) needs a packet:
	// one WIZ_MAGIC_PROCESS MAGIC_FAIL with sData[3] = -100 through CUser::HandlePacket() after the guard (CLI-03, CLI-11).
	// 'cause' ("cmd" / "move") is telemetry text only. Result only from the reply the server published to the caster
	// (m_castEcho). NOTHING "idle": no series. NOTHING "dropped": ARMED series, nothing in flight, dropped without a
	// packet (or the session cannot send). SENT "cancelled": the echo (MAGIC_FAIL, -100) arrived, series ended.
	// FAILED "no_result": no echo, series ended anyway. REFUSED "rate": guard (FAIRNESS_REJECT written), series KEPT.
	static CastOutcome CancelCast(BotSession * s, const char * cause, std::chrono::steady_clock::time_point now);

	// Classifies one bag item for the perception snapshot: 1 = HP pot, 2 = MP pot, 0 = this bot cannot drink it
	// (unknown item, no Effect1, class/level mismatch, unknown skill, or not a supported pot shape). Applies the same
	// rules as BeginPotion (keep them in sync). Touches nothing; the stock is NOT checked.
	static uint8 PotKindOf(CUser * user, uint32 itemId);

	// Validates and arms a series of 'count' pots of ITEM 'itemId'; sends nothing yet (the same Tick()'s TickPotion()
	// does). REFUSED (nothing armed): "not_in_game", "dead", "bad_item" (count < 1, unknown item, no Effect1, level or
	// class does not match, unknown skill), "unsupported_item" (see 5.4 rules). The bag stock is NOT checked here:
	// the guard checks it before every packet, so the refusal is visible as FAIRNESS_REJECT (CLI-06).
	static PotionOutcome BeginPotion(BotSession * s, uint32 itemId, uint32 count,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game session; NOTHING unless s->m_potActive and the shared pot timer allows
	// the next pot. Sends at most one WIZ_MAGIC_PROCESS (MAGIC_EFFECTING). SENT: pot sent, series continues.
	// FINISHED: last pot sent. REFUSED: guard rejected, series dropped. FAILED: server answered MAGIC_FAIL or
	// published nothing, series dropped.
	static PotionOutcome TickPotion(BotSession * s, std::chrono::steady_clock::time_point now);

	// Clears the pot series without sending anything (stop, despawn). Keeps the shared pot timer.
	static void EndPotion(BotSession * s);

	// One-shot stance change: sit down (sit = true) or stand up. Validates, runs the guard, sends one WIZ_STATE_CHANGE
	// (type 1) through CUser::HandlePacket() and maps the result from the broadcast the server published (m_stateEcho).
	// SENT "applied": the broadcast carried the requested stance. FAILED "no_result": no/other broadcast.
	// REFUSED: "not_in_game", "dead", "no_change" (already in that stance; no event) or a guard verdict ("busy", "toggle",
	// "rate"; FAIRNESS_REJECT written).
	static StanceOutcome SetStance(BotSession * s, bool sit, std::chrono::steady_clock::time_point now);

	// One-shot target selection + HP request: sends one WIZ_TARGET_HP through CUser::HandlePacket() after the guard
	// (CLI-10: target inside the bot's 3x3 regions, same target re-polled at most every 2 s; CLI-11) and maps the result
	// from the reply the server published (m_targetHpEcho / m_targetHpValues). SENT "observed": a reply for that target
	// arrived (hp / maxHp filled). FAILED "no_result": no reply (target dead or unknown to the server).
	// REFUSED: "not_in_game", "dead", "bad_target" (id < 0 or the bot itself) or a guard verdict ("out_of_view", "poll",
	// "rate"; FAIRNESS_REJECT written).
	static TargetHpOutcome RequestTargetHp(BotSession * s, const TargetHpTarget & target,
		std::chrono::steady_clock::time_point now);

	// One-shot respawn: sends one WIZ_REGENE (type 1) through CUser::HandlePacket() after the guard (CLI-14: at least
	// 3 s after the death was noticed; CLI-11) and maps the result from the WIZ_REGENE reply the server published
	// (m_regeneEcho). SENT "respawned": the reply arrived (x / z filled). FAILED "no_result": no reply.
	// REFUSED without an event: "not_in_game", "not_dead", "no_np" (loyalty 0: the server would kick the bot out of the
	// zone, KI-013); with FAIRNESS_REJECT: "dead_wait", "rate".
	static RegeneOutcome RequestRegene(BotSession * s, std::chrono::steady_clock::time_point now);

	// One-shot party invitation: sends one WIZ_PARTY (PARTY_CREATE when the bot is in no party, PARTY_INSERT when it leads
	// one) with the target's name through CUser::HandlePacket() after the guard (CLI-15: leader only, target in the 3x3
	// regions, >= 1 s between invitations; CLI-11). Result only from published replies: a refusal arrives as WIZ_PARTY
	// PARTY_INSERT + i16 code (FAILED "refused_*"); PARTY_CREATE is confirmed by the bot's own WIZ_STATE_CHANGE type 6
	// (leader flag 1) -> SENT "created"; PARTY_INSERT has no positive reply -> SENT "sent" (no refusal reply; [A]).
	static PartyOutcome RequestPartyInvite(BotSession * s, const PartyInviteTarget & target,
		std::chrono::steady_clock::time_point now);

	// One-shot acceptance of the pending invitation (PARTY_PERMIT 1 through CUser::HandlePacket()) after the guard
	// (CLI-15: >= 1 s after the invitation arrived; CLI-11). The pending invitation is the one OnPacket() recorded
	// (m_partyInviteEcho); none -> REFUSED "no_invite" without an event. SENT "joined": the bot's own PARTY_INSERT member
	// packet (sid == its id, flag 1) arrived. FAILED "no_result": it did not (e.g. the leader changed zone).
	static PartyOutcome RequestPartyAccept(BotSession * s, std::chrono::steady_clock::time_point now);

	// One-shot decline of the pending invitation (PARTY_PERMIT 0 through CUser::HandlePacket()) after the guard (CLI-16:
	// >= 1 s after the invitation arrived; CLI-11). The pending invitation is the one OnPacket() recorded
	// (m_partyInviteEcho); none -> REFUSED "no_invite" without an event. The server sends the decliner no reply (it
	// answers the leader), so the result is SENT "declined" once the packet went out ([A]); the invitation record is consumed.
	static PartyOutcome RequestPartyDecline(BotSession * s, std::chrono::steady_clock::time_point now);

	// One-shot party leave (PARTY_REMOVE with the bot's own id through CUser::HandlePacket()) after the guard (CLI-16:
	// >= 1 s after the bot entered the party, when known; CLI-11). Preconditions without an event: REFUSED "invite_pending"
	// (an invitation must be accepted or declined first), "not_in_party". Result only from published replies: the bot's
	// own PARTY_REMOVE (sid == its id) -> SENT "left"; PARTY_DELETE -> SENT "disbanded" (it led the party, or only the
	// leader remained); neither -> FAILED "no_result".
	static PartyOutcome RequestPartyLeave(BotSession * s, std::chrono::steady_clock::time_point now);

	// One-shot leader handover (PARTY_PROMOTE + the target's id through CUser::HandlePacket()) after the guard (CLI-17:
	// leader only, target in the bot's party, >= 1 s between leader actions; CLI-11). Preconditions without an event:
	// REFUSED "not_in_game", "dead", "bad_target" (invalid id or the bot itself), "not_in_party". Result only from
	// published replies: the server broadcasts the new leader's member packet (PARTY_INSERT, sid == target, flag 100) to
	// every member including the sender -> SENT "promoted"; none -> FAILED "no_result".
	static PartyOutcome RequestPartyPromote(BotSession * s, const PartyMemberTarget & target,
		std::chrono::steady_clock::time_point now);

	// One-shot kick (PARTY_REMOVE + the target's id through CUser::HandlePacket()) after the same guard and preconditions
	// as RequestPartyPromote. Result only from published replies: the sender's own PARTY_REMOVE with sid == target ->
	// SENT "kicked"; PARTY_DELETE (only the leader remained) -> SENT "disbanded"; neither -> FAILED "no_result".
	static PartyOutcome RequestPartyKick(BotSession * s, const PartyMemberTarget & target,
		std::chrono::steady_clock::time_point now);

	// One-shot party chat message (WIZ_CHAT: PARTY_CHAT + the text through CUser::HandlePacket()) after the guard (CLI-18:
	// printable ASCII 1..128 bytes not starting with '+', >= 4 s since the last message, not the same text within 8 s,
	// <= 6 messages per minute; CLI-11). Preconditions without an event: REFUSED "not_in_game", "dead", "invite_pending"
	// (an invitation must be accepted or declined first), "not_in_party". Result only from the published reply: the server
	// broadcasts the message to every party member including the sender -> the bot's own WIZ_CHAT (type PARTY_CHAT,
	// sender == its id, same text hash) -> SENT "sent"; none -> FAILED "no_result".
	static ChatOutcome RequestChatParty(BotSession * s, const std::string & text,
		std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game, living session. Sends one WIZ_REQ_USERIN through CUser::HandlePacket()
	// for the ids the last WIZ_REGIONCHANGE listed and the observation table does not know (at most kUserInMaxIds; the
	// bot's own id is never requested) when the CLI-19 guard allows it (>= 1 s since the previous request). NOTHING when
	// there is nothing to ask or the gap has not passed (the ids stay pending). Result only from the reply the server
	// published (m_userInEcho). Not counted in the CLI-11 window (automatic client traffic).
	static UserInOutcome TickUserIn(BotSession * s, std::chrono::steady_clock::time_point now);

	// Called once per Tick() for every in-game, living session. Sends one WIZ_REQ_NPCIN through CUser::HandlePacket()
	// for the ids the last WIZ_NPC_REGION listed and the NPC table does not know (at most kNpcInMaxIds) when the CLI-20
	// guard allows it (>= 1 s since the previous request). NOTHING when there is nothing to ask or the gap has not
	// passed (the ids stay pending). Result only from the reply the server published (m_npcInEcho). Not counted in the
	// CLI-11 window (automatic client traffic).
	static NpcInOutcome TickNpcIn(BotSession * s, std::chrono::steady_clock::time_point now);
};
