#ifndef __BITTY_CONFIG_HH__
#define __BITTY_CONFIG_HH__

#include <list>
#include <mutex>
#include <nlohmann/json.hpp>

namespace bitty {
class Config;

class ConfigListener {
 public:
  ConfigListener();

  virtual void OnConfigReload() = 0;

  void StopListening();
};

class Config {
  nlohmann::json json_;
  mutable std::mutex mutex_;

  std::list<ConfigListener *> listeners_;

  Config();
  void operator=(const Config &) = delete;
  Config(const Config &) = delete;

 public:
  static Config &Get();
  bool Reload();

  inline void Listen(ConfigListener *listener) {
    std::unique_lock lock{mutex_};
    listeners_.push_back(listener);
  }

  inline void StopListening(ConfigListener *listener) {
    std::unique_lock lock{mutex_};
    listeners_.remove(listener);
  }

  inline std::optional<std::string> FontFamily() const {
    std::unique_lock lock{mutex_};
    if (auto ent = json_.find("font_family");
        ent != json_.end() && ent->is_string())
      return std::optional{ent->template get<std::string>()};
    return std::nullopt;
  }

  inline double FontSize() const {
    std::unique_lock lock{mutex_};
    if (auto ent = json_.find("font_size");
        ent != json_.end() && ent->is_number())
      return ent->template get<double>();
    return 14.0;
  }

  inline double Opacity() const {
    std::unique_lock lock{mutex_};

    double opacity = 0;
    if (auto ent = json_.find("opacity");
        ent != json_.end() && ent->is_number())
      opacity = ent->template get<double>();
    else
      opacity = 1.0;

    return std::clamp(opacity, 0., 1.);
  }

  inline double CalcPixelsPerPt() const { return 96.0 / 72.0; }
};
}  // namespace bitty

#endif /* __BITTY_CONFIG_HH__ */