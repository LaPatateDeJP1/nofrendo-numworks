import os
import sys
import subprocess
import shutil
import shlex

CC = "arm-none-eabi-gcc"
NWLINK = ["npx", "--yes", "--", "nwlink@0.0.19"]

def get_eadk_cflags():
    res = subprocess.run(NWLINK + ["eadk-cflags-device"], stdout=subprocess.PIPE, text=True, check=True, shell=(os.name == "nt"))
    return shlex.split(res.stdout.strip())

CFLAGS = [
    "-Os", "-DNDEBUG",
    "-fdata-sections", "-ffunction-sections",
    "-flto", "-fno-fat-lto-objects", "-fwhole-program", "-fvisibility=internal",
    "-Isrc", "-Isrc/nofrendo", "-Isrc/nofrendo/nes", "-Isrc/nofrendo/libsnss", "-Isrc/nofrendo/cpu"
] + get_eadk_cflags()

LDFLAGS = [
    "-Wl,--relocatable",
    "-nostartfiles",
    "--specs=nano.specs",
    "-Wl,-e,main",
    "-Wl,-u,eadk_app_name",
    "-Wl,-u,eadk_app_icon",
    "-Wl,-u,eadk_api_level",
    "-Wl,--gc-sections",
    "-flinker-output=nolto-rel",
]

NOFRENDO_SRCS = [
    "src/nofrendo/bitmap.c",
    "src/nofrendo/config.c",
    "src/nofrendo/cpu/dis6502.c",
    "src/nofrendo/cpu/nes6502.c",
    "src/nofrendo/event.c",
    "src/nofrendo/libsnss/libsnss.c",
    "src/nofrendo/log.c",
    "src/nofrendo/mappers/map000.c",
    "src/nofrendo/mappers/map001.c",
    "src/nofrendo/mappers/map002.c",
    "src/nofrendo/mappers/map003.c",
    "src/nofrendo/mappers/map004.c",
    "src/nofrendo/mappers/map005.c",
    "src/nofrendo/mappers/map007.c",
    "src/nofrendo/mappers/map008.c",
    "src/nofrendo/mappers/map009.c",
    "src/nofrendo/mappers/map011.c",
    "src/nofrendo/mappers/map015.c",
    "src/nofrendo/mappers/map016.c",
    "src/nofrendo/mappers/map018.c",
    "src/nofrendo/mappers/map019.c",
    "src/nofrendo/mappers/map024.c",
    "src/nofrendo/mappers/map032.c",
    "src/nofrendo/mappers/map033.c",
    "src/nofrendo/mappers/map034.c",
    "src/nofrendo/mappers/map040.c",
    "src/nofrendo/mappers/map041.c",
    "src/nofrendo/mappers/map042.c",
    "src/nofrendo/mappers/map046.c",
    "src/nofrendo/mappers/map050.c",
    "src/nofrendo/mappers/map064.c",
    "src/nofrendo/mappers/map065.c",
    "src/nofrendo/mappers/map066.c",
    "src/nofrendo/mappers/map069.c",
    "src/nofrendo/mappers/map070.c",
    "src/nofrendo/mappers/map073.c",
    "src/nofrendo/mappers/map075.c",
    "src/nofrendo/mappers/map078.c",
    "src/nofrendo/mappers/map079.c",
    "src/nofrendo/mappers/map085.c",
    "src/nofrendo/mappers/map087.c",
    "src/nofrendo/mappers/map093.c",
    "src/nofrendo/mappers/map094.c",
    "src/nofrendo/mappers/map099.c",
    "src/nofrendo/mappers/map160.c",
    "src/nofrendo/mappers/map225.c",
    "src/nofrendo/mappers/map229.c",
    "src/nofrendo/mappers/map231.c",
    "src/nofrendo/mappers/mapvrc.c",
    "src/nofrendo/memguard.c",
    "src/nofrendo/nes/mmclist.c",
    "src/nofrendo/nes/nes.c",
    "src/nofrendo/nes/nes_mmc.c",
    "src/nofrendo/nes/nes_pal.c",
    "src/nofrendo/nes/nes_ppu.c",
    "src/nofrendo/nes/nes_rom.c",
    "src/nofrendo/nes/nesinput.c",
    "src/nofrendo/nes/nesstate.c",
    "src/nofrendo/nofrendo.c",
    "src/nofrendo/pcx.c",
]

PORT_SRCS = [
    "src/timing.c",
    "src/display.c",
    "src/sound.c",
    "src/keyboard.c",
    "src/lz4.c",
    "src/main.c",
    "src/menu.c",
    "src/pause_menu.c",
    "src/rom_catalog.c",
    "src/osd.c",
    "src/i18n.c",
    "src/settings.c",
    "src/statefile_wrapper.c",
    "src/storage.c",
    "src/stubs.c",
]

def run(cmd, shell=False):
    res = subprocess.run(cmd, shell=shell)
    if res.returncode != 0:
        print(f"Erreur lors de l'exécution de: {cmd}", file=sys.stderr)
        sys.exit(res.returncode)

def main():
    os.makedirs("output/nofrendo/cpu", exist_ok=True)
    os.makedirs("output/nofrendo/libsnss", exist_ok=True)
    os.makedirs("output/nofrendo/mappers", exist_ok=True)
    os.makedirs("output/nofrendo/nes", exist_ok=True)

    objs = []

    for src in NOFRENDO_SRCS:
        rel = os.path.relpath(src, "src/nofrendo")
        obj = os.path.join("output/nofrendo", os.path.splitext(rel)[0] + ".o")
        objs.append(obj)
        if not os.path.exists(obj) or os.path.getmtime(src) > os.path.getmtime(obj):
            print(f"CC      {src}")
            run([CC] + CFLAGS + ["-c", src, "-o", obj])

    for src in PORT_SRCS:
        base = os.path.basename(src)
        obj = os.path.join("output", os.path.splitext(base)[0] + ".o")
        objs.append(obj)
        if not os.path.exists(obj) or os.path.getmtime(src) > os.path.getmtime(obj):
            print(f"CC      {src}")
            run([CC] + CFLAGS + ["-c", src, "-o", obj])

    icon_png = "src/icon.png"
    icon_obj = "output/icon.o"
    if not os.path.exists(icon_obj) or (os.path.exists(icon_png) and os.path.getmtime(icon_png) > os.path.getmtime(icon_obj)):
        print(f"ICON    {icon_png}")
        run(NWLINK + ["png-icon-o", icon_png, icon_obj], shell=(os.name == "nt"))

    all_objs = objs + [icon_obj]

    nwa = "output/nofrendo.nwa"
    print(f"LD      {nwa}")
    run([CC] + CFLAGS + LDFLAGS + all_objs + ["-lm", "-o", nwa])

    bin_out = "output/nofrendo.bin"
    print(f"NWLINK  {bin_out}")
    run(NWLINK + ["nwa-bin", nwa, bin_out], shell=(os.name == "nt"))

    run(["arm-none-eabi-size", nwa])
    print("\n==================================================")
    print("Compilation terminée avec succès !")
    print(f"Binaire généré : {bin_out}")
    print("==================================================\n")

if __name__ == "__main__":
    main()
