"""Stage B: produce a playable APK from the CI core archive + a local ROM.

Does not compile the huge crszone_lifted_*.c files. Those are already in
libcrisiszone_core.a. This script generates the real H8 from the ROM,
lets CMake link the production libmain.so, then packages and signs the APK.

Windows:
  python tools/stage_b_build.py --core path\\to\\libcrisiszone_core.a --rom path\\to\\crszone.zip --device path\\to\\namco_csz1.zip
"""
from pathlib import Path
import argparse, hashlib, os, platform, shutil, subprocess, sys, zipfile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import bootstrap, prepare_crisis_zone, build as build_mod

NDK_REVISION = '29.0.14206865'
STUB_MARKERS = (
    'void tc2sub_run(h8_cpu *c) { (void)c; }',
    'void tc2io_run(h8_cpu *c) { (void)c; }',
)

def run(cmd, **kw):
    print('> ' + ' '.join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], check=True, **kw)

def ndk_host_dir(ndk: Path) -> Path:
    prebuilt = ndk / 'toolchains/llvm/prebuilt'
    hosts = [p for p in prebuilt.iterdir() if p.is_dir()] if prebuilt.is_dir() else []
    if not hosts:
        raise SystemExit(f'No NDK prebuilt host under {prebuilt}')
    prefer = 'windows-x86_64' if platform.system() == 'Windows' else 'linux-x86_64'
    for h in hosts:
        if h.name == prefer:
            return h
    return hosts[0]

def clang_exe(host: Path) -> Path:
    for name in ('clang.exe', 'clang'):
        p = host / 'bin' / name
        if p.is_file():
            return p
    raise SystemExit(f'clang not found under {host}/bin')

def ensure_ndk() -> Path:
    candidates = [
        ROOT / '.tools/android-sdk/ndk' / NDK_REVISION,
        ROOT / '.tools/ndk',
    ]
    for c in candidates:
        props = c / 'source.properties'
        if props.is_file():
            text = props.read_text(encoding='utf-8', errors='replace')
            if NDK_REVISION in text:
                return c
            raise SystemExit(
                f'Wrong NDK at {c}. Expected revision {NDK_REVISION}.\n{text}'
            )
    system = platform.system()
    url = ('https://dl.google.com/android/repository/android-ndk-r29-windows.zip'
           if system == 'Windows' else
           'https://dl.google.com/android/repository/android-ndk-r29-linux.zip')
    dest = ROOT / '.tools/android-sdk/ndk' / NDK_REVISION
    dest.parent.mkdir(parents=True, exist_ok=True)
    archive = ROOT / '.tools/ndk-r29.zip'
    print(f'Downloading NDK {NDK_REVISION}', flush=True)
    import urllib.request
    urllib.request.urlretrieve(url, archive)
    import zipfile as zf
    with zf.ZipFile(archive) as z:
        z.extractall(dest.parent)
    extracted = next(dest.parent.glob('android-ndk-*'))
    if extracted != dest:
        if dest.exists():
            shutil.rmtree(dest)
        extracted.rename(dest)
    props = dest / 'source.properties'
    text = props.read_text(encoding='utf-8', errors='replace') if props.is_file() else ''
    if NDK_REVISION not in text:
        raise SystemExit(f'Downloaded NDK revision mismatch.\n{text}')
    return dest

def ensure_deps():
    # SDL and OpenXR are ROM-independent and small. Fetch if missing.
    for item in bootstrap.PACKAGES:
        if item[0] in ('sdl', 'openxr', 'ninja'):
            bootstrap.fetch(item)
    xr = ROOT / '.tools/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so'
    if not xr.is_file():
        raise SystemExit(f'OpenXR loader missing after bootstrap: {xr}')

def reject_stubs():
    for name in ('csz_sub.c', 'csz_io.c'):
        path = ROOT / 'upstream/crszone/gen' / name
        if not path.is_file():
            raise SystemExit(f'Missing generated H8 source: {path}')
        text = path.read_text(encoding='utf-8', errors='replace')
        if any(m in text for m in STUB_MARKERS) or len(text) < 2000:
            raise SystemExit(f'{path} looks like a stub. Refusing to package.')

def check_aarch64(path: Path, readelf: Path):
    out = subprocess.check_output([str(readelf), '-h', str(path)], text=True, errors='replace')
    if 'AArch64' not in out and 'EM_AARCH64' not in out:
        raise SystemExit(f'{path} is not AArch64:\n{out}')

def main():
    p = argparse.ArgumentParser(description='Stage B: CI core + local ROM -> signed APK')
    p.add_argument('--core', type=Path, required=True, help='libcrisiszone_core.a from CI')
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--device', type=Path, default=None)
    args = p.parse_args()
    if not args.core.is_file():
        raise SystemExit(f'core archive not found: {args.core}')
    if not args.rom.is_file():
        raise SystemExit(f'ROM not found: {args.rom}')
    ndk = ensure_ndk()
    host = ndk_host_dir(ndk)
    clang = clang_exe(host)
    readelf = host / 'bin' / ('llvm-readelf.exe' if platform.system() == 'Windows' else 'llvm-readelf')
    print(f'NDK {NDK_REVISION} host={host.name} clang={clang.name}', flush=True)
    check_aarch64(args.core, readelf)
    ensure_deps()
    device = args.device or args.rom.with_name('namco_csz1.zip')
    prepare_crisis_zone.prepare(args.rom, device)
    reject_stubs()
    build_dir = ROOT / 'build/native'
    build_dir.mkdir(parents=True, exist_ok=True)
    ninja = ROOT / '.tools/ninja/ninja.exe'
    if not ninja.is_file():
        ninja = ROOT / '.tools/ninja/ninja'
    cmake_args = [
        'cmake', '-Wno-dev', '-Wno-deprecated',
        '-S', ROOT / 'quest', '-B', build_dir, '-G', 'Ninja',
        '-DCMAKE_MAKE_PROGRAM=' + str(ninja),
        '-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
        '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-29', '-DANDROID_STL=c++_static',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DTC2_USE_PRECOMPILED_CORE=' + str(args.core.resolve()),
    ]
    run(cmake_args)
    # Build only the production target. SDL is a dependency. Lifted files are not.
    run(['cmake', '--build', build_dir, '--target', 'main', '--parallel', '2'])
    so = build_dir / 'libmain.so'
    if not so.is_file():
        raise SystemExit('production libmain.so was not produced')
    check_aarch64(so, readelf)
    assets = ROOT / 'build/assets'
    # Reuse the existing packaging (strip, zipalign, sign, verify).
    build_mod.package(assets)
    apk = next((ROOT / 'artifacts').glob('CrisisZoneVR-*-quest3.apk'))
    with zipfile.ZipFile(apk) as z:
        names = z.namelist()
        if 'lib/arm64-v8a/libmain.so' not in names:
            raise SystemExit('APK is missing lib/arm64-v8a/libmain.so')
    digest = hashlib.sha256(apk.read_bytes()).hexdigest()
    print(f'APK: {apk}')
    print(f'SHA256: {digest}')
    print('Real csz_sub.c and csz_io.c were linked. Lifted files were not recompiled.')

if __name__ == '__main__':
    main()
