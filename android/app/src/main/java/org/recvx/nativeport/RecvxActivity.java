package org.recvx.nativeport;

import android.content.Intent;
import org.libsdl.app.SDLActivity;

/**
 * Startet die native Runtime (libmain.so, Einstieg SDL_main in native/runtime.cpp).
 * Datenorte loest die Runtime selbst ueber SDL auf (apply_android_locations).
 */
public class RecvxActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        // c++_shared vor den Bibliotheken, die sie brauchen; SDLActivity laedt in dieser Reihenfolge.
        return new String[] { "c++_shared", "SDL2", "main" };
    }

    /**
     * Kommandozeile der Runtime fuer Diagnose- und Pruefstarts, z. B.
     * adb shell am start -n org.recvx.nativeport/.RecvxActivity --esa args --renderer,vulkan
     * Ohne Extra startet die Runtime wie vom Launcher ohne Argumente.
     */
    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();
        String[] args = intent != null ? intent.getStringArrayExtra("args") : null;
        return args != null ? args : new String[0];
    }
}
