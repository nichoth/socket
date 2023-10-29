#ifndef SSC_CORE_CONFIG_H
#define SSC_CORE_CONFIG_H

#include <iterator>

// TODO(@jwerle): remove this and any need for it
#ifndef SSC_SETTINGS
#define SSC_SETTINGS ""
#endif

#ifndef SSC_VERSION
#define SSC_VERSION ""
#endif

#ifndef SSC_VERSION_HASH
#define SSC_VERSION_HASH ""
#endif

// TODO(@jwerle): use a better name
#if !defined(WAS_CODESIGNED)
#define WAS_CODESIGNED 0
#endif

// TODO(@jwerle): stop using this and prefer a namespaced macro
#ifndef DEBUG
#define DEBUG 0
#endif

// TODO(@jwerle): stop using this and prefer a namespaced macro
#ifndef HOST
#define HOST "localhost"
#endif

// TODO(@jwerle): stop using this and prefer a namespaced macro
#ifndef PORT
#define PORT 0
#endif

// gaurd in case this file is indirectly included in a C/ObjC file
#if defined(__cplusplus)
#include "types.hh"

namespace SSC {
  /**
   * A container for configuration that can be mutated and queried using
   * `.` syntax. Configuration can be created from an INI source string.
   */
  class Config {
    /**
     * Internal configuration mapping, exposed as a const reference to the
     * caller in `Config::data()`
     */
    Map map;
    public:
      using Iterator = Map::const_iterator;
      using Path = Vector<String>;

      /**
       * The configuration prefix.
       */
      const String prefix;

      /**
       * `Config` class constructors.
       */
      Config () = default;
      Config (const String& source);
      Config (const Map& source);
      Config (const Config& source);
      Config (const String& prefix, const Map& source);
      Config (const String& prefix, const Config& source);

      /**
       * Get a configuration value by name or `.` path.
       *
       * @param key The configuration name or key path
       * @return The value at `key` or an empty string.
       */
      const String get (const String& key, const String& fallback = "") const noexcept;

      /**
       * List values at `key`
       *
       * @param key The key to list values for
       * @return A `Vector<String>` of values for `key`
       */
      const Vector<String> list (const String& key) const noexcept;

      /**
       * Get a configuration value reference by name.
       *
       * @param key The configuration name or key path
       * @return `String&` The reference at `key` or an empty string.
       */
      const String& at (const String& key) const;

      /**
       * Set a configuration `value` by `key`.
       *
       * @param key The configuration name of `value` to set
       * @param value The value of `key` to set
       */
      void set (const String& key, const String& value) noexcept;
      void set (const String& key, const Map& value) noexcept;
      void set (const String& key, bool value) noexcept;
      void set (const String& key, double value) noexcept;
      void set (const String& key, int64_t value) noexcept;

      /**
       * Append a value to a key.
       *
       * @param key The configuration name of `value` to append to
       * @param value The value of `key` to to append
       */
      void append (const String& key, const String& value) noexcept;

      /**
       * Returns `true` if `key` exists in configuration and is not empty.
       *
       * @param key The key to check for existence.
       * @return `true` if it exists, otherwise `false`
       */
      bool contains (const String& key) const noexcept;

      /**
       * Erase a configuration value by `key`.
       *
       * @param key The key to erase a value for. Can be valid input for `query`
       * @return `true` if erased, otherwise `false`
       */
      bool erase (const String& key) noexcept;

      /**
       * Get a const reference to the underlying data map
       * of this configuration.
       *
       * @return `Map&` A reference to the internal data map
       */
      const Map& data () const noexcept;

      /**
       * Get a `Config` instance as a "slice" of this configuration, such as
       * a subsection using `.` syntax or configuration section prefixes. The
       * returned `Config` instance is read-only.
       *
       * @param key The key to filter on
       * @return A `const Config` with section starting or matching `key`
       *
       * @example
       *   const auto config = Config::getUserConfig();
       *   const auto build = config.slice("build");
       *   const auto script = build["script"];
       */
      const Config slice (const String& key) const noexcept;

      /**
       * Query for sections in this `Config` instance. The `query` can contain
       * valid regular expression useful for matching sections, keys, and values.
       *
       * @param query The query to filter sections, keys, and values
       * @return A `const Config` with sections, keys, and values matched by `query`
       *
       * @example
       *   const auto config = Config::getUserConfig();
       *   const auto icons = config.query("*icon=");
       */
      const Config query (const String& query) const noexcept;

      /**
       * Get a vector all configuration keys.
       *
       * @return A `const Vector` of all configuration keys.
       */
      const Vector<String> keys () const noexcept;

      /**
       * Get a configuration value by name or `.` path using `[]` notation.
       *
       * @param key The key to look up a value for.
       * @return The value at `key`
       */
      const String operator [] (const String& key) const;

      /**
       * Get a configuration value reference by name or `.` path
       * using `[]` notation.
       *
       * @param key The key to look up a value for.
       * @return A reference to the value at `key`
       */
      String& operator [] (const String& key);

      /**
       * Copy assignment implementation.
       *
       * @param other The `Config` instance to copy from
       * @return A reference to this `Config` instance
       */
      Config& operator = (const Config& other);

      /**
       * Get the beginning of iterator to the configuration tuples.
       *
       * @return `Config::Iterator`
       */
      const Iterator begin () const noexcept;

      /**
       * Get the end of iterator to the configuration tuples.
       *
       * @return `Config::Iterator`
       */
      const Iterator end () const noexcept;

      /**
       * Get the number of entries in the configuration.
       *
       * @return The number of entires in the configuration.
       */
      const std::size_t size () const noexcept;

      /**
       * Clears all entries in the configuration.
       * @return `true` upon success, otherwise `false`.
       */
      const bool clear () noexcept;

      /**
       * Get a vector of configuration children as "slices".
       *
       * @return Child configuration for this configuration.
       */
      const Vector<Config> children () const noexcept;

      /**
       * Extend this configuration with another configuration.
       *
       * @param config The configuration to extend this configuration with
       * @return A reference to this configuration.
       */
      const Config& extend (const Config& config) noexcept;

      /**
       * Extend this configuration with another configuration map.
       *
       * @param config The configuration map to extend this configuration with
       * @return A reference to this configuration.
       */
      const Config& extend (const Map& config) noexcept;
  };

  class UserConfig : public Config {
    public:
      using Config::Config;
  };

  class ExtensionConfig : public Config {
    public:
      class CompilerConfig : public Config {
        public:
          using Config::Config;
          struct Options {
            bool debug = false;
          };

          const Vector<String> flags () const;
          const Vector<String> flags (const String& targetPlatform) const;
          const Vector<String> flags (
            const String& targetPlatform,
            const Options& options
          ) const;
      };

      class LinkerConfig : public Config {
        public:
          using Config::Config;
          const Vector<String> flags (const String& targetPlatform) const;
      };

      class ConfigureConfig : public Config {
        public:
          using Config::Config;
          const String script () const;
      };

      class BuildConfig : public Config {
        public:
          using Config::Config;
          const String script () const;
          const Vector<String> copy () const;
      };

      using Config::Config;
      const CompilerConfig& compiler () const;
      const LinkerConfig& linker () const;
      const ConfigureConfig& configure () const;
      const BuildConfig& build () const;
      const Vector<String> sources () const;
      const String source () const;
      const String path () const;
  };

  // implemented in `init.cc`
  extern const Config& getUserConfig ();
  extern bool isDebugEnabled ();
  extern const char* getDevHost ();
  extern int getDevPort ();
}
#endif

#endif
