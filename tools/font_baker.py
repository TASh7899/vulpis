import argparse
import struct
import os
from pathlib import Path
from fontTools.ttLib import TTFont
from blackrenderer.font import BlackRendererFont
from blackrenderer.backends.skia import SkiaCanvas
import skia

def bake_font(font_path, font_size=64):
    script_dir = Path(__file__).parent.absolute()
    project_root = script_dir.parent

    input_path = project_root / font_path
    output_dir = project_root / "src" / "assets" / "baked_fonts"
    output_dir.mkdir(parents=True, exist_ok=True)

    output_base = output_dir / input_path.stem
    print(f"Baking: {input_path} -> {output_dir}")

    # 1. Load Font for Metrics (fontTools)
    tt_font = TTFont(input_path)
    hmtx = tt_font['hmtx']
    hhea = tt_font['hhea']
    cmap = tt_font.getBestCmap()
    units_per_em = tt_font['head'].unitsPerEm
    scale_factor = font_size / units_per_em

    ascent = hhea.ascent * scale_factor
    descent = hhea.descent * scale_factor
    line_gap = hhea.lineGap * scale_factor

    max_row_height = int(ascent - descent + line_gap)
    baseline_offset = ascent

    # 2. Load Font for Rendering (BlackRenderer)
    br_font = BlackRendererFont(input_path)

    # 3. Setup Atlas
    atlas_size = 4096
    surface = skia.Surface(atlas_size, atlas_size)
    canvas = surface.getCanvas()

    cursor_x, cursor_y = 0, 0

    baked_glyphs = [] 

    all_codepoints = sorted(cmap.keys())

    count = 0
    print(f"Found {len(all_codepoints)} characters in font.")

    # we only want visible glyphs. skip 0-31
    for codepoint in all_codepoints:
        if codepoint < 32:
            continue

        glyph_name = cmap[codepoint]

        # if glyph does not have metrics skip
        if glyph_name not in hmtx.metrics:
            print(f"{glyph_name} does not have metrics")
            continue

        advance_width_units, left_side_bearing_unit = hmtx[glyph_name]
        pixel_advance = int(advance_width_units * scale_factor)
        pixel_lsb = int(left_side_bearing_unit * scale_factor)

        x_offset = 0
        baked_bearing_x = 0

        if pixel_lsb < 0:
            x_offset = -pixel_lsb
            baked_bearing_x = pixel_lsb


        if cursor_x + pixel_advance > atlas_size:
            cursor_x = 0
            cursor_y = max_row_height

        if cursor_y + max_row_height > atlas_size:
            print(f"atlas full!! stopped at char index {count}. Consider increasing atlas_size")
            break

        canvas.save()
        draw_y = cursor_y + baseline_offset
        canvas.translate(cursor_x + x_offset, draw_y)
        canvas.scale(scale_factor, -scale_factor)

        skia_ctx = SkiaCanvas(canvas)
        br_font.drawGlyph(glyph_name, skia_ctx)

        canvas.restore()

        u_min = cursor_x / atlas_size
        v_min = cursor_y / atlas_size
        u_max = (cursor_x + pixel_advance) / atlas_size
        v_max = (cursor_y + max_row_height) / atlas_size

        glyph_struct = struct.pack(
            'IiiiiIffff',
            codepoint,
            int(pixel_advance + x_offset), int(max_row_height),
            int(baked_bearing_x), int(baseline_offset),
            int(pixel_advance),
            u_min, v_min, u_max, v_max
        )
        baked_glyphs.append(glyph_struct)

        cursor_x += pixel_advance + x_offset + 2
        count += 1

    # 5. Save Files
    image = surface.makeImageSnapshot()
    image.save(str(output_base) + ".png", skia.kPNG)

    with open(str(output_base) + ".bin", "wb") as f:
        f.write(b'BAKE')
        f.write(struct.pack('III', len(baked_glyphs), atlas_size, atlas_size))
        for g in baked_glyphs:
            f.write(g)

    print(f"Success! Saved to {output_base}.bin")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("font", help="Path to .ttf file")
    args = parser.parse_args()
    bake_font(args.font)
