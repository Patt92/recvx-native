#pragma once
#include <string>
struct SDL_Window;
namespace recvx {
// iOS/tvOS: haengt das SDL-Fenster an die per UIApplicationSceneManifest verbundene UIWindowScene (ios_scene.mm).
void ios_adopt_scene(SDL_Window* window);
// iOS: nimmt einen Ordner (Documents/data, mehrere GB eigene Spieldaten) aus dem iCloud-/Geraetebackup aus
// (NSURLIsExcludedFromBackupKey). false mit Begruendung, wenn das System den Schluessel nicht setzt.
bool ios_exclude_from_backup(const char* path,std::string& reason);
}
