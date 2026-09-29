import sys
mode, args = sys.argv[1], sys.argv[2:]
if mode == "key":
    for k in args: print(f"sendkey {k}")
    raise SystemExit
plain = {" ": "spc", "\n": "ret", "\t": "tab", "-": "minus", "=": "equal", "[": "bracket_left",
         "]": "bracket_right", "\\": "backslash", ";": "semicolon", "'": "apostrophe",
         "`": "grave_accent", ",": "comma", ".": "dot", "/": "slash"}
shifted = {"!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7", "*": "8",
           "(": "9", ")": "0", "_": "minus", "+": "equal", "{": "bracket_left", "}": "bracket_right",
           "|": "backslash", ":": "semicolon", '"': "apostrophe", "~": "grave_accent",
           "<": "comma", ">": "dot", "?": "slash"}
text = " ".join(args) + ("\n" if mode == "typeln" else "")
for ch in text:
    if ch.isascii() and ch.isalpha():
        print(f"sendkey shift-{ch.lower()}" if ch.isupper() else f"sendkey {ch}")
    elif ch.isdigit(): print(f"sendkey {ch}")
    elif ch in plain: print(f"sendkey {plain[ch]}")
    elif ch in shifted: print(f"sendkey shift-{shifted[ch]}")
    else: sys.exit(f"cannot type {ch!r}")
