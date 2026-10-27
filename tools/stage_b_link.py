"""Stage B: link a production libmain.so from the CI core archive + real H8.

Usage (after downloading crisiszone-precompiled-core and extracting the .a):

  python tools/stage_b_link.py \\
      --core path/to/libcrisiszone_core.a \\
      --rom path/to/crszone.zip \\
      --device path/to/namco_csz1.zip

This script:
  1. Validates the ROM and generates the real csz_sub.c / csz_io.c.
  2. Compiles only those two small H8 sources with NDK r29.
  3. Links them with the precompiled core into libmain.so.
  4. Prints the path to the production library.

The CI archive does NOT contain stub H8. The real generated files are linked here.
"""
from pathlib import Path
import argparse, os, subprocess, sys
ROOT = Path(__file__).resolve().parents[1]

def run(cmd, **kw):
    print('+', ' '.join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], check=True, **kw)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--core', type=Path, required=True, help='libcrisiszone_core.a from CI')
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--device', type=Path, default=None)
    p.add_argument('--ndk', type=Path, default=ROOT/'.tools/ndk')
    args = p.parse_args()
    if not args.core.is_file():
        sys.exit(f'missing core archive: {args.core}')
    # Generate real H8 from the ROM.
    sys.path.insert(0, str(ROOT/'tools'))
    import prepare_crisis_zone
    prepare_crisis_zone.prepare(args.rom, args.device)
    sub = ROOT/'upstream/crszone/gen/csz_sub.c'
    io = ROOT/'upstream/crszone/gen/csz_io.c'
    if not sub.is_file() or not io.is_file():
        sys.exit('real H8 sources were not generated')
    ndk = args.ndk
    clang = ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/clang'
    if not clang.is_file():
        # Windows NDK layout
        clang = next(ndk.glob('toolchains/llvm/prebuilt/*/bin/clang*'), None)
    if not clang or not Path(clang).is_file():
        sys.exit(f'clang not found under {ndk}; install NDK r29 into .tools/ndk')
    out = ROOT/'build/stage-b'
    out.mkdir(parents=True, exist_ok=True)
    flags = ['--target=aarch64-none-linux-android29', f'--sysroot={ndk}/toolchains/llvm/prebuilt/linux-x86_64/sysroot',
             '-DTCVR', '-DXR_USE_PLATFORM_ANDROID', '-DXR_USE_GRAPHICS_API_OPENGL_ES', '-DTC2_ADDR_MASK=0x1FFFFFFFu',
             '-O2', '-fPIC', '-Iquest', '-Iquest/include', '-Iupstream/tc2/include', '-Iupstream/tc2/src',
             '-Iupstream/crszone/include', '-Iupstream/crszone/gen']
    run([clang, *flags, '-c', sub, '-o', out/'csz_sub.o'])
    run([clang, *flags, '-c', io, '-o', out/'csz_io.o'])
    so = out/'libmain.so'
    run([clang, *flags, '-shared', '-o', so, 'quest/quest_main.c', out/'csz_sub.o', out/'csz_io.o',
         args.core, '-Lbuild/native/sdl', '-lSDL2',
         f'-L{ROOT}/.tools/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a', '-lopenxr_loader',
         '-lGLESv3', '-lEGL', '-landroid', '-llog', '-lz', '-lm',
         '-Wl,-z,max-page-size=16384', '-Wl,--no-undefined'])
    print(f'production libmain.so: {so} ({so.stat().st_size} bytes)')
    print('Real csz_sub.c and csz_io.c were compiled and linked into this binary.')

if __name__ == '__main__':
    main()
