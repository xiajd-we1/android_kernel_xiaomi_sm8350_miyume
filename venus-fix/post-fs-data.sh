#!/system/bin/sh
# Runs right after /data is mounted, before zygote.
#
# Everything here used to be a manual edit, which meant every kernel reflash or
# data wipe silently reverted it. Running it at post-fs-data guarantees the
# device always boots in the hardened state.
#
# Why not rely on the susfs4ksu module's own boot-completed.sh?
# On this device that script does not run at all (the susfs4ksu module has no
# logs/ directory, so nothing ever wrote there), which is why cmdline kept
# falling back to androidboot.verifiedbootstate=orange. Driving ksu_susfs
# directly removes that dependency.
MODDIR=${0%/*}
CONF=/data/adb/susfs4ksu/config.sh
SUSPATH=/data/adb/susfs4ksu/sus_path.txt
OMK=/data/adb/modules/oh_my_keymint/injector.toml
KSU_SUSFS=/data/adb/ksu/bin/ksu_susfs

log() { echo "venus_fix: $*" >> /data/adb/venus_fix.log; }

# ---------------------------------------------------------------- SUSFS config
# Only rewrite keys that already exist: susfs4ksu owns the file layout and a
# missing key means a different SUSFS build, which we must not guess at.
if [ -f "$CONF" ]; then
    sed -i 's/^susfs_log=.*/susfs_log=0/'                     "$CONF" 2>/dev/null
    sed -i "s/^kernel_version=.*/kernel_version='default'/"   "$CONF" 2>/dev/null
    sed -i "s/^kernel_build=.*/kernel_build='default'/"       "$CONF" 2>/dev/null
    sed -i 's/^force_hide_lsposed=.*/force_hide_lsposed=1/'   "$CONF" 2>/dev/null
    sed -i 's/^spoof_cmdline=.*/spoof_cmdline=1/'             "$CONF" 2>/dev/null
    log "config.sh normalised"
else
    log "config.sh missing, skipped"
fi

# ------------------------------------------------------------------ cmdline
# The bootloader reports androidboot.verifiedbootstate=orange on an unlocked
# device, while the property spoofing modules export green. Rewrite the cmdline
# so both sources agree. Done here rather than via susfs4ksu's boot-completed.sh
# because that script does not execute on this device.
if [ -x "$KSU_SUSFS" ]; then
    CUR=$(cat /proc/cmdline 2>/dev/null)
    if echo "$CUR" | grep -q 'androidboot.verifiedbootstate=orange'; then
        echo "$CUR" | sed 's|androidboot\.verifiedbootstate=orange|androidboot.verifiedbootstate=green|g' \
            > /data/adb/susfs4ksu/cmdline
        "$KSU_SUSFS" set_cmdline_or_bootconfig /data/adb/susfs4ksu/cmdline 2>/dev/null
        log "cmdline spoofed to green"
    fi
fi

# ------------------------------------------------------------------ sus_path
# /dev/scene is created by the Scene app; hiding it stops direct probes.
if [ -f "$SUSPATH" ]; then
    grep -qx '/dev/scene' "$SUSPATH" 2>/dev/null || echo '/dev/scene' >> "$SUSPATH"
    log "sus_path updated"
fi

# ------------------------------------------------------------------- keymint
# Detectors must see the real keybox, otherwise their TEE verdict is decided by
# OMK's fake chain instead of by the device. Drop them from the scoop list.
if [ -f "$OMK" ]; then
    sed -i '/com\.eltavine\.duckdetector/d' "$OMK" 2>/dev/null
    sed -i '/com\.maple\.detect/d'          "$OMK" 2>/dev/null
    log "keymint scoop pruned"
fi

# -------------------------------------------------------------- accessibility
# Scene registers an automation accessibility service, which is a standalone
# root signal independent of PackageManager visibility.
if settings get secure enabled_accessibility_services 2>/dev/null | grep -q 'com.omarea.vtools'; then
    settings put secure enabled_accessibility_services "" 2>/dev/null
    settings put secure accessibility_enabled 0 2>/dev/null
    log "scene accessibility disabled"
fi

log "post-fs-data done"
