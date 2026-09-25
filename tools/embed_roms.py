import os
import sys
import re
import argparse

POLYNOMIAL = 0x04C11DB7
TOPBIT = 1 << 31

def crc_slow(data: bytes) -> int:
    remainder = 0
    for byte in data:
        remainder ^= (byte << 24) & 0xFFFFFFFF
        for _ in range(8):
            if remainder & TOPBIT:
                remainder = ((remainder << 1) ^ POLYNOMIAL) & 0xFFFFFFFF
            else:
                remainder = (remainder << 1) & 0xFFFFFFFF
    return remainder

def sanitize_identifier(name: str) -> str:
    s = re.sub(r'[^0-9a-zA-Z_]', '_', name)
    if s and s[0].isdigit():
        s = 'rom_' + s
    else:
        s = 'rom_' + s
    return s.lower()

def format_title(filename: str) -> str:
    base = os.path.splitext(filename)[0]
    title = base.replace('_', ' ').replace('-', ' ').strip()
    if title.islower() or title.isupper():
        title = title.title()
    return title

def generate_catalog(input_dir: str, output_c: str, output_h: str):
    if not os.path.exists(input_dir):
        print(f"Erreur : le dossier source '{input_dir}' n'existe pas.", file=sys.stderr)
        sys.exit(1)

    nes_files = sorted([f for f in os.listdir(input_dir) if f.lower().endswith('.nes')])
    if not nes_files:
        print(f"Attention : aucun fichier .nes trouvé dans '{input_dir}'.", file=sys.stderr)

    games = []
    for f in nes_files:
        path = os.path.join(input_dir, f)
        with open(path, 'rb') as fp:
            data = fp.read()
        
        if len(data) < 16 or data[:4] != b'NES\x1a':
            print(f"Avertissement : '{f}' ne possède pas un en-tête iNES valide (NES\\x1a). Il sera ignoré.", file=sys.stderr)
            continue
        
        var_name = sanitize_identifier(os.path.splitext(f)[0])
        title = format_title(f)
        crc = crc_slow(data)
        mapper = (data[6] >> 4) | (data[7] & 0xF0)
        prg_kb = data[4] * 16
        chr_kb = data[5] * 8

        games.append({
            'filename': f,
            'var_name': var_name,
            'title': title,
            'size': len(data),
            'crc': crc,
            'mapper': mapper,
            'prg_kb': prg_kb,
            'chr_kb': chr_kb,
            'data': data
        })

    os.makedirs(os.path.dirname(os.path.abspath(output_c)), exist_ok=True)
    if output_h:
        os.makedirs(os.path.dirname(os.path.abspath(output_h)), exist_ok=True)

    with open(output_c, 'w', encoding='utf-8') as fc:
        fc.write("#include \"game_entry.h\"\n")
        fc.write("#include <stddef.h>\n")
        fc.write("#include <stdint.h>\n\n")

        for g in games:
            fc.write(f"static const uint8_t {g['var_name']}[{g['size']}] __attribute__((section(\".rodata\"), aligned(4))) = {{\n")
            
            data = g['data']
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
                if i + 16 < len(data):
                    fc.write(f"  {hex_str},\n")
                else:
                    fc.write(f"  {hex_str}\n")
            fc.write("};\n\n")

        fc.write("const GameEntry game_catalog[] = {\n")
        for g in games:
            fc.write(f'  {{ "{g["title"]}", {g["var_name"]}, sizeof({g["var_name"]}), 0x{g["crc"]:08X}U, {g["mapper"]}, {g["prg_kb"]}, {g["chr_kb"]} }},\n')
        if not games:
            fc.write("  { NULL, NULL, 0, 0, 0, 0, 0 }\n")
        fc.write("};\n\n")
        fc.write(f"const size_t game_catalog_count = {len(games)};\n\n")

        fc.write("static const GameEntry *s_current_game = NULL;\n\n")
        fc.write("void game_set_current(const GameEntry *game) {\n")
        fc.write("  s_current_game = game;\n")
        fc.write("}\n\n")
        fc.write("const GameEntry *game_get_current(void) {\n")
        fc.write("  return s_current_game;\n")
        fc.write("}\n")

    print(f"Catalogue généré avec succès dans '{output_c}' ({len(games)} jeux intégrés).")

def main():
    parser = argparse.ArgumentParser(description="Générateur de catalogue de ROMs C pour NumWorks.")
    parser.add_argument("-i", "--input", default="roms", help="Dossier contenant les fichiers .nes")
    parser.add_argument("-c", "--output-c", default="src/rom_catalog.c", help="Chemin du fichier source C à générer")
    parser.add_argument("-H", "--output-h", default="", help="Chemin du fichier d'en-tête H (optionnel)")
    args = parser.parse_args()

    generate_catalog(args.input, args.output_c, args.output_h)

if __name__ == "__main__":
    main()
