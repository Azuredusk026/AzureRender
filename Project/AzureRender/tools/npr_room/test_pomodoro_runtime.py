"""Run the room controller's deterministic cases with the selected Lua binary."""

import argparse
import ctypes
from pathlib import Path


def run(library, scripts):
    lua = ctypes.CDLL(str(library.resolve()))
    pointer = ctypes.c_void_p
    lua.luaL_newstate.restype = pointer
    lua.luaL_openlibs.argtypes = [pointer]
    lua.luaL_openlibs.restype = None
    lua.luaL_loadbufferx.argtypes = [pointer, ctypes.c_char_p, ctypes.c_size_t,
                                   ctypes.c_char_p, ctypes.c_char_p]
    lua.lua_pcallk.argtypes = [pointer, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                              ctypes.c_ssize_t, pointer]
    lua.lua_tolstring.argtypes = [pointer, ctypes.c_int, pointer]
    lua.lua_tolstring.restype = ctypes.c_char_p
    lua.lua_close.argtypes = [pointer]
    lua.lua_close.restype = None

    state = lua.luaL_newstate()
    if not state:
        raise MemoryError("Could not create a Lua state")
    try:
        lua.luaL_openlibs(state)
        for script in scripts:
            chunk = script.read_bytes()
            label = ("@" + str(script)).encode("utf-8")
            if (lua.luaL_loadbufferx(state, chunk, len(chunk), label, b"t")
                    or lua.lua_pcallk(state, 0, 0, 0, 0, None)):
                raise RuntimeError(lua.lua_tolstring(state, -1, None).decode("utf-8", "replace"))
    finally:
        lua.lua_close(state)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lua", type=Path, required=True)
    args = parser.parse_args()
    example = Path(__file__).resolve().parents[2] / "examples/npr-room"
    run(args.lua, [example / "assets/pomodoro.lua", example / "tests/pomodoro.lua"])
    run(args.lua, [example / "assets/startup-flow.lua", example / "tests/startup-flow.lua"])
    run(args.lua, [example / "assets/notes.lua", example / "tests/notes.lua"])
    print("Timer state, cues, room startup and notes checks passed")


if __name__ == "__main__":
    main()
