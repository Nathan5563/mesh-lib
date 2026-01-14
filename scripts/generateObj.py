#!/usr/bin/env python3
import argparse
import os
import sys

def gen_tile(
    f,
    v_offset,
    vt_offset,
    vn_offset,
    nx=128,
    ny=128,
    origin_x=0.0,
    origin_y=0.0,
    scale=1.0,
    float_fmt="{:.6f}",
):
    """
    Writes a (nx x ny) grid tile using ONLY: v, vt, vn, f.
    Returns (num_vertices, num_vt, num_vn).
    """
    # Emit vertices (positions, UVs, normals) — strictly v/vt/vn lines
    for j in range(ny + 1):
        y = origin_y + j * scale
        tv = j / ny
        for i in range(nx + 1):
            x = origin_x + i * scale
            tu = i / nx
            f.write("v "  + float_fmt.format(x) + " " + float_fmt.format(y) + " " + float_fmt.format(0.0) + "\n")
            f.write("vt " + float_fmt.format(tu) + " " + float_fmt.format(tv) + "\n")
            f.write("vn " + float_fmt.format(0.0) + " " + float_fmt.format(0.0) + " " + float_fmt.format(1.0) + "\n")

    # Local index helper (1-based within tile)
    def lidx(i, j):
        return j * (nx + 1) + i + 1

    # Faces: two triangles per quad, format: f v/vt/vn v/vt/vn v/vt/vn
    for j in range(ny):
        for i in range(nx):
            v00 = lidx(i,     j)
            v10 = lidx(i + 1, j)
            v01 = lidx(i,     j + 1)
            v11 = lidx(i + 1, j + 1)

            a = v_offset + v00 - 1
            b = v_offset + v10 - 1
            c = v_offset + v01 - 1
            d = v_offset + v11 - 1

            at = vt_offset + v00 - 1
            bt = vt_offset + v10 - 1
            ct = vt_offset + v01 - 1
            dt = vt_offset + v11 - 1

            an = vn_offset + v00 - 1
            bn = vn_offset + v10 - 1
            cn = vn_offset + v01 - 1
            dn = vn_offset + v11 - 1

            f.write(f"f {a}/{at}/{an} {b}/{bt}/{bn} {d}/{dt}/{dn}\n")
            f.write(f"f {a}/{at}/{an} {d}/{dt}/{dn} {c}/{ct}/{cn}\n")

    nverts = (nx + 1) * (ny + 1)
    return nverts, nverts, nverts  # v, vt, vn counts (1:1:1)

def generate_obj(
    path,
    target_bytes,
    nx=128,
    ny=128,
    scale=1.0,
    buffer_bytes=1024 * 1024  # 1 MiB buffered writes for throughput
):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)

    with open(path, "w", buffering=buffer_bytes, encoding="utf-8", newline="\n") as f:
        v_off = 1
        vt_off = 1
        vn_off = 1

        # Lay tiles on a raster to keep coordinates compact
        row = 0
        col = 0
        stride = nx * scale

        while f.tell() < target_bytes:
            ox = col * stride
            oy = row * stride

            nv, nvt, nvn = gen_tile(
                f, v_off, vt_off, vn_off,
                nx=nx, ny=ny, origin_x=ox, origin_y=oy, scale=scale
            )
            v_off  += nv
            vt_off += nvt
            vn_off += nvn

            col += 1
            if col >= 64:
                col = 0
                row += 1

        # Print a short summary to stderr (not into the OBJ file)
        print(f"[done] {path}: {f.tell()} bytes (~{f.tell()/(1024**3):.2f} GiB)", file=sys.stderr)

def parse_args():
    p = argparse.ArgumentParser(description="Generate a very large OBJ using ONLY v/vt/vn/f.")
    p.add_argument("--path", required=True, help="Output .obj path")
    size = p.add_mutually_exclusive_group(required=True)
    size.add_argument("--target-bytes", type=int, help="Target size in bytes")
    size.add_argument("--target-gb", type=float, help="Target size in GiB (1 GiB = 1024^3 bytes)")
    p.add_argument("--nx", type=int, default=128, help="Tile quads in X")
    p.add_argument("--ny", type=int, default=128, help="Tile quads in Y")
    p.add_argument("--scale", type=float, default=1.0, help="Grid spacing")
    return p.parse_args()

if __name__ == "__main__":
    args = parse_args()
    target = args.target_bytes if args.target_bytes is not None else int(args.target_gb * (1024 ** 3))
    generate_obj(
        path=args.path,
        target_bytes=target,
        nx=args.nx,
        ny=args.ny,
        scale=args.scale,
    )
