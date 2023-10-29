#include "config.hh"
#include "debug.hh"
#include "ini.hh"
#include "platform.hh"
#include "string.hh"

namespace SSC {
  static constexpr char NAMESPACE_SEPARATOR = '.';
  static const String NAMESPACE_SEPARATOR_STRING = String(1, NAMESPACE_SEPARATOR);

  Config::Config (const String& source) {
    this->map = INI::parse(source, NAMESPACE_SEPARATOR_STRING);
  }

  Config::Config (const Config& source) : prefix(source.prefix) {
    this->map = source.data();
  }

  Config::Config (const Map& source) {
    this->map = source;
  }

  Config::Config (const String& prefix, const Map& source) : prefix(prefix) {
    this->map = source;
  }

  Config::Config (const String& prefix, const Config& source) : prefix(prefix) {
    this->map = source.data();
  }

  const String Config::get (const String& key, const String& fallback) const noexcept {
    if (this->contains(key) && this->at(key).size() > 0) {
      return tmpl(this->at(key), this->map);
    }

    return fallback;
  }

  const Vector<String> Config::list (const String& key) const noexcept {
    Vector<String> results;
    for (const auto& item : parseStringList(this->get(key))) {
      results.push_back(trim(item));
    }
    return results;
  }

  const String& Config::at (const String& key) const {
    return this->map.at(key);
  }

  void Config::set (const String& key, const String& value) noexcept {
    this->map.insert_or_assign(key, value);
  }

  void Config::set (const String& key, const Map& value) noexcept {
    for (const auto& tuple : value) {
      this->map.insert_or_assign(key + "." + tuple.first, tuple.second);
    }
  }

  void Config::set (const String& key, bool value) noexcept {
    this->map.insert_or_assign(key, value ? "true" : "false");
  }

  void Config::set (const String& key, double value) noexcept {
    try {
      this->map.insert_or_assign(key, std::to_string(value));
    } catch (Exception& e) {}
  }

  void Config::set (const String& key, int64_t value) noexcept {
    try {
      this->map.insert_or_assign(key, std::to_string(value));
    } catch (Exception& e) {}
  }

  void Config::append (const String& key, const String& value) noexcept {
    this->map[key] += value;
  }

  const std::size_t Config::size () const noexcept {
    return this->map.size();
  }

  bool Config::contains (const String& key) const noexcept {
    if (this->map.contains(key) && this->map.at(key).size() > 0) {
      return true;
    }

    return this->query(key).size() > 0;
  }

  bool Config::erase (const String& key) noexcept {
    if (this->map.contains(key)) {
      this->map.erase(key);
      return true;
    }

    const auto view = this->query(key);
    bool erased = false;

    for (const auto& tuple : view) {
      if (this->map.contains(tuple.first)) {
        this->map.erase(tuple.first);
        erased = true;
      }
    }

    return erased;
  }

  const Map& Config::data () const noexcept {
    return this->map;
  }

  const Config Config::slice (const String& key) const noexcept {
    const auto view = this->query("[" + key + "]");
    Map slice;

    for (const auto& tuple : view) {
      if (
        tuple.first.starts_with(key + NAMESPACE_SEPARATOR) &&
        tuple.second.size() > 0
      ) {
        const auto k = tuple.first.substr(key.size() + 1, tuple.first.size());
        const auto v = tuple.second;
        slice.insert_or_assign(k, v);
      }
    }

    return Config { key, slice };
  }

  const Config Config::query (const String& input) const noexcept {
    struct State {
      Vector<String> paths;
      Vector<String> targets;
      String property;
      String compared;
      String token;
      bool parsingSingleQuote = false;
      bool parsingDoubleQuote = false;
      bool parsingNamespace = false;
      bool parsingProperty = false;
      bool negate = false;
      bool compare = false;
    };

    String query = trim(input);
    State state;
    Map results;

    if (!query.starts_with("[") && !query.starts_with(NAMESPACE_SEPARATOR_STRING)) {
      query = "[" + query + "]";
    }

    if (query.starts_with(NAMESPACE_SEPARATOR_STRING)) {
      query = "[*]" + query;
    }

    for (int i = 0; i < query.size(); ++i) {
      const auto ch = query[i];

      if (ch == '[') {
        if (state.parsingNamespace) {
          return Config {}; // error
        }

        state.parsingNamespace = true;
        state.token = "";
      } else if (ch == ']') {
        if (!state.parsingNamespace) {
          return Config {}; // error
        }

        state.parsingNamespace = false;
        if (state.token.size() == 0) {
          // [] implies [*]
          state.paths.push_back("*");
        } else {
          state.paths.push_back(state.token);
          state.token = "";
        }
      } else if (state.parsingNamespace) {
        state.token += ch;
      } else if (ch == '.') {
        state.parsingProperty = true;
      } else if (state.parsingProperty) {
        if (ch == ' ' && state.token.size() == 0) {
          continue;
        } else if (ch == '"') {
          if (!state.parsingSingleQuote) {
            state.parsingDoubleQuote = state.token.size() == 0;
            continue;
          }
        } else if (ch == '\'') {
          if (!state.parsingDoubleQuote) {
            state.parsingSingleQuote = state.token.size() == 0;
            continue;
          }
        } else if (ch == '!' && !state.parsingDoubleQuote && !state.parsingSingleQuote) {
          if (query[i + 1] == '=') {
            state.negate = true;
            state.compare = true;
            continue;
          } else {
            return Config {}; // error
          }
        } else if (ch == '=' && !state.parsingDoubleQuote && !state.parsingSingleQuote) {
          state.compare = true;
          continue;
        }

        if (state.compare) {
          state.compared += ch;
        } else {
          state.token += ch;
        }
      }
    }

    if (state.parsingProperty && state.token.size()) {
      state.property = trim(state.token);
      state.token = "";
    }

    state.compared = trim(state.compared);

    const auto& path = join(state.paths, NAMESPACE_SEPARATOR_STRING);
    for (const auto& tuple : this->map) {
      const auto parts = split(tuple.first, NAMESPACE_SEPARATOR_STRING);
      const auto& target = tuple.first;
      const auto prefix = join(
        Vector<String>(parts.begin(), parts.begin() + parts.size() - 1),
        NAMESPACE_SEPARATOR_STRING
      );

      bool match = false;
      if (path.starts_with(NAMESPACE_SEPARATOR_STRING)) {
        if (state.compare) {
          match = prefix.ends_with(path);
        } else {
          match = prefix.find(path) != String::npos;
        }
      } else if (path == "*") {
        match = true;
      } else if (prefix.starts_with(path)) {
        match = true;
      }

      if (match) {
        if (state.property == "*") {
          state.targets.push_back(target);
          state.compare = false;
        } else if (state.compare || state.property.size() > 0) {
          state.targets.push_back(prefix);
        } else {
          state.targets.push_back(target);
        }
      }
    }

    for (const auto& target : state.targets) {
      const auto key = state.compare || state.property.size() > 0
        ? target + NAMESPACE_SEPARATOR_STRING + state.property
        : target;

      if (!this->map.contains(key)) {
        continue;
      }

      const auto& value = this->map.at(key);

      if (state.compare) {
        if (state.negate) {
          if (value != state.compared) {
            results[key] = value;
          }
        } else if (value == state.compared) {
          results[key] = value;
        }
      } else {
        results[key] = value;
      }
    }

    return Config { results };
  }

  const Vector<String> Config::keys () const noexcept {
    Vector<String> results;
    for (const auto& tuple : this->map) {
      results.push_back(tuple.first);
    }
    return results;
  }

  const String Config::operator [] (const String& key) const {
    return this->get(key);
  }

  String& Config::operator [] (const String& key) {
    return this->map[key];
  }

  Config& Config::operator = (const Config& other) {
    const_cast<String*>(&this->prefix)->assign(other.prefix);
    this->map = other.data();
    return *this;
  }

  const Config::Iterator Config::begin () const noexcept {
    return this->map.begin();
  }

  const Config::Iterator Config::end () const noexcept {
    return this->map.end();
  }

  const bool Config::clear () noexcept {
    if (this->map.size() == 0) {
      return false;
    }

    this->map.clear();
    return true;
  }

  const Vector<Config> Config::children () const noexcept {
    Vector<Config> children;
    Vector<String> seen;
    for (const auto& tuple : this->map) {
      const auto parts = split(tuple.first, NAMESPACE_SEPARATOR_STRING);
      const auto duplicate = std::find(seen.begin(), seen.end(), parts[0]) != seen.end();
      if (parts.size() > 1 && !duplicate) {
        seen.push_back(parts[0]);
        children.push_back(Config(parts[0], this->slice(parts[0])));
      }
    }
    return children;
  }

  const Config& Config::extend (const Config& config) noexcept {
    for (const auto& tuple : config.data()) {
      this->map.insert_or_assign(tuple.first, tuple.second);
    }
    return *this;
  }

  const Config& Config::extend (const Map& config) noexcept {
    for (const auto& tuple : config) {
      this->map.insert_or_assign(tuple.first, tuple.second);
    }
    return *this;
  }

   const Vector<String> ExtensionConfig::CompilerConfig::flags () const {
     return this->flags(platform.os);
   }

   const Vector<String> ExtensionConfig::CompilerConfig::flags (
    const String& targetPlatform
   ) const {
     static const auto isDebugEnabled = SSC::isDebugEnabled();
     return this->flags(targetPlatform, CompilerConfig::Options {
       .debug = isDebugEnabled
     });
   }

   const Vector<String> ExtensionConfig::CompilerConfig::flags (
    const String& targetPlatform,
    const CompilerConfig::Options& options
   ) const {
     Vector<String> flags = this->list("compiler.flags");

     flags.push_back("-DSOCKET_RUNTIME_EXTENSION");

     if (options.debug) {
       flags = concat(flags, this->list("compiler.debug.flags"));
     }

     if (targetPlatform == "mac") {
     }

     if (targetPlatform == "linux") {
     }

     if (targetPlatform == "win") {
     }

     if (targetPlatform == "android" || targetPlatform == "android-emulator") {
       for (int i = 0; i < flags.size(); ++i) {
         flags[i] = replace(flags[i], "-I", "-I$(LOCAL_PATH");
       }
     }

     if (targetPlatform == "ios" || targetPlatform == "ios-simulator") {
     }

     return flags;
   }

   const String ExtensionConfig::path () const {
     return this->get("path");
   }

   const String ExtensionConfig::source () const {
     return this->get("source");
   }
}
