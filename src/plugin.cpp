#include "pch.h"

#include "activation.h"
#include "plugin_version.h"
#include "runtime_settings.h"

namespace arcane_activation {
namespace {
void initialize_log() {
  std::vector<spdlog::sink_ptr> sinks;
#ifndef NDEBUG
  sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
  auto path = logger::log_directory();
  if (!path.has_value()) {
    SKSE::stl::report_and_fail("ArcaneActivation could not locate the SKSE log directory");
  }
  *path /= std::format("{}.log", plugin_version::name);
  sinks.push_back(
      std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
  auto log = std::make_shared<spdlog::logger>(
      "ArcaneActivation", sinks.begin(), sinks.end());
  log->set_level(spdlog::level::info);
  log->flush_on(spdlog::level::info);
  spdlog::set_default_logger(std::move(log));
  spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
}

void on_skse_message(SKSE::MessagingInterface::Message* message) {
  if (message == nullptr) {
    return;
  }

  if (message->type == SKSE::MessagingInterface::kDataLoaded &&
      !activation::install()) {
    logger::critical("Arcane Activation failed to initialize");
    RE::DebugNotification(
        "Arcane Activation failed to initialize; check ArcaneActivation.log");
  } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
    activation::recover_runtime_state("post_load_game");
  } else if (message->type == SKSE::MessagingInterface::kNewGame) {
    activation::recover_runtime_state("new_game");
  }
}
} // namespace
} // namespace arcane_activation

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
  arcane_activation::initialize_log();
  logger::info("{} v{} loading", arcane_activation::plugin_version::name,
               arcane_activation::plugin_version::version_string);
  if (skse->RuntimeVersion() !=
      arcane_activation::plugin_version::supported_runtime) {
    logger::critical("unsupported Skyrim runtime {}", skse->RuntimeVersion());
    return false;
  }
  SKSE::Init(skse);
  const auto* papyrus = SKSE::GetPapyrusInterface();
  if (papyrus == nullptr ||
      !papyrus->Register(&arcane_activation::runtime_settings::register_papyrus)) {
    logger::critical("Arcane Activation failed to register its MCM functions");
    return false;
  }
  return SKSE::GetMessagingInterface()->RegisterListener(
      arcane_activation::on_skse_message);
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() noexcept {
  SKSE::PluginVersionData version;
  version.PluginName(arcane_activation::plugin_version::name.data());
  version.PluginVersion(arcane_activation::plugin_version::version);
  version.CompatibleVersions(
      {arcane_activation::plugin_version::supported_runtime});
  version.HasNoStructUse();
  return version;
}();

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Query(const SKSE::QueryInterface* skse,
                 SKSE::PluginInfo* plugin_info) {
  plugin_info->name = SKSEPlugin_Version.pluginName;
  plugin_info->infoVersion = SKSE::PluginInfo::kVersion;
  plugin_info->version = SKSEPlugin_Version.pluginVersion;
  return skse->RuntimeVersion() ==
         arcane_activation::plugin_version::supported_runtime;
}
