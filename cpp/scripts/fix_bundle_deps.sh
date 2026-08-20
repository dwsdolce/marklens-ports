#!/usr/bin/env bash
# Make a macdeployqt'd .app fully self-contained when built against Homebrew Qt.
#
# Homebrew splits Qt across many kegs (qtbase, qtdeclarative, qtwebengine, …),
# and macdeployqt doesn't rewrite every cross-keg reference — most notably the
# QtWebEngineProcess helper still loads QtCore/QtGui/QtQuick from /opt/homebrew.
# This rewrites any remaining /opt/homebrew LOAD dependency to point inside the
# bundle (leaving the harmless self install-IDs), then re-signs.
#
# Usage: fix_bundle_deps.sh <path-to.app>
set -euo pipefail

APP="${1:?usage: fix_bundle_deps.sh <app-bundle>}"
FRAMEWORKS="$APP/Contents/Frameworks"

find "$APP" -type f | while read -r f; do
    file "$f" 2>/dev/null | grep -q "Mach-O" || continue
    selfid="$(otool -D "$f" 2>/dev/null | sed -n 2p)"

    # (1) Homebrew load deps: give this binary an rpath to the app's Frameworks
    # (separate helper processes like QtWebEngineProcess need their own, since
    # @rpath resolves against the *running* executable's rpaths), then repoint.
    homebrew="$(otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}' | grep '^/opt/homebrew/' || true)"
    if [ -n "$homebrew" ]; then
        rel="$(python3 -c 'import os,sys; print(os.path.relpath(sys.argv[1], os.path.dirname(sys.argv[2])))' "$FRAMEWORKS" "$f")"
        install_name_tool -add_rpath "@loader_path/$rel" "$f" 2>/dev/null || true
        while read -r dep; do
            [ -z "$dep" ] || [ "$dep" = "$selfid" ] && continue
            # /opt/homebrew/opt/<keg>/lib/QtX.framework/.../QtX -> @rpath/QtX.framework/.../QtX
            install_name_tool -change "$dep" "@rpath/${dep#*/lib/}" "$f"
        done <<< "$homebrew"
    fi

    # (2) macdeployqt writes some deps as @executable_path/../Frameworks/… — fine
    # for the main app, but broken in the WebEngine helper process, whose
    # @executable_path is the helper, not the app. @rpath works for both.
    execpath="$(otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}' | grep '^@executable_path/../Frameworks/' || true)"
    while read -r dep; do
        [ -z "$dep" ] && continue
        install_name_tool -change "$dep" "@rpath/${dep#@executable_path/../Frameworks/}" "$f"
    done <<< "$execpath"
done

# install_name_tool invalidated every signature it touched, so something has to
# re-sign. A caller with a real identity - packaging/build_mac - re-signs the
# whole bundle itself, inside-out, with the hardened runtime and the WebEngine
# helper's entitlements; signing here would only be thrown away, and `--deep`
# would not apply those entitlements anyway. Ad-hoc signing remains the
# fallback for a local build, where an unsigned bundle will not launch at all
# on Apple silicon.
if [ -n "${CODESIGN_IDENTITY:-}" ]; then
    echo "fix_bundle_deps: CODESIGN_IDENTITY is set - leaving signing to the caller"
else
    codesign --force --deep --sign - "$APP"
fi

echo "fix_bundle_deps: rewrote remaining /opt/homebrew load paths into $APP"
