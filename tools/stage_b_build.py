"""Stage B: produce a playable APK from the CI core archive + a local ROM.

Does not compile the huge crszone_lifted_*.c files. Those are already in
libcrisiszone_core.a. This script generates the real H8 from the ROM,
lets CMake link the production libmain.so, then packages and signs the APK.

Windows:
  python tools/stage_b_build.py --core path\\to\\libcrisiszone_core.a --rom path\\to\\crszone.zip --device path\\to\\namco_csz1.zip
"""
from pathlib import Path
import argparse, hashlib, json, os, platform, shutil, subprocess, sys, urllib.request, zipfile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import bootstrap, prepare_crisis_zone, build as build_mod

NDK_REVISION = '29.0.14206865'
STUB_MARKERS = (
    'void tc2sub_run(h8_cpu *c) { (void)c; }',
    'void tc2io_run(h8_cpu *c) { (void)c; }',
)
SDK = ROOT / '.tools/android-sdk'
PLATFORM_URL = 'https://dl.google.com/android/repository/platform-35_r01.zip'
BUILDTOOLS_URL = {
    'Windows': 'https://dl.google.com/android/repository/build-tools_r35_windows.zip',
    'Linux': 'https://dl.google.com/android/repository/build-tools_r35_linux.zip',
}

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
        SDK / 'ndk' / NDK_REVISION,
        ROOT / '.tools/ndk',
    ]
    for c in candidates:
        props = c / 'source.properties'
        if props.is_file():
            text = props.read_text(encoding='utf-8', errors='replace')
            if NDK_REVISION in text:
                return c
            raise SystemExit(f'Wrong NDK at {c}. Expected revision {NDK_REVISION}.\n{text}')
    system = platform.system()
    url = ('https://dl.google.com/android/repository/android-ndk-r29-windows.zip'
           if system == 'Windows' else
           'https://dl.google.com/android/repository/android-ndk-r29-linux.zip')
    dest = SDK / 'ndk' / NDK_REVISION
    dest.parent.mkdir(parents=True, exist_ok=True)
    archive = ROOT / '.tools/ndk-r29.zip'
    print(f'Downloading NDK {NDK_REVISION}', flush=True)
    urllib.request.urlretrieve(url, archive)
    with zipfile.ZipFile(archive) as z:
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
    for item in bootstrap.PACKAGES:
        if item[0] in ('sdl', 'openxr', 'ninja'):
            bootstrap.fetch(item)
    xr = ROOT / '.tools/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so'
    if not xr.is_file():
        raise SystemExit(f'OpenXR loader missing after bootstrap: {xr}')

def download_zip(url: str, dest: Path):
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f'Downloading {url}', flush=True)
    urllib.request.urlretrieve(url, dest)

def ensure_android_sdk():
    platform_jar = SDK / 'platforms/android-35/android.jar'
    bt_dir = SDK / 'build-tools/35.0.0'
    aapt2 = bt_dir / ('aapt2.exe' if platform.system() == 'Windows' else 'aapt2')
    if not platform_jar.is_file():
        archive = ROOT / '.tools/platform-35.zip'
        download_zip(PLATFORM_URL, archive)
        extract = ROOT / '.tools/platform-35-extract'
        if extract.exists():
            shutil.rmtree(extract)
        extract.mkdir(parents=True)
        with zipfile.ZipFile(archive) as z:
            z.extractall(extract)
        jar = next(extract.rglob('android.jar'), None)
        if jar is None:
            raise SystemExit('Downloaded platform 35 zip has no android.jar')
        platform_jar.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(jar, platform_jar)
    if not aapt2.is_file():
        system = platform.system()
        url = BUILDTOOLS_URL.get(system, BUILDTOOLS_URL['Linux'])
        archive = ROOT / '.tools/build-tools-35.zip'
        download_zip(url, archive)
        extract = ROOT / '.tools/build-tools-35-extract'
        if extract.exists():
            shutil.rmtree(extract)
        extract.mkdir(parents=True)
        with zipfile.ZipFile(archive) as z:
            z.extractall(extract)
        found = next(extract.rglob('aapt2.exe' if system == 'Windows' else 'aapt2'), None)
        if found is None:
            raise SystemExit('Downloaded build-tools 35 zip has no aapt2')
        bt_dir.mkdir(parents=True, exist_ok=True)
        # Copy the tool directory that contains aapt2.
        src = found.parent
        for item in src.iterdir():
            dest = bt_dir / item.name
            if item.is_dir():
                if dest.exists():
                    shutil.rmtree(dest)
                shutil.copytree(item, dest)
            else:
                shutil.copy2(item, dest)
    missing = [p for p in (platform_jar, bt_dir / 'aapt2.exe' if platform.system() == 'Windows' else bt_dir / 'aapt2',
                           bt_dir / 'zipalign.exe' if platform.system() == 'Windows' else bt_dir / 'zipalign',
                           bt_dir / 'lib/d8.jar', bt_dir / 'lib/apksigner.jar') if not p.is_file()]
    if missing:
        raise SystemExit('Android SDK tools missing after prepare:\n' + '\n'.join(str(p) for p in missing))
    print('Android platform 35 and build-tools 35.0.0 ready', flush=True)

def java_bin() -> Path:
    home = os.environ.get('JAVA_HOME')
    candidates = []
    if home:
        candidates.append(Path(home) / 'bin')
    candidates.append(Path(r'C:\Program Files\Android\Android Studio\jbr') / 'bin')
    candidates.append(Path(r'C:\Program Files\Android\Android Studio\jbr\bin'))
    suffix = '.exe' if platform.system() == 'Windows' else ''
    needed = ('java', 'javac', 'jar', 'keytool')
    for bin_dir in candidates:
        if all((bin_dir / (name + suffix)).is_file() for name in needed):
            return bin_dir
    raise SystemExit(
        'Java tools not found. Set JAVA_HOME to a JDK (or Android Studio JBR) '
        'that provides java, javac, jar and keytool.'
    )

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

def assert_no_lifted_rebuild(build_dir: Path):
    ninja = ROOT / '.tools/ninja/ninja.exe'
    if not ninja.is_file():
        ninja = ROOT / '.tools/ninja/ninja'
    out = subprocess.check_output([str(ninja), '-C', str(build_dir), '-t', 'targets', 'all'], text=True, errors='replace')
    lifted = [line for line in out.splitlines() if 'crszone_lifted_' in line]
    if lifted:
        raise SystemExit('Precompiled core still has lifted targets:\n' + '\n'.join(lifted[:10]))
    print('Using precompiled Crisis Zone core; lifted sources will not be rebuilt.', flush=True)

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
    metadata=args.core.with_name('core-build-info.json')
    if not metadata.is_file():raise SystemExit('Core metadata missing: use the matching candidate core artifact.')
    info=json.loads(metadata.read_text())
    expected=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    if info.get('source_commit')!=expected:raise SystemExit('Core/source commit mismatch: refusing stale host code.')
    if info.get('sha256')!=hashlib.sha256(args.core.read_bytes()).hexdigest():raise SystemExit('Core checksum mismatch.')
    ndk = ensure_ndk()
    host = ndk_host_dir(ndk)
    clang = clang_exe(host)
    readelf = host / 'bin' / ('llvm-readelf.exe' if platform.system() == 'Windows' else 'llvm-readelf')
    print(f'NDK {NDK_REVISION} host={host.name} clang={clang.name}', flush=True)
    check_aarch64(args.core, readelf)
    ensure_deps()
    ensure_android_sdk()
    java = java_bin()
    # Point the packaging module at the verified Java bin.
    build_mod.JAVA = java
    device = args.device or args.rom.with_name('namco_csz1.zip')
    prepare_crisis_zone.prepare(args.rom, device)
    reject_stubs()
    build_dir = ROOT / 'build/stage-b-native'
    if build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(parents=True)
    ninja = ROOT / '.tools/ninja/ninja.exe'
    if not ninja.is_file():
        ninja = ROOT / '.tools/ninja/ninja'
    cmake_args = [
        'cmake', '-Wno-dev', '-Wno-deprecated',
        '-S', ROOT / 'quest', '-B', build_dir, '-G', 'Ninja',
        '-DCMAKE_MAKE_PROGRAM=' + str(ninja),
        '-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
        '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-29', '-DANDROID_STL=c++_static',
        '-DCMAKE_BUILD_TYPE=Release', '-DTC2_CORE_ONLY=OFF',
        '-DTC2_USE_PRECOMPILED_CORE=' + str(args.core.resolve()),
    ]
    run(cmake_args)
    assert_no_lifted_rebuild(build_dir)
    run(['cmake', '--build', build_dir, '--target', 'main', '--parallel', '2'])
    so = build_dir / 'libmain.so'
    if not so.is_file():
        raise SystemExit('production libmain.so was not produced')
    check_aarch64(so, readelf)
    assets = ROOT / 'build/assets'
    apk = build_mod.package(assets, args.rom, bundle_roms=True, native_dir=build_dir)
    with zipfile.ZipFile(apk) as z:
        if 'lib/arm64-v8a/libmain.so' not in z.namelist():
            raise SystemExit('APK is missing lib/arm64-v8a/libmain.so')
    digest = hashlib.sha256(apk.read_bytes()).hexdigest()
    print(f'APK: {apk}')
    print(f'SHA256: {digest}')
    print('Real csz_sub.c and csz_io.c were linked. Lifted files were not recompiled.')

if __name__ == '__main__':
    main()
