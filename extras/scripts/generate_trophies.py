#!/usr/bin/env python3
"""
Generate the PS Vita TROPHY.TRP for Riptide GP (NoTrpDrm style, same packer as
the Prince of Persia Classic port).

Trophy list = the game's own AchievementDB (Base.apf -> VuDBAsset/AchievementDB)
with the official en-us names/descriptions (Ach_N_Name / Ach_N_Desc). Achievement
23 "Throw Down" (beat a friend's challenge) needs Google Play and is left out.

The ORDER of TROPHIES[1:] must match ACHIEVEMENTS[] in source/patch.c:
trophy id N  <->  AchievementDB key ACHIEVEMENTS[N].

Usage (from the repo root):  python3 extras/scripts/generate_trophies.py
"""

import io
import os
import struct
import hashlib
from PIL import Image, ImageDraw

COMM_ID = "RPGP00001_00"

# (ttype, name, detail) -- ttype from the game's Points: <=25 B, 30-40 S, >=50 G
TROPHIES = [
    ("P", "Riptide GP", "Earn every other trophy."),
    ("B", "First Steps", "Earn a medal of any type in any event."),
    ("B", "24 Karat", "Earn a gold medal in any event."),
    ("S", "1000 Karat", "Earn a gold medal in any event in the 1000cc class."),
    ("B", "Starter Kit", "Unlock a hydro jet."),
    ("S", "Collector", "Unlock all hydro jets."),
    ("B", "Race Enthusiast", "Earn a medal of any kind in every one of the Race events."),
    ("B", "Hot Lap Enthusiast", "Earn a medal of any kind in every one of the Hot Lap events."),
    ("B", "Championship Enthusiast", "Earn a medal in every one of the Championships."),
    ("S", "Riptide Enthusiast", "Earn a medal of any kind in every Race, Hot lap, and Championship."),
    ("B", "Race Pro", "Earn a gold medal in every one of the Race events."),
    ("B", "Hot Lap Pro", "Earn a gold medal in every one of the Hot Lap events."),
    ("B", "Championship Pro", "Earn a gold medal in every one of the Championships."),
    ("G", "Riptide Pro", "Earn a gold medal in every Race, Hot lap, and Championship."),
    ("S", "Race Grand Master", "Earn a 1000cc gold medal in every one of the Race events."),
    ("S", "Hot Lap Grand Master", "Earn a 1000cc gold medal in every one of the Hot Lap events."),
    ("S", "Championship Grand Master", "Earn a 1000cc gold medal in every one of the Championships."),
    ("G", "Riptide Grand Master", "Earn a 1000cc gold medal every Race, Hot lap, and Championship."),
    ("B", "Entry Level Champ", "Earn a medal in the Entry Level Championship."),
    ("B", "Pro Circuit Champ", "Earn a medal in the Pro Circuit Championship."),
    ("B", "Master Series Champ", "Earn a medal in the Master Series Championship."),
    ("B", "Grand Prix Champ", "Earn a medal in the Grand Prix Championship."),
    ("B", "Reverse Champ", "Earn a medal in the Grand Prix R Championship."),
    ("B", "Kickin' It", "Perform a stunt. Any stunt."),
    ("B", "Stunt Master", "Perform one of every stunt in the game."),
    ("B", "The Right Stuff", "Launch the rocket in Launchpad or Launchpad R."),
    ("B", "Bridge Burner", "Break the bridge in Alpha City or Alpha City R."),
    ("B", "Harpist", "Jump through the harp sculpture in Opera Plaza R without breaking it."),
    ("B", "Mad Props", "Watch the game credits all the way through."),
    ("B", "Luddite", "Smash all four control room computer towers in a single Race or Hot Lap in Launchpad."),
]

FRAME = {"P": (180, 220, 240), "G": (230, 190, 60), "S": (200, 200, 210), "B": (190, 120, 70)}


def xml_escape(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def trophy_attrs(i, ttype):
    return f'id="{i:03d}" hidden="no" ttype="{ttype}" pid="{"-1" if i == 0 else "000"}"'


def make_sfm(with_text):
    lines = [
        f"<!--Sce-Np-Trophy-Signature: {'x' * 320}-->",
        '<trophyconf version="1.1" platform="psp2" policy="large">',
        f" <npcommid>{COMM_ID}</npcommid>",
        " <trophyset-version>01.00</trophyset-version>",
        ' <parental-level license-area="default">0</parental-level>',
    ]
    if with_text:
        lines += [" <title-name>Riptide GP</title-name>",
                  " <title-detail>Riptide GP PlayStation Vita Port</title-detail>"]
    for i, (ttype, name, detail) in enumerate(TROPHIES):
        if not with_text:
            lines.append(f" <trophy {trophy_attrs(i, ttype)}/>")
            continue
        lines += [f" <trophy {trophy_attrs(i, ttype)}>",
                  f"  <name>{xml_escape(name)}</name>",
                  f"  <detail>{xml_escape(detail)}</detail>",
                  " </trophy>"]
    lines.append("</trophyconf>\n")
    return "\n".join(lines).encode("utf-8")


def png8(img):
    """8-bit palette PNG: full-RGBA icons made the set install fail (NP-6185-0)."""
    buf = io.BytesIO()
    img.convert("RGBA").quantize(256, method=Image.Quantize.FASTOCTREE).save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def trophy_icon(icon, ttype):
    # ponytail: same game icon for every trophy, only the tier frame changes; swap in real art if wanted.
    canvas = Image.new("RGBA", (240, 240), (0, 0, 0, 0))
    ImageDraw.Draw(canvas).rounded_rectangle((0, 0, 239, 239), radius=36, fill=FRAME[ttype] + (255,))
    canvas.paste(icon.resize((208, 208), Image.LANCZOS), (16, 16))
    return canvas


def build_trp(file_dict, output_path):
    def align64(v):
        return (v + 63) & ~63

    first_file_offset = align64(64 + len(file_dict) * 64)
    offsets, cur = {}, first_file_offset
    for name, data in file_dict.items():
        offsets[name] = cur
        cur = align64(cur + len(data))
    total_size = cur

    out = bytearray(struct.pack(">4sIQIII20s16s", b"\xdc\xa2\x4d\x00", 2, total_size,
                                len(file_dict), 64, 0, b"\x00" * 20, b"\x00" * 16))
    for name, data in file_dict.items():
        out += struct.pack(">32sQQI12s", name.encode("latin1"), offsets[name], len(data), 0, b"\x00" * 12)
    for name, data in file_dict.items():
        out += b"\x00" * (offsets[name] - len(out))
        out += data
    out += b"\x00" * (total_size - len(out))
    # Header field is a SHA-1 over the whole file with the field zeroed; the
    # firmware checks it in sceNpTrophySetupDialogInit even with NoTrpDrm.
    out[28:48] = hashlib.sha1(bytes(out)).digest()

    with open(output_path, "wb") as f:
        f.write(out)
    print(f"TRP written to {output_path} ({len(out)} bytes, {len(TROPHIES)} trophies)")


def main():
    outdir = f"extras/trophy/{COMM_ID}"
    os.makedirs(outdir, exist_ok=True)
    icon = Image.open("extras/livearea/icon0.png").convert("RGBA")
    banner = Image.open("extras/livearea/pic0.png").convert("RGBA")
    # ICON0 is 320x176: center-crop pic0 (960x544) to that aspect, then scale.
    h = 960 * 176 // 320
    banner = banner.crop((0, (544 - h) // 2, 960, (544 - h) // 2 + h)).resize((320, 176), Image.LANCZOS)

    files = {"TROPCONF.SFM": make_sfm(False), "TROP.SFM": make_sfm(True), "ICON0.PNG": png8(banner)}
    for i, (ttype, _, _) in enumerate(TROPHIES):
        files[f"TROP{i:03d}.PNG"] = png8(trophy_icon(icon, ttype))
    build_trp(files, os.path.join(outdir, "TROPHY.TRP"))


if __name__ == "__main__":
    main()
