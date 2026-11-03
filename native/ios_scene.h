#pragma once
#include <string>
struct SDL_Window;
namespace recvx {
// iOS/tvOS: haengt das SDL-Fenster an die per UIApplicationSceneManifest verbundene UIWindowScene (ios_scene.mm).
void ios_adopt_scene(SDL_Window* window);
// iOS: nimmt einen Ordner (Documents/data, mehrere GB eigene Spieldaten) aus dem iCloud-/Geraetebackup aus
// (NSURLIsExcludedFromBackupKey). false mit Begruendung, wenn das System den Schluessel nicht setzt.
bool ios_exclude_from_backup(const char* path,std::string& reason);
// iOS/tvOS: legt die SDL-Ansicht auf die Grenzen ihres UIWindow, wenn SDL sie anders gesetzt hat (true = korrigiert).
// SDL 2.32 (UIKit_ComputeViewFrame) tauscht im Vollbild Breite und Hoehe, sobald statusBarOrientation nicht
// Querformat meldet; unter dem UIScene-Lebenszyklus meldet es das nicht, die Ansicht wird hochkant und das Bild
// landet verkleinert unten links. Nach Vollbildwechsel und Rueckkehr aus dem Hintergrund setzt SDL das erneut.
bool ios_fit_view(SDL_Window* window);
}
