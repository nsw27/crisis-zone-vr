"""Deprecated. Use tools/stage_b_build.py — it produces the signed APK."""
import sys
print("stage_b_link.py is deprecated. Use:", file=sys.stderr)
print("  python tools/stage_b_build.py --core <libcrisiszone_core.a> --rom <crszone.zip> --device <namco_csz1.zip>", file=sys.stderr)
sys.exit(2)
