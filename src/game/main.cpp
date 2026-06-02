#include <SDL3/SDL_main.h>
#include <exception>
#include <string>
#if _WIN32
#include <windows.h>
#endif

int GameEntry(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  try {
    return GameEntry(argc, argv);
  } catch (std::exception& ex) {
#if _WIN32
    MessageBoxA(NULL, (std::string("Sorry, something went wrong :(\r\n\r\n") + ex.what()).c_str(),
                "Liero", MB_OK | MB_ICONWARNING);
#else
    (void)ex;
#endif
    return 1;
  }
}
