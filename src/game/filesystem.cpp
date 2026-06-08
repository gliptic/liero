#include "filesystem.hpp"
#include <SDL3/SDL.h>
#include <sys/stat.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>
#include "io/stream.hpp"
#include "text.hpp"
#if _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

std::string ChangeLeaf(std::string const& path, std::string const& new_leaf) {
  std::size_t const kLastSep = path.find_last_of("\\/");

  if (kLastSep == std::string::npos) return new_leaf;  // We assume there's only a leaf in the path
  return path.substr(0, kLastSep + 1) + new_leaf;
}

std::string GetRoot(std::string const& path) {
  std::size_t const kLastSep = path.find_last_of("\\/");

  if (kLastSep == std::string::npos) return "";
  return path.substr(0, kLastSep);
}

std::string GetLeaf(std::string const& path) {
  std::size_t const kLastSep = path.find_last_of("\\/");

  if (kLastSep == std::string::npos) return path;
  return path.substr(kLastSep + 1);
}

std::string GetBasename(std::string const& path) {
  std::size_t const kLastSep = path.find_last_of('.');

  if (kLastSep == std::string::npos) return path;
  return path.substr(0, kLastSep);
}

std::string GetExtension(std::string const& path) {
  std::size_t const kLastSep = path.find_last_of('.');

  if (kLastSep == std::string::npos) return "";
  return path.substr(kLastSep + 1);
}

std::string ToUpperCase(std::string str) {
  for (char& i : str) {
    i = std::toupper(static_cast<unsigned char>(
        i));  // TODO: Uppercase conversion that works for the DOS charset
  }
  return str;
}

static std::string ToLowerCase(std::string str) {
  for (char& i : str) {
    i = std::tolower(static_cast<unsigned char>(
        i));  // TODO: Lowercase conversion that works for the DOS charset
  }
  return str;
}

FILE* TolerantFOpen(std::string const& name, char const* mode) {
  FILE* f = std::fopen(name.c_str(), mode);
  if (f) return f;

  f = std::fopen(ToUpperCase(name).c_str(), mode);
  if (f) return f;

  f = std::fopen(ToLowerCase(name).c_str(), mode);
  if (f) return f;

  // Try with first letter capital
  std::string ch(ToLowerCase(name));
  ch[0] = std::toupper(static_cast<unsigned char>(ch[0]));
  f = std::fopen(ch.c_str(), mode);
  if (f) return f;

  return nullptr;
}

std::size_t FileLength(FILE* f) {
  long const kOld = ftell(f);
  fseek(f, 0, SEEK_END);  // NOLINT(cert-err33-c) — used by short helpers; size queries on
                          // already-open files are reliable in practice.
  long const kLen = ftell(f);
  fseek(f, kOld, SEEK_SET);  // NOLINT(cert-err33-c) — restoring the original cursor position;
                             // failure is non-fatal.
  return kLen;
}

#if _WIN32
#include "windows.h"

#if defined(__BORLANDC__) || defined(__MWERKS__)
#if defined(__BORLANDC__)
using std::time_t;
#endif
#include "utime.h"
#else
#include "sys/utime.h"
#endif
#else
#include <cerrno>
#include "dirent.h"
#include "fcntl.h"
#include "utime.h"
#endif

namespace {

struct FilenameResult {
  FilenameResult() : name(nullptr) {}

  FilenameResult(char const* name) : name(name) {}

  operator void const*() const { return name; }

  char const* name;
};

#if __unix__ || __APPLE__

#define BOOST_HANDLE DIR*
#define BOOST_INVALID_HANDLE_VALUE 0
#define BOOST_SYSTEM_DIRECTORY_TYPE struct dirent*

// DirIter{Open,Close,Next} wrap the POSIX/Win32 directory iteration APIs
// behind a uniform interface. Local names are deliberately distinct from
// the Windows FindFirstFile/FindNextFile/FindClose macros (which the
// Windows headers #define to the A/W variants) to avoid preprocessor
// substitution inside our own definitions.
inline FilenameResult DirIterOpen(const char* dir, BOOST_HANDLE& handle,
                                  BOOST_SYSTEM_DIRECTORY_TYPE&)
// Returns: 0 if error, otherwise name
{
  const char* dummy_first_name = ".";
  return ((handle = ::opendir(dir)) == BOOST_INVALID_HANDLE_VALUE)
             ? FilenameResult()
             : FilenameResult(dummy_first_name);
}

inline void DirIterClose(BOOST_HANDLE handle) {
  assert(handle != BOOST_INVALID_HANDLE_VALUE);
  ::closedir(handle);
}

inline FilenameResult DirIterNext(BOOST_HANDLE handle, BOOST_SYSTEM_DIRECTORY_TYPE&)
// Returns: if EOF 0, otherwise name
// Throws: if system reports error
{
  //  TODO: use readdir_r() if available, so code is multi-thread safe.
  //  Fly-in-ointment: not all platforms support readdir_r().

  struct dirent* dp = nullptr;
  errno = 0;
  // NOLINTNEXTLINE(bugprone-assignment-in-if-condition) — readdir POSIX idiom: assign-then-check is the documented pattern.
  if ((dp = ::readdir(handle)) == nullptr) {
    if (errno != 0) {
      throw std::runtime_error("Error iterating directory");
    }
    return {};
    // end reached
  }
  return {dp->d_name};
}
#elif _WIN32

#define BOOST_HANDLE HANDLE
#define BOOST_INVALID_HANDLE_VALUE INVALID_HANDLE_VALUE
#define BOOST_SYSTEM_DIRECTORY_TYPE WIN32_FIND_DATAA

inline FilenameResult DirIterOpen(const char* dir, BOOST_HANDLE& handle,
                                  BOOST_SYSTEM_DIRECTORY_TYPE& data) {
  std::string dirpath(std::string(dir) + "/*");
  bool fail = ((handle = ::FindFirstFileA(dirpath.c_str(), &data)) == BOOST_INVALID_HANDLE_VALUE);

  if (fail) return FilenameResult();

  return FilenameResult(data.cFileName);
}

inline void DirIterClose(BOOST_HANDLE handle) {
  assert(handle != BOOST_INVALID_HANDLE_VALUE);
  ::FindClose(handle);
}

inline FilenameResult DirIterNext(BOOST_HANDLE handle, BOOST_SYSTEM_DIRECTORY_TYPE& data) {
  if (::FindNextFileA(handle, &data) == 0) {
    if (::GetLastError() != ERROR_NO_MORE_FILES) {
      throw std::runtime_error("Error iterating directory");
    } else {
      return FilenameResult();
    }
  }

  return FilenameResult(data.cFileName);
}

#else

#error "Not supported"
#endif

}  // namespace

#if _WIN32

inline char IsDirSep(char c) { return c == '\\' || c == '/'; }

static char const kDirSep = '\\';

bool CreateDirectories(std::string const& dir) {
  for (std::size_t i = 0; i < dir.size(); ++i) {
    if (IsDirSep(dir[i])) {
      std::string path = dir.substr(0, i);
      DWORD attr = GetFileAttributesA(path.c_str());

      if (attr == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(path.c_str(), NULL);
      } else if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
      }
    }
  }

  return true;
}

#else

static char const kDirSep = '/';

static inline char IsDirSep(char c) { return c == kDirSep; }

#include <sys/stat.h>
#include <sys/types.h>

bool CreateDirectories(std::string const& dir) {
  for (std::size_t i = 1; i < dir.size(); ++i) {
    if (IsDirSep(dir[i])) {
      std::string const kPath = dir.substr(0, i);
      struct stat s;
      if (stat(kPath.c_str(), &s) < 0) {
        if (mkdir(kPath.c_str(), 0777) < 0) {
          return false;
        }
      }
    }
  }

  return true;
}

#endif

std::string JoinPath(std::string const& root, std::string const& leaf) {
  if (!root.empty() && root[root.size() - 1] != '\\' && root[root.size() - 1] != '/') {
    return root + '/' + leaf;
  }
  return root + leaf;
}

static inline bool DotOrDotDot(char const* name) {
  return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

DirectoryListing::DirectoryListing(std::string const& dir) {
  char const* dir_path = dir.empty() ? "." : dir.c_str();

  BOOST_HANDLE handle{};
  BOOST_SYSTEM_DIRECTORY_TYPE scratch{};
  FilenameResult name;  // initialization quiets compiler warnings

  if (!dir_path[0])
    handle = BOOST_INVALID_HANDLE_VALUE;
  else
    name = DirIterOpen(dir_path, handle, scratch);  // sets handle

  if (handle != BOOST_INVALID_HANDLE_VALUE) {
    do {
      if (!DotOrDotDot(name.name)) {
        struct stat st;
        if (stat(JoinPath(dir, name.name).c_str(), &st) == 0) {
          subs.emplace_back(name.name, (st.st_mode & S_IFMT) == S_IFDIR);

          if (EndsWith(subs.back().name, ".zip")) {
            auto& back = subs.back();
            back.name.erase(subs.back().name.size() - 4);
            back.is_dir = true;
          }
        }
      }
    } while ((name = DirIterNext(handle, scratch)));

    DirIterClose(handle);
  }

  Sort();
}

DirectoryListing::DirectoryListing(std::vector<NodeName>&& subs_init) : subs(std::move(subs_init)) {
  Sort();
}

DirectoryListing::~DirectoryListing() = default;

void DirectoryListing::Sort() {
  std::ranges::sort(subs, [](NodeName const& a, NodeName const& b) { return a.name < b.name; });
  auto const [first, last] = std::ranges::unique(
      subs, [](NodeName const& a, NodeName const& b) { return a.name == b.name; });
  subs.erase(first, last);
}

// ------------ FsNodeZipFile

struct FsNodeZipFile;

struct FsNodeZipArchive {
  FsNodeZipArchive(std::string const& path);

  mz_zip_archive archive;

  FsNodeZipFile* root;
};

static std::shared_ptr<FsNodeImp> Join(std::shared_ptr<FsNodeImp> a, std::shared_ptr<FsNodeImp> b);

struct FsNodeJoin : FsNodeImp {
  std::shared_ptr<FsNodeImp> a, b;

  FsNodeJoin(std::shared_ptr<FsNodeImp> a_init, std::shared_ptr<FsNodeImp> b_init)
      : a(std::move(a_init)), b(std::move(b_init)) {}

  std::string const& FullPath() override { return a->FullPath(); }

  DirectoryListing Iter() override { return a->Iter() | b->Iter(); }

  std::shared_ptr<FsNodeImp> Go(std::string const& name) override {
    return Join(a->Go(name), b->Go(name));
  }

  bool Exists() const override { return a->Exists() || b->Exists(); }

  std::unique_ptr<io::Reader> TryToReader() override {
    auto s = a->TryToReader();
    if (s) return s;
    return b->TryToReader();
  }

  std::unique_ptr<io::Writer> TryToWriter() override {
    auto s = a->TryToWriter();
    if (s) return s;
    return b->TryToWriter();
  }
};

std::shared_ptr<FsNodeImp> Join(std::shared_ptr<FsNodeImp> a, std::shared_ptr<FsNodeImp> b) {
  if (!b) return a;
  if (!a) return b;
  return std::shared_ptr<FsNodeImp>(new FsNodeJoin(std::move(a), std::move(b)));
}

struct FsNodeZipFile : FsNodeImp {
  std::shared_ptr<FsNodeZipArchive> archive;
  std::string path;
  std::string rel_path;
  int file_index;
  bool is_dir;

  FsNodeZipFile(std::string const& path, bool is_dir)
      : archive(new FsNodeZipArchive(path)), path(path), file_index(-1), is_dir(is_dir) {
    archive->root = this;

    auto file_count = mz_zip_reader_get_num_files(&archive->archive);

    for (mz_uint file_index = 0; file_index < file_count; ++file_index) {
      char buf[260];
      mz_zip_reader_get_filename(&archive->archive, file_index, buf, 260);

      bool const kIsDir = mz_zip_reader_is_file_a_directory(&archive->archive, file_index);

      std::string filepath(buf);

      FsNodeZipFile* cur = this;

      std::size_t beg = 0;
      std::size_t i = 0;

      for (; i < filepath.size(); ++i) {
        if (IsDirSep(filepath[i])) {
          std::string const& part = filepath.substr(beg, i - beg);

          auto& c = cur->children[part];
          if (!c)
            c = std::make_shared<FsNodeZipFile>(archive, JoinPath(cur->path, part),
                                                JoinPath(cur->rel_path, part), -1, /*is_dir=*/true);
          cur = c.get();

          beg = i + 1;
        }
      }

      if (beg != i) {
        std::string const& part = filepath.substr(beg, i - beg);

        auto& c = cur->children[part];
        if (!c)
          c = std::make_shared<FsNodeZipFile>(archive, JoinPath(cur->path, part),
                                              JoinPath(cur->rel_path, part),
                                              static_cast<int>(file_index), kIsDir);
        cur = c.get();
      }
    }
  }

  FsNodeZipFile(std::shared_ptr<FsNodeZipArchive> archive, std::string full_path,
                std::string rel_path, int file_index, bool is_dir)
      : archive(std::move(archive)),
        path(std::move(full_path)),
        rel_path(std::move(rel_path)),
        file_index(file_index),
        is_dir(is_dir) {}

  std::map<std::string, std::shared_ptr<FsNodeZipFile>> children;

  std::string const& FullPath() override { return path; }

  DirectoryListing Iter() override {
    std::vector<NodeName> subs;

    subs.reserve(children.size());
    for (auto& i : children) {
      subs.emplace_back(i.first, i.second->is_dir);
    }

    return {std::move(subs)};
  }

  std::shared_ptr<FsNodeImp> Go(std::string const& name) override {
    auto i = children.find(name);
    if (i != children.end()) return i->second;
    return {};
  }

  bool Exists() const override { return true; }

  std::unique_ptr<io::Reader> TryToReader() override {
    if (file_index < 0) return nullptr;

    std::size_t size = 0;
    auto* ptr = mz_zip_reader_extract_to_heap(&archive->archive, static_cast<mz_uint>(file_index),
                                              &size, 0);

    if (!ptr) return nullptr;

    // Copy the heap-extracted bytes into a vector so we own the lifetime
    // (mz_zip_reader_extract_to_heap requires the caller to free()).
    std::vector<uint8_t> data(reinterpret_cast<uint8_t const*>(ptr),
                              reinterpret_cast<uint8_t const*>(ptr) + size);
    free(ptr);  // NOLINT(cppcoreguidelines-no-malloc, hicpp-no-malloc) — miniz hands ownership back
                // via plain C `free`; we cannot use RAII at the API boundary.

    struct OwnedMemReader : io::Reader {
      std::vector<uint8_t> data;
      io::MemReader inner;
      explicit OwnedMemReader(std::vector<uint8_t>&& d)
          : data(std::move(d)), inner(data.data(), data.size()) {}
      uint8_t Get() override { return inner.Get(); }
      std::size_t TryGet(uint8_t* dst, std::size_t n) override { return inner.TryGet(dst, n); }
    };
    return std::make_unique<OwnedMemReader>(std::move(data));
  }

  std::unique_ptr<io::Writer> TryToWriter() override {
    return nullptr;  // We don't want to write to .zip files
  }
};

struct FsNodeFilesystem : FsNodeImp {
  FsNodeFilesystem(std::string path_init) : path(std::move(path_init)) {
    if (path.empty()) path.assign(".");
  }

  std::string const& FullPath() override { return path; }

  DirectoryListing Iter() override { return DirectoryListing{path}; }

  std::shared_ptr<FsNodeImp> Go(std::string const& name) override {
    std::string const kFullPath(JoinPath(path, name));
    std::shared_ptr<FsNodeImp> imp;

    // struct stat st;
    // if (stat(fullPath.c_str(), &st) == 0)
    {
      // if ((st.st_mode & S_IFMT) == S_IFDIR)
      //	dir = true;

      imp = std::make_shared<FsNodeFilesystem>(kFullPath);
    }

    std::string const kZipPath(kFullPath + ".zip");

    if (access(kZipPath.c_str(), 0) != -1) {
      // We have a zip file, merge nodes
      imp = Join(std::move(imp),
                 std::shared_ptr<FsNodeImp>(new FsNodeZipFile(kZipPath, /*is_dir=*/true)));
    } else if (access(kFullPath.c_str(), 0) != -1 && EndsWith(kFullPath, ".zip")) {
      imp = Join(std::move(imp),
                 std::shared_ptr<FsNodeImp>(new FsNodeZipFile(kFullPath, /*is_dir=*/true)));
    }

    return imp;
  }

  bool Exists() const override { return access(path.c_str(), 0) != -1; }

  std::unique_ptr<io::Reader> TryToReader() override {
    FILE* f = TolerantFOpen(path, "rb");
    if (!f) return nullptr;

    return std::make_unique<io::FileReader>(f, io::FileReader::OwnFile{});
  }

  std::unique_ptr<io::Writer> TryToWriter() override {
    FILE* f = fopen(path.c_str(), "wbe");
    if (!f) {
      // Try to create directories
      CreateDirectories(path);
      f = fopen(path.c_str(), "wbe");
      if (!f) return nullptr;
    }

    return std::make_unique<io::FileWriter>(f, io::FileWriter::OwnFile{});
  }

  std::string path;
};

FsNodeZipArchive::FsNodeZipArchive(std::string const& path) {
  memset(&archive, 0, sizeof(archive));
  mz_zip_reader_init_file(&archive, path.c_str(), 0);
}

// TODO: Free archive

FsNode::FsNode(std::string const& path) {
  if (path.empty()) {
    imp = std::make_shared<FsNodeFilesystem>(path);
    return;
  }

  std::size_t beg = 0;
  std::size_t i = 0;

  for (; i < path.size(); ++i) {
    if (IsDirSep(path[i])) {
      std::string const& part = path.substr(beg, i - beg);
      if (!imp) {
#if _WIN32
        if (part.size() == 2 && part[1] == ':')
          imp.reset(new FsNodeFilesystem(part));
        else {
          imp.reset(new FsNodeFilesystem("."));
          imp = imp->Go(part);
        }
#else
        if (part.empty()) {
          imp = std::make_shared<FsNodeFilesystem>("/");
        } else {
          imp = std::make_shared<FsNodeFilesystem>(".");
          imp = imp->Go(part);
        }
#endif
      } else {
        imp = imp->Go(part);
      }

      beg = i + 1;
    }
  }

  if (beg != i) {
    std::string const& part = path.substr(beg, i - beg);

    if (!imp) {
#if _WIN32
      if (path.size() == 2 && path[1] == ':')
        imp.reset(new FsNodeFilesystem(part));
      else {
        imp.reset(new FsNodeFilesystem("."));
        imp = imp->Go(part);
      }
#else
      if (path.empty()) {
        imp = std::make_shared<FsNodeFilesystem>(part);
      } else {
        imp = std::make_shared<FsNodeFilesystem>(".");
        imp = imp->Go(part);
      }
#endif
    } else {
      imp = imp->Go(part);
    }
  }
}

namespace paths {

FsNode UserDataRoot() {
  // Test-only override. Not documented for end users — production paths
  // should go through --config-root or portable.txt instead.
  if (const char* env = std::getenv("OPENLIERO_TEST_USER_DIR")) {
    if (env[0] != '\0') {
      CreateDirectories(env);
      return FsNode(std::string(env));
    }
  }

  char* p = SDL_GetPrefPath("openliero", "openliero");
  if (!p) return {};
  std::string const kPath(p);
  SDL_free(p);

  CreateDirectories(kPath);

  return FsNode(kPath);
}

FsNode SystemDataRoot() {
  // Runtime override: respected by Flatpak builds, packagers who can't
  // recompile, and the test suite.
  if (const char* env = std::getenv("OPENLIERO_DATADIR")) {
    if (env[0] != '\0') {
      FsNode candidate{std::string(env)};
      if (candidate.Exists()) return candidate;
    }
  }

#ifdef OPENLIERO_DATADIR
  {
    FsNode candidate{std::string(OPENLIERO_DATADIR)};
    if (candidate.Exists()) return candidate;
  }
#endif

  // SDL3: SDL_GetBasePath returns a const char* owned by SDL; do NOT free.
  if (const char* base = SDL_GetBasePath()) {
    FsNode candidate{std::string(base)};
    if (candidate.Exists()) return candidate;
  }

  return {};
}

// True if a real, usable portable.txt sits at `path`. We can't use the
// generic FsNode::exists() (which calls _access) because on Windows it
// reports cloud-sync placeholders as present even after the user has
// deleted the file in Explorer — leaving anyone with a sync-backed
// extraction folder permanently stuck in portable mode.
static bool PortableMarkerPresent(std::string const& path) {
#if _WIN32
  DWORD attr = GetFileAttributesA(path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES) return false;
  if (attr & FILE_ATTRIBUTE_DIRECTORY) return false;
  constexpr DWORD kCloudPlaceholder = FILE_ATTRIBUTE_OFFLINE |
                                      0x00400000u     // FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
                                      | 0x00040000u;  // FILE_ATTRIBUTE_RECALL_ON_OPEN
  return !(attr & kCloudPlaceholder);
#else
  struct stat st;
  if (stat(path.c_str(), &st) != 0) return false;
  return S_ISREG(st.st_mode);
#endif
}

bool ShadowsSystem(FsNode const& user_root, std::string const& subdir, std::string const& leaf) {
  // Auto-managed filenames the game writes itself. Picking these in a
  // Save As dialog would clobber the game's own auto-write target or
  // shadow the shipped default unreachably.
  static char const* const kReserved[][2] = {
      {"Setups", "liero.cfg"},
  };
  for (auto const& r : kReserved) {
    if (subdir == r[0] && CiCompare(leaf, r[1])) return true;
  }

  FsNode const kSys = SystemDataRoot();
  if (!kSys.imp) return false;

  // Single-dir layouts (portable.txt / --config-root pointing at the
  // install dir): user writes go to the same directory the system
  // data is read from. There's no separate layer to shadow, and the
  // user must be able to overwrite their own files.
  if (user_root.imp && user_root.imp->FullPath() == kSys.imp->FullPath()) return false;

  return (kSys / subdir / leaf).Exists();
}

// Safe to call before SDL_Init: SDL3's SDL_GetPrefPath/SDL_GetBasePath
// do not require subsystem initialization.
ResolvedPaths Resolve(int argc, char* argv[], std::string const& base_path) {
  ResolvedPaths r;
  r.port = 0;

  std::string config_root;

  // Matches "--<name>" or "--<name>=value". Returns the value (after '=' or
  // the next argv) and advances `i` past it. Refuses to consume the next
  // argv as a value if it looks like another flag, so a typo like
  // "--config-root --port 1234" doesn't silently set configRoot="--port".
  auto match_opt = [&](int& i, char const* name, std::string* out) -> bool {
    char const* arg = argv[i] + 2;
    std::size_t const kNlen = std::strlen(name);
    if (std::strncmp(arg, name, kNlen) != 0) return false;
    if (arg[kNlen] == '=') {
      *out = arg + kNlen + 1;
      return true;
    }
    if (arg[kNlen] != '\0') return false;
    if (i + 1 >= argc || argv[i + 1][0] == '-') return false;
    ++i;
    *out = argv[i];
    return true;
  };

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      std::string value;
      if (match_opt(i, "config-root", &value))
        config_root = std::move(value);
      else if (match_opt(i, "port", &value))
        // NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion, cert-err34-c) — malformed --port falls back to the default; treating it as an error here would be hostile.
        r.port = static_cast<uint16_t>(std::atoi(value.c_str()));
    } else if (argv[i][0] != '-') {
      r.positional_args.emplace_back(argv[i]);
    }
  }

  if (!config_root.empty()) {
    // Explicit single-directory override (Emscripten, CI, power users).
    r.config_node = FsNode(config_root);
    r.user_config_node = FsNode(config_root);
    return r;
  }

  // Determine basePath: caller-supplied (tests) or SDL_GetBasePath().
  std::string base = base_path;
  if (base.empty()) {
    if (const char* p = SDL_GetBasePath()) base = p;
  }

  // portable.txt next to the binary selects single-directory mode.
  if (!base.empty() && PortableMarkerPresent((FsNode(base) / "portable.txt").FullPath())) {
    r.config_node = FsNode(base);
    r.user_config_node = FsNode(base);
    return r;
  }

  // XDG split: user dir for writes; merged (user + system) for reads.
  FsNode const kUser = UserDataRoot();
  FsNode const kSystem = SystemDataRoot();

  r.user_config_node = kUser;
  if (kSystem.imp)
    r.config_node = FsNode(Join(kUser.imp, kSystem.imp));
  else
    r.config_node = kUser;

  return r;
}

}  // namespace paths
