#include "pch.h"

#include "activation.h"

#include "arcane_activation/interaction.h"
#include "arcane_activation/timing.h"
#include "config.h"

namespace arcane_activation::activation {
namespace {
using clock = std::chrono::steady_clock;

constexpr std::array spell_editor_ids{
    "REQ_Alteration1_Open_Self", "REQ_Alteration2_Open_Self",
    "REQ_Alteration3_Open_Self", "REQ_Alteration4_Open_Self",
    "REQ_Alteration5_Open_Self"};
constexpr std::string_view marker_spell_editor_id = "AA_ProxySpell";
constexpr std::string_view cast_fx_spell_editor_id = "AA_CastFXSpell";
constexpr std::string_view cast_fx_art_editor_id = "AA_AlterationGreenHandArt";
constexpr std::string_view cast_fx_art_resource_path =
    R"(meshes\SpellHotbar\paralyzemasshandeffects_l.nif)";
constexpr std::array animation_resource_paths{
    R"(meshes\actors\character\OpenAnimationReplacer\ArcaneActivation\config.json)",
    R"(meshes\actors\character\OpenAnimationReplacer\ArcaneActivation\cast_left_aimed\config.json)",
    R"(meshes\actors\character\OpenAnimationReplacer\ArcaneActivation\cast_left_aimed\animations\mt_shout_inhale.hkx)",
    R"(meshes\actors\character\OpenAnimationReplacer\ArcaneActivation\cast_left_aimed\_1stperson\animations\mt_shout_inhale.hkx)"};
constexpr auto animation_start_timeout = std::chrono::milliseconds(500);
constexpr auto post_release_cleanup = std::chrono::milliseconds(1100);
constexpr auto hard_timeout = std::chrono::milliseconds(2500);
constexpr float target_fx_duration_seconds = 1.1F;

enum class cast_phase : std::uint8_t {
  waiting_for_animation,
  charging,
  released,
};

std::string_view phase_name(const cast_phase phase) {
  switch (phase) {
  case cast_phase::waiting_for_animation:
    return "waiting_for_animation";
  case cast_phase::charging:
    return "charging";
  case cast_phase::released:
    return "released";
  }
  return "unknown";
}

struct binding {
  std::optional<std::uint32_t> keyboard;
  std::optional<std::uint32_t> gamepad;
};

struct activation {
  RE::ObjectRefHandle target;
  RE::SpellItem* spell = nullptr;
  float cost = 0.0F;
};

struct casting_transaction {
  activation selected;
  clock::time_point began;
  std::optional<clock::time_point> animation_started_at;
  std::optional<clock::time_point> released_at;
  cast_phase phase = cast_phase::waiting_for_animation;

  bool marker_added = false;
  bool fx_added = false;
  bool fx_loss_logged = false;
  bool notify_start_result = false;
  bool notify_release_result = false;
  bool notify_stop_result = false;
  bool saw_is_shouting = false;
  bool saw_animation_exit = false;
  bool committed = false;
  bool menu_opened_during_dispatch = false;
  bool target_fx_requested = false;
  bool target_fx_applied = false;
  std::uint32_t animation_event_count = 0;

  bool observation_initialized = false;
  bool last_is_shouting = false;
  bool last_marker_known = false;
  bool last_fx_known = false;
  std::uint32_t last_camera_state =
      (std::numeric_limits<std::uint32_t>::max)();
  std::uint32_t last_weapon_state =
      (std::numeric_limits<std::uint32_t>::max)();
  cast_phase last_phase = cast_phase::waiting_for_animation;
};

struct suppressed_press {
  RE::INPUT_DEVICE device = RE::INPUT_DEVICE::kKeyboard;
  std::uint32_t button = 0;
};

struct state {
  config::t configuration;
  binding activate;
  std::array<RE::SpellItem*, 5> spells{};
  RE::SpellItem* marker_spell = nullptr;
  RE::SpellItem* cast_fx_spell = nullptr;
  RE::BGSArtObject* cast_fx_art = nullptr;
  bool animation_resources_ready = false;
  bool animation_ready = false;
  bool fx_resource_ready = false;
  bool fx_ready = false;

  std::optional<casting_transaction> transaction;
  std::optional<suppressed_press> suppressed;
  bool animation_sink_registered = false;
  bool unlock_dispatch_in_progress = false;
};

state& get_state() {
  static state instance;
  return instance;
}

RE::FormID form_id(const RE::TESForm* form) {
  return form == nullptr ? 0 : form->GetFormID();
}

bool graph_bool(RE::PlayerCharacter* player, std::string_view name) {
  bool value = false;
  return player != nullptr &&
         player->GetGraphVariableBool(RE::BSFixedString(name), value) && value;
}

bool spell_known(RE::PlayerCharacter* player, RE::SpellItem* spell) {
  return player != nullptr && spell != nullptr && player->HasSpell(spell);
}

std::uint32_t camera_state_id() {
  const auto* camera = RE::PlayerCamera::GetSingleton();
  return camera == nullptr || camera->currentState == nullptr
             ? (std::numeric_limits<std::uint32_t>::max)()
             : static_cast<std::uint32_t>(camera->currentState->id);
}

std::string_view camera_mode() {
  const auto* camera = RE::PlayerCamera::GetSingleton();
  if (camera == nullptr) {
    return "unavailable";
  }
  if (camera->IsInFirstPerson()) {
    return "first_person";
  }
  if (camera->IsInThirdPerson()) {
    return "third_person";
  }
  return "other";
}

std::uint32_t weapon_state(RE::PlayerCharacter* player) {
  return player == nullptr
             ? (std::numeric_limits<std::uint32_t>::max)()
             : static_cast<std::uint32_t>(
                   player->AsActorState()->GetWeaponState());
}

bool uses_combat_visual_fallback(RE::PlayerCharacter* player) {
  return player != nullptr && player->AsActorState()->GetWeaponState() !=
                                  RE::WEAPON_STATE::kSheathed;
}

std::int64_t elapsed_milliseconds(const casting_transaction& transaction) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
                                                                transaction.began)
      .count();
}

void log_state_change(casting_transaction& transaction,
                      RE::PlayerCharacter* player) {
  const state& current = get_state();
  const bool is_shouting = graph_bool(player, "IsShouting");
  const bool marker_known = spell_known(player, current.marker_spell);
  const bool fx_known = spell_known(player, current.cast_fx_spell);
  const std::uint32_t camera_state = camera_state_id();
  const std::uint32_t current_weapon_state = weapon_state(player);
  if (transaction.observation_initialized &&
      transaction.last_is_shouting == is_shouting &&
      transaction.last_marker_known == marker_known &&
      transaction.last_fx_known == fx_known &&
      transaction.last_camera_state == camera_state &&
      transaction.last_weapon_state == current_weapon_state &&
      transaction.last_phase == transaction.phase) {
    return;
  }

  transaction.observation_initialized = true;
  transaction.last_is_shouting = is_shouting;
  transaction.last_marker_known = marker_known;
  transaction.last_fx_known = fx_known;
  transaction.last_camera_state = camera_state;
  transaction.last_weapon_state = current_weapon_state;
  transaction.last_phase = transaction.phase;
  logger::info(
      "CAST_STATE_CHANGE elapsed_ms={}, phase={}, is_shouting={}, "
      "marker_known={}, fx_known={}, camera={}, camera_state={}, "
      "weapon_state={}, equipped_left={:08X}, equipped_right={:08X}",
      elapsed_milliseconds(transaction), phase_name(transaction.phase),
      is_shouting, marker_known, fx_known, camera_mode(), camera_state,
      current_weapon_state,
      player != nullptr ? form_id(player->GetEquippedObject(true)) : 0,
      player != nullptr ? form_id(player->GetEquippedObject(false)) : 0);
}

binding resolve_activate_binding() {
  const auto* controls = RE::ControlMap::GetSingleton();
  if (controls == nullptr) {
    return {};
  }
  binding result;
  const auto keyboard =
      controls->GetMappedKey("Activate", RE::INPUT_DEVICE::kKeyboard);
  if (keyboard != RE::ControlMap::kInvalid) {
    result.keyboard = keyboard;
  }
  const auto gamepad =
      controls->GetMappedKey("Activate", RE::INPUT_DEVICE::kGamepad);
  if (gamepad != RE::ControlMap::kInvalid && gamepad != 0) {
    result.gamepad = gamepad;
  }
  return result;
}

bool matches_activate(const binding& activate, const RE::InputEvent* event) {
  if (event == nullptr) {
    return false;
  }
  const auto* button = event->AsButtonEvent();
  if (button == nullptr) {
    return false;
  }
  if (event->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
    return activate.keyboard.has_value() &&
           button->GetIDCode() == *activate.keyboard;
  }
  if (event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
    return activate.gamepad.has_value() &&
           button->GetIDCode() == *activate.gamepad;
  }
  return false;
}

RE::NiPointer<RE::TESObjectREFR> crosshair_target() {
  const auto* pick = RE::CrosshairPickData::GetSingleton();
  return pick == nullptr ? nullptr : pick->target.get();
}

std::optional<interaction::lock_tier>
required_tier(const RE::TESObjectREFR& target) {
  const auto level = target.GetLockLevel();
  if (level == RE::LOCK_LEVEL::kRequiresKey ||
      level == RE::LOCK_LEVEL::kUnlocked) {
    return std::nullopt;
  }
  const int value = static_cast<int>(level);
  if (value < static_cast<int>(RE::LOCK_LEVEL::kVeryEasy) ||
      value > static_cast<int>(RE::LOCK_LEVEL::kVeryHard)) {
    return std::nullopt;
  }
  return static_cast<interaction::lock_tier>(value + 1);
}

std::optional<activation> choose_activation() {
  state& current = get_state();
  auto* player = RE::PlayerCharacter::GetSingleton();
  const auto target = crosshair_target();
  if (current.transaction.has_value() || player == nullptr ||
      target == nullptr || target->GetBaseObject() == nullptr ||
      target->GetBaseObject()->As<RE::TESObjectCONT>() == nullptr) {
    return std::nullopt;
  }
  const auto* lock = target->GetLock();
  const auto tier = required_tier(*target);
  if (lock == nullptr || !lock->IsLocked() || !tier.has_value()) {
    return std::nullopt;
  }

  std::array<bool, 5> known{};
  for (std::size_t index = 0; index < current.spells.size(); ++index) {
    known[index] = spell_known(player, current.spells[index]);
  }
  const auto choice = interaction::choose_spell(*tier, known);
  if (!choice.has_value()) {
    return std::nullopt;
  }
  RE::SpellItem* spell = current.spells[choice->index];
  const float cost = (std::max)(0.0F, spell->CalculateMagickaCost(player));
  if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) <
      cost) {
    return std::nullopt;
  }
  return activation{
      .target = target->GetHandle(), .spell = spell, .cost = cost};
}

bool dispatch_magic_unlock(const RE::ObjectRefHandle& handle,
                           RE::PlayerCharacter* player) {
  const auto target = handle.get();
  auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
  if (target == nullptr || vm == nullptr) {
    return false;
  }
  auto* policy = vm->GetObjectHandlePolicy();
  if (policy == nullptr) {
    return false;
  }
  const auto vm_handle = policy->GetHandleForObject(target->GetFormType(),
                                                     target.get());
  RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
  auto* arguments = RE::MakeFunctionArguments(
      static_cast<RE::TESObjectREFR*>(player));
  return vm->DispatchMethodCall(
      vm_handle, RE::BSFixedString("REQ_LockpickControl"),
      RE::BSFixedString("MagicUnlock"), arguments, callback);
}

bool commit_magic_unlock(casting_transaction& transaction,
                         std::string_view trigger) {
  if (transaction.committed) {
    return true;
  }
  state& current = get_state();
  auto* player = RE::PlayerCharacter::GetSingleton();
  const auto target = transaction.selected.target.get();
  if (player == nullptr || target == nullptr || target->GetLock() == nullptr ||
      !target->GetLock()->IsLocked() || transaction.selected.spell == nullptr ||
      !player->HasSpell(transaction.selected.spell)) {
    logger::warn(
        "CAST_COMMIT_REJECTED trigger={}, target={:08X}, target_valid={}, "
        "locked={}, spell={:08X}, known={}",
        trigger, target != nullptr ? target->GetFormID() : 0,
        target != nullptr,
        target != nullptr && target->GetLock() != nullptr &&
            target->GetLock()->IsLocked(),
        form_id(transaction.selected.spell),
        spell_known(player, transaction.selected.spell));
    return false;
  }

  const float magicka = player->AsActorValueOwner()->GetActorValue(
      RE::ActorValue::kMagicka);
  if (magicka < transaction.selected.cost) {
    logger::warn(
        "CAST_COMMIT_REJECTED trigger={}, reason=magicka, have={:.2f}, "
        "need={:.2f}",
        trigger, magicka, transaction.selected.cost);
    if (current.configuration.show_notifications) {
      RE::DebugNotification("Not enough magicka to open this lock");
    }
    return false;
  }

  logger::info("CAST_UNLOCK_DISPATCH begin target={:08X}, player={:08X}",
               target->GetFormID(), player->GetFormID());
  current.unlock_dispatch_in_progress = true;
  const bool dispatch_result =
      dispatch_magic_unlock(transaction.selected.target, player);
  current.unlock_dispatch_in_progress = false;
  logger::info(
      "CAST_UNLOCK_DISPATCH end result={}, menu_opened_during_dispatch={}",
      dispatch_result, transaction.menu_opened_during_dispatch);
  if (!dispatch_result) {
    logger::error(
        "CAST_COMMIT_REJECTED trigger={}, target={:08X}, "
        "reason=REQ_LockpickControl_not_bound",
        trigger, target->GetFormID());
    if (current.configuration.show_notifications) {
      RE::DebugNotification("Arcane Activation could not open this lock");
    }
    return false;
  }

  player->AsActorValueOwner()->RestoreActorValue(
      RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka,
      -transaction.selected.cost);
  transaction.committed = true;
  logger::info(
      "CAST_COMMITTED trigger={}, target={:08X}, spell={:08X}, cost={:.2f}, "
      "magicka_before={:.2f}, animation_released={}",
      trigger, target->GetFormID(), transaction.selected.spell->GetFormID(),
      transaction.selected.cost, magicka, transaction.released_at.has_value());
  if (current.configuration.show_notifications) {
    const char* name = transaction.selected.spell->GetName();
    RE::DebugNotification(name != nullptr && name[0] != '\0'
                              ? name
                              : "Lock opened with Alteration");
  }
  return true;
}

bool remove_owned_spell(RE::PlayerCharacter* player, RE::SpellItem* spell,
                        bool owned, std::string_view label,
                        std::string_view reason) {
  const bool known_before = spell_known(player, spell);
  const bool remove_result =
      !owned || !known_before || (player != nullptr && player->RemoveSpell(spell));
  logger::info(
      "CAST_{}_REMOVE reason={}, owned={}, spell={:08X}, known_before={}, "
      "remove_result={}, known_after={}",
      label, reason, owned, form_id(spell), known_before, remove_result,
      spell_known(player, spell));
  return remove_result;
}

void end_transaction(std::string_view reason) {
  state& current = get_state();
  if (!current.transaction.has_value()) {
    return;
  }
  casting_transaction finished = std::move(*current.transaction);
  current.transaction.reset();
  auto* player = RE::PlayerCharacter::GetSingleton();
  const auto target = finished.selected.target.get();
  const bool marker_known = spell_known(player, current.marker_spell);
  logger::info(
      "CAST_END reason={}, target={:08X}, committed={}, phase={}, "
      "marker_owned={}, marker_known={}, fx_owned={}, fx_known={}, "
      "notify_start={}, notify_release={}, saw_is_shouting={}, "
      "saw_animation_exit={}, animation_events={}, "
      "menu_opened_during_dispatch={}, target_fx_requested={}, "
      "target_fx_applied={}, elapsed_ms={}",
      reason, target != nullptr ? target->GetFormID() : 0, finished.committed,
      phase_name(finished.phase), finished.marker_added, marker_known,
      finished.fx_added, spell_known(player, current.cast_fx_spell),
      finished.notify_start_result, finished.notify_release_result,
      finished.saw_is_shouting, finished.saw_animation_exit,
      finished.animation_event_count, finished.menu_opened_during_dispatch,
      finished.target_fx_requested, finished.target_fx_applied,
      elapsed_milliseconds(finished));

  if (player != nullptr && finished.marker_added && marker_known &&
      graph_bool(player, "IsShouting")) {
    finished.notify_stop_result =
        player->NotifyAnimationGraph(RE::BSFixedString("ShoutStop"));
    logger::info(
        "ARCANE_ANIMATION_STOP reason={}, result={}, is_shouting_after={}",
        reason, finished.notify_stop_result, graph_bool(player, "IsShouting"));
  }
  remove_owned_spell(player, current.cast_fx_spell, finished.fx_added, "FX",
                     reason);
  remove_owned_spell(player, current.marker_spell, finished.marker_added,
                     "MARKER", reason);
  logger::info(
      "CAST_CLEANUP_COMPLETE reason={}, marker_known={}, fx_known={}, "
      "is_shouting={}, notify_stop={}",
      reason, spell_known(player, current.marker_spell),
      spell_known(player, current.cast_fx_spell),
      graph_bool(player, "IsShouting"), finished.notify_stop_result);
}

bool add_owned_spell(RE::PlayerCharacter* player, RE::SpellItem* spell,
                     bool& owned, std::string_view label) {
  const bool known_before = spell_known(player, spell);
  const bool add_result = player != nullptr && spell != nullptr &&
                          !known_before && player->AddSpell(spell);
  const bool known_after = spell_known(player, spell);
  owned = !known_before && known_after;
  logger::info(
      "CAST_{}_ADD spell={:08X}, known_before={}, add_result={}, "
      "known_after={}, transaction_owned={}",
      label, form_id(spell), known_before, add_result, known_after, owned);
  return owned;
}

bool ensure_animation_sink(RE::PlayerCharacter* player);

bool commit_fallback(const activation& selected, std::string_view trigger) {
  state& current = get_state();
  current.transaction = casting_transaction{
      .selected = selected, .began = clock::now()};
  const auto target = selected.target.get();
  logger::warn(
      "CAST_FALLBACK_BEGIN trigger={}, target={:08X}, spell={:08X}, cost={:.2f}",
      trigger, target != nullptr ? target->GetFormID() : 0,
      form_id(selected.spell), selected.cost);
  const bool committed = commit_magic_unlock(*current.transaction, trigger);
  if (current.transaction.has_value()) {
    end_transaction(committed ? "fallback_complete" : "fallback_rejected");
  }
  return committed;
}

bool start_cast(const activation& selected) {
  state& current = get_state();
  auto* player = RE::PlayerCharacter::GetSingleton();
  if (current.transaction.has_value() || player == nullptr) {
    logger::error("CAST_START_REJECTED active={}, player={}",
                  current.transaction.has_value(), player != nullptr);
    return false;
  }
  if (!current.animation_ready) {
    return commit_fallback(selected, "arcane_animation_unavailable");
  }

  const bool is_shouting = graph_bool(player, "IsShouting");
  const bool marker_known = spell_known(player, current.marker_spell);
  const bool fx_known = spell_known(player, current.cast_fx_spell);
  if (is_shouting || marker_known || fx_known) {
    logger::warn(
        "CAST_START_REJECTED reason=animation_busy, is_shouting={}, "
        "marker_known={}, fx_known={}",
        is_shouting, marker_known, fx_known);
    return false;
  }
  if (!ensure_animation_sink(player)) {
    logger::warn(
        "CAST_EVENT_SINK_UNAVAILABLE; graph polling and timeout logging remain "
        "active");
  }

  current.transaction = casting_transaction{
      .selected = selected, .began = clock::now()};
  auto& transaction = *current.transaction;
  const auto target = selected.target.get();
  const auto timing_schedule =
      timing::make_schedule(current.configuration.charge_duration_ms);
  logger::info(
      "CAST_BEGIN target={:08X}, unlock_spell={:08X}, cost={:.2f}, "
      "camera={}, camera_state={}, weapon_state={}, marker={:08X}, "
      "cast_fx={:08X}, resources_ready={}, fx_ready={}, "
      "start_timeout_ms={}, unlock_delay_ms={}, animation_release_ms={}, "
      "cleanup_ms={}",
      target != nullptr ? target->GetFormID() : 0, form_id(selected.spell),
      selected.cost, camera_mode(), camera_state_id(), weapon_state(player),
      form_id(current.marker_spell), form_id(current.cast_fx_spell),
      current.animation_resources_ready, current.fx_ready,
      animation_start_timeout.count(), timing_schedule.unlock_after_ms,
      timing_schedule.animation_release_after_ms,
      post_release_cleanup.count());

  if (!add_owned_spell(player, current.marker_spell, transaction.marker_added,
                       "MARKER")) {
    logger::error(
        "CAST_MARKER_REJECTED; using captured-target fallback without animation");
    const bool committed = commit_magic_unlock(transaction, "marker_add_failed");
    if (current.transaction.has_value()) {
      end_transaction(committed ? "marker_add_fallback_complete"
                                : "marker_add_fallback_rejected");
    }
    return committed;
  }
  if (current.fx_ready) {
    add_owned_spell(player, current.cast_fx_spell, transaction.fx_added, "FX");
  } else {
    logger::warn("CAST_FX_UNAVAILABLE spell={:08X}",
                 form_id(current.cast_fx_spell));
  }

  const bool combat_fallback = uses_combat_visual_fallback(player);
  const bool target_loaded = target != nullptr && target->Is3DLoaded();
  if (combat_fallback && current.fx_ready && target_loaded) {
    transaction.target_fx_requested = true;
    transaction.target_fx_applied =
        target->ApplyArtObject(current.cast_fx_art, target_fx_duration_seconds,
                               player, true) != nullptr;
  }
  logger::info(
      "CAST_VISUAL_ROUTE route={}, weapon_state={}, hand_fx_added={}, "
      "target={:08X}, target_3d_loaded={}, target_fx_requested={}, "
      "target_fx_applied={}, art={:08X}, art_mesh_available={}, duration_s={}",
      combat_fallback ? "hand_plus_target_fallback" : "hand",
      weapon_state(player), transaction.fx_added,
      target != nullptr ? target->GetFormID() : 0, target_loaded,
      transaction.target_fx_requested, transaction.target_fx_applied,
      form_id(current.cast_fx_art), current.fx_resource_ready,
      target_fx_duration_seconds);

  transaction.notify_start_result =
      player->NotifyAnimationGraph(RE::BSFixedString("ShoutStart"));
  const bool shouting_after = graph_bool(player, "IsShouting");
  if (shouting_after) {
    transaction.phase = cast_phase::charging;
    transaction.animation_started_at = clock::now();
    transaction.saw_is_shouting = true;
  }
  logger::info(
      "ARCANE_ANIMATION_START event=ShoutStart, result={}, "
      "is_shouting_before={}, is_shouting_after={}, phase={}, "
      "marker_known={}, fx_known={}, camera={}",
      transaction.notify_start_result, is_shouting, shouting_after,
      phase_name(transaction.phase), spell_known(player, current.marker_spell),
      spell_known(player, current.cast_fx_spell), camera_mode());
  log_state_change(transaction, player);
  return true;
}

void finish_with_commit(std::string_view trigger, std::string_view end_reason) {
  state& current = get_state();
  if (!current.transaction.has_value()) {
    return;
  }
  const bool committed = commit_magic_unlock(*current.transaction, trigger);
  if (!current.transaction.has_value()) {
    return;
  }
  if (!committed) {
    end_transaction("commit_rejected");
  } else if (current.transaction->menu_opened_during_dispatch) {
    end_transaction("menu_opened_during_dispatch");
  } else if (!end_reason.empty()) {
    end_transaction(end_reason);
  }
}

bool commit_and_continue_animation(const std::string_view trigger) {
  state& current = get_state();
  if (!current.transaction.has_value()) {
    return false;
  }
  const bool committed = commit_magic_unlock(*current.transaction, trigger);
  if (!current.transaction.has_value()) {
    return false;
  }
  if (!committed) {
    end_transaction("commit_rejected");
    return false;
  }
  if (current.transaction->menu_opened_during_dispatch) {
    end_transaction("menu_opened_during_dispatch");
    return false;
  }
  return true;
}

void advance_transaction() {
  state& current = get_state();
  if (!current.transaction.has_value()) {
    return;
  }
  auto* player = RE::PlayerCharacter::GetSingleton();
  auto* ui = RE::UI::GetSingleton();
  if (player == nullptr) {
    end_transaction("runtime_unavailable");
    return;
  }
  if (ui != nullptr && ui->GameIsPaused()) {
    end_transaction("game_paused");
    return;
  }

  auto& transaction = *current.transaction;
  const auto now = clock::now();
  const auto elapsed = now - transaction.began;
  log_state_change(transaction, player);
  if (transaction.marker_added &&
      !spell_known(player, current.marker_spell)) {
    logger::warn(
        "CAST_MARKER_DISPLACED elapsed_ms={}, phase={}; using captured-target "
        "fallback",
        elapsed_milliseconds(transaction), phase_name(transaction.phase));
    finish_with_commit("marker_displaced", "marker_displaced");
    return;
  }
  if (transaction.fx_added && !spell_known(player, current.cast_fx_spell) &&
      !transaction.fx_loss_logged) {
    transaction.fx_loss_logged = true;
    logger::warn("CAST_FX_DISPLACED elapsed_ms={}, phase={}",
                 elapsed_milliseconds(transaction),
                 phase_name(transaction.phase));
  }

  const bool is_shouting = graph_bool(player, "IsShouting");
  if (transaction.phase == cast_phase::waiting_for_animation) {
    if (is_shouting) {
      transaction.phase = cast_phase::charging;
      transaction.animation_started_at = now;
      transaction.saw_is_shouting = true;
      logger::info(
          "ARCANE_ANIMATION_OBSERVED elapsed_ms={}, notify_start_result={}, "
          "camera={}, camera_state={}",
          elapsed_milliseconds(transaction), transaction.notify_start_result,
          camera_mode(), camera_state_id());
      log_state_change(transaction, player);
      return;
    }
    if (elapsed >= animation_start_timeout) {
      logger::warn(
          "ARCANE_ANIMATION_START_TIMEOUT elapsed_ms={}, notify_result={}, "
          "is_shouting={}, marker_known={}, fx_known={}; using "
          "captured-target fallback",
          elapsed_milliseconds(transaction), transaction.notify_start_result,
          is_shouting, spell_known(player, current.marker_spell),
          spell_known(player, current.cast_fx_spell));
      finish_with_commit("animation_start_timeout",
                         "animation_start_timeout");
    }
    return;
  }

  if (transaction.phase == cast_phase::charging) {
    if (!is_shouting) {
      transaction.saw_animation_exit = true;
      logger::warn(
          "ARCANE_ANIMATION_INTERRUPTED elapsed_ms={}, charge_elapsed_ms={}; "
          "using captured-target fallback",
          elapsed_milliseconds(transaction),
          transaction.animation_started_at.has_value()
              ? std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - *transaction.animation_started_at)
                    .count()
              : -1);
      finish_with_commit("animation_interrupted", "animation_interrupted");
      return;
    }
    const auto charge_elapsed = transaction.animation_started_at.has_value()
                                    ? now - *transaction.animation_started_at
                                    : elapsed;
    const auto timing_schedule =
        timing::make_schedule(current.configuration.charge_duration_ms);
    const auto unlock_after =
        std::chrono::milliseconds(timing_schedule.unlock_after_ms);
    const auto animation_release_after =
        std::chrono::milliseconds(timing_schedule.animation_release_after_ms);

    if (!transaction.committed && charge_elapsed >= unlock_after) {
      logger::info(
          "ARCANE_UNLOCK_DEADLINE elapsed_ms={}, charge_elapsed_ms={}, "
          "unlock_delay_ms={}, animation_release_ms={}",
          elapsed_milliseconds(transaction),
          std::chrono::duration_cast<std::chrono::milliseconds>(charge_elapsed)
              .count(),
          timing_schedule.unlock_after_ms,
          timing_schedule.animation_release_after_ms);
      if (!commit_and_continue_animation("unlock_delay_elapsed")) {
        return;
      }
    }

    if (charge_elapsed >= animation_release_after) {
      transaction.phase = cast_phase::released;
      transaction.released_at = now;
      transaction.notify_release_result = player->NotifyAnimationGraph(
          RE::BSFixedString("MT_BreathExhaleShort"));
      logger::info(
          "ARCANE_ANIMATION_RELEASE event=MT_BreathExhaleShort, result={}, "
          "elapsed_ms={}, charge_elapsed_ms={}, is_shouting_after={}, "
          "marker_known={}, fx_known={}, unlock_committed={}, "
          "animation_release_ms={}",
          transaction.notify_release_result, elapsed_milliseconds(transaction),
          std::chrono::duration_cast<std::chrono::milliseconds>(charge_elapsed)
              .count(),
          graph_bool(player, "IsShouting"),
          spell_known(player, current.marker_spell),
          spell_known(player, current.cast_fx_spell), transaction.committed,
          timing_schedule.animation_release_after_ms);
      if (!transaction.committed) {
        logger::warn(
            "ARCANE_ANIMATION_RELEASE_WITHOUT_COMMIT; applying defensive "
            "captured-target commit");
        static_cast<void>(
            commit_and_continue_animation("arcane_animation_release"));
      }
    }
    return;
  }

  if (transaction.phase == cast_phase::released) {
    if (!is_shouting) {
      transaction.saw_animation_exit = true;
      end_transaction("animation_complete");
      return;
    }
    if (transaction.released_at.has_value() &&
        now - *transaction.released_at >= post_release_cleanup) {
      end_transaction("post_release_cleanup");
      return;
    }
  }
  if (elapsed >= hard_timeout) {
    if (!transaction.committed) {
      finish_with_commit("hard_timeout", "hard_timeout");
    } else {
      end_transaction("hard_timeout");
    }
  }
}

class animation_event_sink final
    : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
public:
  static animation_event_sink& get_singleton() {
    static animation_event_sink instance;
    return instance;
  }

  RE::BSEventNotifyControl ProcessEvent(
      const RE::BSAnimationGraphEvent* event,
      RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
    state& current = get_state();
    if (event == nullptr || !current.transaction.has_value()) {
      return RE::BSEventNotifyControl::kContinue;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    ++current.transaction->animation_event_count;
    logger::info(
        "CAST_ANIMATION_EVENT index={}, elapsed_ms={}, phase={}, tag={}, "
        "payload={}, holder={:08X}, is_shouting={}, marker_known={}, "
        "fx_known={}, camera={}, camera_state={}",
        current.transaction->animation_event_count,
        elapsed_milliseconds(*current.transaction),
        phase_name(current.transaction->phase), event->tag.c_str(),
        event->payload.c_str(), form_id(event->holder),
        graph_bool(player, "IsShouting"),
        spell_known(player, current.marker_spell),
        spell_known(player, current.cast_fx_spell), camera_mode(),
        camera_state_id());
    return RE::BSEventNotifyControl::kContinue;
  }
};

bool ensure_animation_sink(RE::PlayerCharacter* player) {
  state& current = get_state();
  if (current.animation_sink_registered) {
    return true;
  }
  current.animation_sink_registered =
      player != nullptr && player->AddAnimationGraphEventSink(
                               &animation_event_sink::get_singleton());
  logger::info("CAST_EVENT_SINK_REGISTERED result={}, player={:08X}",
               current.animation_sink_registered, form_id(player));
  return current.animation_sink_registered;
}

class menu_event_sink final
    : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
  static menu_event_sink& get_singleton() {
    static menu_event_sink instance;
    return instance;
  }

  RE::BSEventNotifyControl ProcessEvent(
      const RE::MenuOpenCloseEvent* event,
      RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
    state& current = get_state();
    if (event != nullptr && event->opening &&
        current.transaction.has_value()) {
      const std::string_view menu_name = event->menuName.c_str();
      if (menu_name == "LootMenu") {
        logger::info(
            "CAST_MENU_PRESERVED menu={}, committed={}, phase={}, "
            "elapsed_ms={}",
            event->menuName.c_str(), current.transaction->committed,
            phase_name(current.transaction->phase),
            elapsed_milliseconds(*current.transaction));
      } else if (current.unlock_dispatch_in_progress) {
        current.transaction->menu_opened_during_dispatch = true;
        logger::info("CAST_MENU_DEFERRED menu={}, reason=unlock_dispatch_active",
                     event->menuName.c_str());
      } else {
        logger::info("CAST_MENU_INTERRUPT menu={}", event->menuName.c_str());
        end_transaction("menu_opened");
      }
    }
    return RE::BSEventNotifyControl::kContinue;
  }
};

bool consume_activate(RE::InputEvent* event) {
  state& current = get_state();
  if (!matches_activate(current.activate, event)) {
    return false;
  }
  auto* ui = RE::UI::GetSingleton();
  if (ui != nullptr && ui->GameIsPaused()) {
    return false;
  }
  auto* button = event->AsButtonEvent();
  if (button == nullptr) {
    return false;
  }
  if (current.suppressed.has_value()) {
    if (event->GetDevice() != current.suppressed->device ||
        button->GetIDCode() != current.suppressed->button) {
      return false;
    }
    if (button->IsUp()) {
      current.suppressed.reset();
      logger::debug("ACTIVATE_SUPPRESSION_CLEARED");
    }
    return true;
  }
  if (!button->IsDown()) {
    return false;
  }
  if (current.transaction.has_value()) {
    current.suppressed = suppressed_press{
        .device = event->GetDevice(), .button = button->GetIDCode()};
    logger::info("ACTIVATE_SUPPRESSED reason=cast_in_progress");
    return true;
  }
  const auto selected = choose_activation();
  if (!selected.has_value()) {
    return false;
  }
  current.suppressed = suppressed_press{
      .device = event->GetDevice(), .button = button->GetIDCode()};
  if (!start_cast(*selected)) {
    logger::warn("ACTIVATE_SUPPRESSED reason=cast_start_failed");
    if (current.configuration.show_notifications) {
      RE::DebugNotification("Arcane Activation cannot cast right now");
    }
  }
  return true;
}

class input_dispatch_hook {
public:
  static bool install() {
    SKSE::AllocTrampoline(14);
    REL::Relocation<std::uintptr_t> target{REL::RelocationID(67315, 68617)};
    original_ = SKSE::GetTrampoline().write_call<5>(target.address() + 0x7B,
                                                    &dispatch);
    return original_.address() != 0;
  }

private:
  static void dispatch(RE::BSTEventSource<RE::InputEvent*>* dispatcher,
                       RE::InputEvent* const* events) {
    advance_transaction();
    if (events == nullptr) {
      original_(dispatcher, events);
      return;
    }
    RE::InputEvent* head = *events;
    RE::InputEvent** next = &head;
    while (*next != nullptr) {
      RE::InputEvent* event = *next;
      if (consume_activate(event)) {
        *next = event->next;
      } else {
        next = &event->next;
      }
    }
    RE::InputEvent* forwarded[] = {head};
    original_(dispatcher, forwarded);
  }

  inline static REL::Relocation<decltype(dispatch)> original_;
};

bool resource_available(std::string_view path) {
  RE::BSResourceNiBinaryStream stream{std::string(path)};
  return stream.good();
}
} // namespace

bool install() {
  state& current = get_state();
  current.configuration = config::load();
  logger::info("ARCANE_CONFIG show_notifications={}, charge_duration_ms={}",
               current.configuration.show_notifications,
               current.configuration.charge_duration_ms);
  current.activate = resolve_activate_binding();
  current.marker_spell =
      RE::TESForm::LookupByEditorID<RE::SpellItem>(marker_spell_editor_id);
  current.cast_fx_spell =
      RE::TESForm::LookupByEditorID<RE::SpellItem>(cast_fx_spell_editor_id);
  current.cast_fx_art =
      RE::TESForm::LookupByEditorID<RE::BGSArtObject>(cast_fx_art_editor_id);
  recover_runtime_state("data_loaded");

  for (std::size_t index = 0; index < spell_editor_ids.size(); ++index) {
    current.spells[index] =
        RE::TESForm::LookupByEditorID<RE::SpellItem>(spell_editor_ids[index]);
    if (current.spells[index] == nullptr) {
      logger::error("required spell not found: {}", spell_editor_ids[index]);
      return false;
    }
  }
  if (!current.activate.keyboard.has_value() &&
      !current.activate.gamepad.has_value()) {
    logger::error("the Activate binding could not be resolved");
    return false;
  }

  current.animation_resources_ready = true;
  for (const std::string_view path : animation_resource_paths) {
    const bool available = resource_available(path);
    current.animation_resources_ready &= available;
    logger::info("ARCANE_RESOURCE_PROBE path={}, available={}", path,
                 available);
  }
  current.animation_ready =
      current.marker_spell != nullptr && current.animation_resources_ready;
  current.fx_resource_ready = resource_available(cast_fx_art_resource_path);
  logger::info("ARCANE_FX_RESOURCE_PROBE path={}, available={}",
               cast_fx_art_resource_path, current.fx_resource_ready);
  current.fx_ready = current.cast_fx_spell != nullptr &&
                     current.cast_fx_art != nullptr &&
                     current.fx_resource_ready;
  logger::info(
      "ARCANE_ANIMATION_INTEGRATION animation_ready={}, fx_ready={}, "
      "resources_ready={}, marker={:08X}, cast_fx={:08X}, fx_art={:08X}, "
      "fx_resource_ready={}, marker_effects={}, fx_effects={}",
      current.animation_ready, current.fx_ready,
      current.animation_resources_ready, form_id(current.marker_spell),
      form_id(current.cast_fx_spell), form_id(current.cast_fx_art),
      current.fx_resource_ready,
      current.marker_spell != nullptr ? current.marker_spell->effects.size() : 0,
      current.cast_fx_spell != nullptr ? current.cast_fx_spell->effects.size()
                                       : 0);
  if (!current.animation_ready) {
    logger::warn(
        "Arcane Activation animation assets or marker are unavailable; "
        "unlocks will use the captured-target fallback");
  }

  auto* ui = RE::UI::GetSingleton();
  if (ui == nullptr) {
    logger::error("the menu event sink could not be installed");
    return false;
  }
  ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(
      &menu_event_sink::get_singleton());
  ensure_animation_sink(RE::PlayerCharacter::GetSingleton());
  const bool installed = input_dispatch_hook::install();
  logger::info(
      "ACTIVATION_READY installed={}, keyboard={}, gamepad={}, "
      "marker={:08X}, cast_fx={:08X}, fx_art={:08X}, animation_ready={}, "
      "fx_ready={}",
      installed,
      current.activate.keyboard.value_or(RE::ControlMap::kInvalid),
      current.activate.gamepad.value_or(RE::ControlMap::kInvalid),
      form_id(current.marker_spell), form_id(current.cast_fx_spell),
      form_id(current.cast_fx_art),
      current.animation_ready, current.fx_ready);
  return installed;
}

void recover_runtime_state(const std::string_view reason) {
  state& current = get_state();
  if (current.transaction.has_value()) {
    const std::string end_reason = std::format("{}_active_transaction", reason);
    end_transaction(end_reason);
  }

  auto* player = RE::PlayerCharacter::GetSingleton();
  const bool marker_before = spell_known(player, current.marker_spell);
  const bool fx_before = spell_known(player, current.cast_fx_spell);
  const RE::FormID equipped_left_before =
      player != nullptr ? form_id(player->GetEquippedObject(true)) : 0;
  const bool marker_removed =
      !marker_before || player->RemoveSpell(current.marker_spell);
  const bool fx_removed = !fx_before || player->RemoveSpell(current.cast_fx_spell);

  current.animation_sink_registered = false;
  const bool sink_registered = ensure_animation_sink(player);
  logger::info(
      "STARTUP_RUNTIME_RECOVERY reason={}, player={}, marker={:08X}, "
      "marker_before={}, marker_removed={}, marker_after={}, cast_fx={:08X}, "
      "fx_before={}, fx_removed={}, fx_after={}, equipped_left_before={:08X}, "
      "equipped_left_after={:08X}, sink_registered={}",
      reason, player != nullptr, form_id(current.marker_spell), marker_before,
      marker_removed, spell_known(player, current.marker_spell),
      form_id(current.cast_fx_spell), fx_before, fx_removed,
      spell_known(player, current.cast_fx_spell), equipped_left_before,
      player != nullptr ? form_id(player->GetEquippedObject(true)) : 0,
      sink_registered);
}

bool show_notifications() { return get_state().configuration.show_notifications; }

void set_show_notifications(const bool enabled) {
  get_state().configuration.show_notifications = enabled;
}

std::uint32_t charge_duration_ms() {
  return get_state().configuration.charge_duration_ms;
}

void set_charge_duration_ms(const std::uint32_t milliseconds) {
  get_state().configuration.charge_duration_ms = milliseconds;
}
} // namespace arcane_activation::activation
