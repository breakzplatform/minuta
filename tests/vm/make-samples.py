#!/usr/bin/env python3
"""Generates the VM test documents.

usage: make-samples.py typing OUT   ~2,000 words of Markdown to type (ASCII)
       make-samples.py large OUT    a ~1 MB Markdown document (CP1252)
"""
import random
import sys

WORDS = ("minuta editor markdown column window text focus typewriter night theme "
         "paragraph sentence heading emphasis strong code block quote list link "
         "retro machine pentium windows keyboard screen caret scroll write read "
         "quiet simple plain clear short long early late small large warm cold "
         "the a of to and in is it that for on with as at by from this be are").split()


def sentence(rng, words):
    chosen = [rng.choice(WORDS) for _ in range(words)]
    chosen[0] = chosen[0].capitalize()
    return " ".join(chosen) + rng.choice([".", ".", ".", "!", "?"])


def decorate(rng, text):
    words = text.split(" ")
    if len(words) > 6:
        i = rng.randrange(1, len(words) - 3)
        kind = rng.randrange(4)
        if kind == 0:
            words[i] = "*" + words[i] + "*"
        elif kind == 1:
            words[i] = "**" + words[i] + " " + words[i + 1] + "**"
            del words[i + 1]
        elif kind == 2:
            words[i] = "`" + words[i] + "`"
        else:
            words[i] = "[" + words[i] + "](http://example.com/" + words[i] + ")"
    return " ".join(words)


def wrap(text, width=64):
    lines, line = [], ""
    for word in text.split(" "):
        if line and len(line) + 1 + len(word) > width:
            lines.append(line)
            line = word
        else:
            line = word if not line else line + " " + word
    lines.append(line)
    return lines


def typing(out):
    rng = random.Random(98)
    lines = ["# Typing test", ""]
    words = 2
    section = 1
    while words < 2000:
        if words > section * 400:
            section += 1
            lines += ["## Section %d" % section, ""]
            words += 2
        kind = rng.randrange(6)
        if kind == 0:
            for _ in range(3):
                item = sentence(rng, rng.randrange(4, 9))
                lines.append("- " + decorate(rng, item))
                words += len(item.split())
            lines.append("")
        elif kind == 1:
            quote = sentence(rng, rng.randrange(8, 14))
            lines += ["> " + l for l in wrap(decorate(rng, quote), 60)] + [""]
            words += len(quote.split())
        else:
            paragraph = " ".join(decorate(rng, sentence(rng, rng.randrange(6, 14)))
                                 for _ in range(rng.randrange(2, 5)))
            lines += wrap(paragraph) + [""]
            words += len(paragraph.split())
    text = "\n".join(lines).rstrip("\n") + "\n"
    with open(out, "w", newline="") as f:
        f.write(text)


def large(out):
    rng = random.Random(1998)
    parts = []
    size = 0
    chapter = 0
    while size < 1024 * 1024:
        chapter += 1
        block = ["# Cap\xedtulo %d" % chapter, ""]
        for _ in range(rng.randrange(4, 9)):
            kind = rng.randrange(7)
            if kind == 0:
                block += ["## Se\xe7\xe3o %d.%d" % (chapter, rng.randrange(1, 9)), ""]
            elif kind == 1:
                block += ["```c", "int main(void)", "{", "    return %d;" % rng.randrange(100), "}", "```", ""]
            elif kind == 2:
                block += ["- " + decorate(rng, sentence(rng, 6)) for _ in range(4)] + [""]
            elif kind == 3:
                block += ["> " + l for l in wrap(sentence(rng, 18), 60)] + [""]
            else:
                paragraph = " ".join(decorate(rng, sentence(rng, rng.randrange(8, 16)))
                                     for _ in range(rng.randrange(2, 6)))
                block += wrap(paragraph + " A\xe7\xe3o, cora\xe7\xe3o e p\xe3o.") + [""]
        text = "\r\n".join(block) + "\r\n"
        parts.append(text)
        size += len(text)
    with open(out, "wb") as f:
        f.write("".join(parts).encode("cp1252"))


if __name__ == "__main__":
    {"typing": typing, "large": large}[sys.argv[1]](sys.argv[2])
