// On-screen touch gamepad for iOS/iPadOS (not tvOS). Host adaptation, no game logic.
#pragma once
struct SDL_Window;
namespace recvx {
// Puts the touch gamepad over the SDL window and attaches the SDL virtual game controller it feeds
// (touch_pad.h, input.h). The gamepad hides while a real controller is connected. A failure only
// logs: the game runs without touch controls, as before.
void ios_touch_pad_attach(SDL_Window* window);
}
