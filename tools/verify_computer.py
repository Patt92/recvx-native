#!/usr/bin/env python3
"""Replay the special-event computer terminal (sp_evt.c) in the running game.

Four runs through the original scripts and the native terminal task:
  * password terminal with the correct code  -> ev_flg 0x3d
  * password terminal with a wrong code      -> ev_flg 0x3e
  * security terminal: rejected code, the script asks again, cancel aborts
  * the original test script: both keyboard words, the conditional jump and
    the 3D character window drawn from the room's enemy work

The terminal is requested through the debug entry, which raises the original
bhEvtComSet request (cb_flg 0x400000 with com_num). That is a diagnostic
entry point, not a claim about which room runs which script; the scanner
sequence of RM_0090 is verified from its own room script by
tools/verify_scanner.py.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# Soft keyboard page of SYSTEM/0002: ten columns, four rows. ESC, BS and the
# tall Enter key are not characters.
KEYS = ["1234567890", ".ABCDEFGH.", "IJKLMNOPQ.", "RSTUVWXYZ."]


class Replay:
    """Builds a runtime replay file one 30 Hz frame at a time."""

    def __init__(self, number):
        # Let the room run a few ordinary passes first: the terminal must
        # freeze an object tree that a control pass has actually posed.
        self.lines = ["10 debug_key F2", f"11 debug_select computer {number}", "12 confirm"]
        self.frame = 70
        self.key = (1, 1)

    def wait(self, frames):
        self.frame += frames

    def act(self, action, spacing=3):
        self.lines.append(f"{self.frame} {action}")
        self.frame += spacing

    def confirm_until(self, last, spacing=6):
        while self.frame < last:
            self.act("confirm", spacing)

    def move(self, x, y):
        kx, ky = self.key
        if kx == 9 and ky != y:          # leave the tall Enter column first
            self.act("left")
            kx = 8
        while ky != y:
            self.act("down" if ky < y else "up")
            ky += 1 if ky < y else -1
        while kx != x:
            self.act("right" if kx < x else "left")
            kx += 1 if kx < x else -1
        self.key = (kx, ky)

    def word(self, text):
        for character in text:
            for y, row in enumerate(KEYS):
                if character in row:
                    self.move(row.index(character), y)
                    self.act("confirm")
                    break
            else:
                raise SystemExit(f"{character!r} is not on the original keyboard")
        self.move(9, 2)
        self.act("confirm")

    def write(self, path):
        path.write_text("\n".join(self.lines) + "\n")
        return path


def run(runtime, data, folder, name, room, frames, replay, screenshot=False):
    state = folder / f"{name}.json"
    command = [str(runtime.resolve()), "--data", str(data.resolve()), "--headless", "--in-game",
               "--room", room, "--frames", str(frames), "--replay", str(replay),
               "--state", str(state)]
    if screenshot:
        command += ["--screenshot", str(folder / f"{name}.bmp")]
    with (folder / f"{name}.log").open("w") as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    result = json.loads(state.read_text())
    assert not result["blocked_commands"], (name, "blocked event commands")
    return result


def event_flag(state, index):
    return bool(state["event_flags"][index // 32] & (0x80000000 >> (index % 32)))


def released(state, name):
    """bhExitSpEvtComputer handed the game back to the player."""
    flags = state["work_flags"]
    assert not flags[5] & 0x4000000, (name, "st_flg 0x4000000 still set")
    assert not flags[10] & 0x400000, (name, "cb_flg 0x400000 still set")
    assert flags[6] == 0xFFFFFFFF, (name, "sp_flg was not restored to -1", flags[6])
    assert flags[11] & 0x4 and flags[11] & 0x8000, (name, "gm_flg 0x4 / 0x8000 missing")
    assert flags[12] & 0x20000 and not flags[12] & 0x80, (name, "ts_flg 0x20000 / 0x80 wrong")
    for field in ["dialog_open", "cinematic", "player_scripted", "player_hidden"]:
        assert not state[field], (name, field, "control was not returned")
    terminal = state["computer"]
    assert terminal and not terminal["active"], (name, "the terminal task is still running")
    assert not terminal["unknown_opcodes"], (name, "the interpreter met an undefined opcode")
    assert not terminal["draw_overflow"], (name, "the quad list overflowed")


def password(runtime, data, folder, code, expect_set, expect_clear, name):
    replay = Replay(7)
    replay.frame = 90                    # the 226 wait is reached around pass 68
    replay.act("confirm")
    replay.frame = 220                   # the keyboard window has finished opening
    replay.word(code)
    state = run(runtime, data, folder, name, "RM_0090.RDX", replay.frame + 200,
                replay.write(folder / f"{name}.txt"), screenshot=True)
    terminal = state["computer"]
    assert terminal["words"][0] == code, (name, "the keyboard recorded", terminal["words"])
    assert event_flag(state, expect_set), (name, f"ev_flg {expect_set:#x} was not set")
    assert not event_flag(state, expect_clear), (name, f"ev_flg {expect_clear:#x} was set as well")
    released(state, name)


def retry_and_abort(runtime, data, folder):
    replay = Replay(2)
    replay.confirm_until(310)            # the waits, then eight characters typed
    replay.frame = 330
    replay.move(9, 2)
    replay.act("confirm")                # a rejected ID
    rejected = run(runtime, data, folder, "security-rejected", "RM_0090.RDX", replay.frame + 320,
                   replay.write(folder / "security-rejected.txt"))
    text = [line for line in rejected["computer"]["text"] if line]
    assert any("rejected" in line for line in text), ("security", "no rejection message", text)
    assert not event_flag(rejected, 4), ("security", "a rejected ID opened the lock")

    # The script jumps back to its label, asks again and command 244 unlocks
    # the cancel button there. Cancel then ends the task without a flag.
    replay.confirm_until(replay.frame + 420, spacing=6)
    replay.act("cancel")
    aborted = run(runtime, data, folder, "security-aborted", "RM_0090.RDX", replay.frame + 220,
                  replay.write(folder / "security-aborted.txt"), screenshot=True)
    assert not event_flag(aborted, 4), ("security", "the aborted terminal opened the lock")
    released(aborted, "security-aborted")


def test_script(runtime, data, folder):
    # RM_0020 places a zombie as enemy 0, so command 238 has an actor to draw.
    replay = Replay(0)
    replay.confirm_until(462)
    replay.frame = 510
    replay.word("JHON")
    replay.wait(90)
    replay.word("ASTOR")
    drawn = run(runtime, data, folder, "test-script", "RM_0020.RDX", replay.frame + 90,
                replay.write(folder / "test-script.txt"), screenshot=True)
    terminal = drawn["computer"]
    assert terminal["words"] == ["JHON", "ASTOR"], ("test script", terminal["words"])
    assert terminal["lcflg"] == 3, ("test script", "both comparisons should match", terminal["lcflg"])
    assert terminal["character"] == 0, ("test script", "the character window has no enemy")
    assert not terminal["character_gaps"], ("test script", "the actor owner answered with nothing")
    assert any("Ok!" in line for line in terminal["text"]), ("test script", terminal["text"])
    # The terminal writes mtn_no on the enemy work (command 239), but the
    # object tree is only posed by the control pass, which is off while the
    # terminal owns the screen. Work and tree must disagree here.
    actor = next(e for e in drawn["enemies"] if e["index"] == 0)
    assert actor["motion"] == 31, ("test script", "command 239 did not reach the work", actor)
    assert actor["posed_motion"] != 31, ("test script", "the tree followed the work during the pause", actor)

    replay.confirm_until(replay.frame + 600, spacing=8)
    finished = run(runtime, data, folder, "test-script-end", "RM_0020.RDX", replay.frame + 120,
                   replay.write(folder / "test-script-end.txt"))
    released(finished, "test script")
    # bhExitSpEvtComputer hands the pass back: the tree tracks the work again.
    live = next(e for e in finished["enemies"] if e["index"] == 0)
    assert live["posed_motion"] == live["motion"], ("test script", "the control pass did not resume", live)


def check(runtime, data, folder):
    password(runtime, data, folder, "1971", 0x3D, 0x3E, "password-correct")
    password(runtime, data, folder, "1972", 0x3E, 0x3D, "password-wrong")
    retry_and_abort(runtime, data, folder)
    test_script(runtime, data, folder)
    print("Computer terminal verified: password accepted/rejected, retry and abort, "
          "soft keyboard words, conditional jump, 3D character window and control return.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        check(args.runtime, args.data, args.output)
    else:
        with tempfile.TemporaryDirectory() as folder:
            check(args.runtime, args.data, Path(folder))


if __name__ == "__main__":
    main()
