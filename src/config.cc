#include "config.hh"

#include <pwd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace bitty {
ConfigListener::ConfigListener() { Config::Get().Listen(this); }

void ConfigListener::StopListening() { Config::Get().StopListening(this); }

Config::Config() { Reload(); }

Config &Config::Get() {
  static Config config;
  return config;
}

std::filesystem::path GetConfigDirectory() {
  for (const auto &var : {"HOME", "USERPROFILE"}) {
    if (const char *path = std::getenv(var)) {
      try {
        return std::filesystem::path(path);
      } catch (...) {
      }
    }
  }

  if (const char *home_drive = std::getenv("HOMEDRIVE")) {
    if (const char *home_path = std::getenv("HOMEPATH")) {
      try {
        return std::filesystem::path(home_drive) /
               std::filesystem::path(home_path);
      } catch (...) {
      }
    }
  }

  return std::filesystem::current_path();
}

bool Config::Reload() {
  std::unique_lock lock{mutex_};

  auto path = GetConfigDirectory() / ".bitty.json";

  if (!std::filesystem::exists(path)) return false;

  std::ifstream stream{path};

  try {
    json_ = nlohmann::json::parse(stream);
  } catch (const nlohmann::json::exception &ex) {
    /* TODO */ (void)ex.what();
    json_ = nlohmann::json{};
  }

  for (auto listener : listeners_) listener->OnConfigReload();

  uid_t uid = getuid();

  struct passwd *pw = getpwuid(uid);
  default_shell_ = pw ? pw->pw_shell : "/bin/sh";

  return true;
}
}  // namespace bitty
