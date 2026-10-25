#pragma once
struct SDL_Window;
namespace recvx {
// iOS/tvOS: haengt das SDL-Fenster an die per UIApplicationSceneManifest verbundene UIWindowScene (ios_scene.mm).
void ios_adopt_scene(SDL_Window* window);
}
