import os
import sys
import unicodedata

# KEYMAP=us-intl (set in _common.sh) means the guest layout has the dead keys of
# US-International (us-dead-keys.keymap): ' " ` ~ ^ are typed as key + space,
# and accented letters as dead key + letter.
mode, args = sys.argv[1], sys.argv[2:]
if mode == "key":
    for k in args: print(f"sendkey {k}")
    raise SystemExit
intl = os.environ.get("KEYMAP") == "us-intl"
plain = {" ": "spc", "\n": "ret", "\t": "tab", "-": "minus", "=": "equal", "[": "bracket_left",
         "]": "bracket_right", "\\": "backslash", ";": "semicolon", "'": "apostrophe",
         "`": "grave_accent", ",": "comma", ".": "dot", "/": "slash"}
shifted = {"!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7", "*": "8",
           "(": "9", ")": "0", "_": "minus", "+": "equal", "{": "bracket_left", "}": "bracket_right",
           "|": "backslash", ":": "semicolon", '"': "apostrophe", "~": "grave_accent",
           "<": "comma", ">": "dot", "?": "slash"}
dead = {"́": "apostrophe", "̀": "grave_accent", "̂": "shift-6",
        "̃": "shift-grave_accent", "̈": "shift-apostrophe", "̧": "apostrophe"}


def letter(ch):
    return f"sendkey shift-{ch.lower()}" if ch.isupper() else f"sendkey {ch}"


text = " ".join(args) + ("\n" if mode == "typeln" else "")
for ch in text:
    if ch.isascii() and ch.isalpha():
        print(letter(ch))
    elif ch.isdigit(): print(f"sendkey {ch}")
    elif ch in plain or ch in shifted:
        print(f"sendkey {plain[ch]}" if ch in plain else f"sendkey shift-{shifted[ch]}")
        # Haiku drops a dead key that the next key can't accent, so a space
        # always follows one: dead key + space types it alone.
        if intl and ch in "'\"`~^":
            print("sendkey spc")
    elif intl and len(unicodedata.normalize("NFD", ch)) == 2 and unicodedata.normalize("NFD", ch)[1] in dead:
        base, mark = unicodedata.normalize("NFD", ch)
        print(f"sendkey {dead[mark]}")
        print(letter(base))
    else: sys.exit(f"cannot type {ch!r}")
