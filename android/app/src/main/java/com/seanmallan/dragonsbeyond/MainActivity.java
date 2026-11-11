package com.seanmallan.dragonsbeyond;

import org.libsdl.app.SDLActivity;

/** Dragons Beyond: the game itself is the Wii U game's code, started by SDL. */
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
