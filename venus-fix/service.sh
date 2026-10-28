#!/system/bin/sh
# Late pass. Two things can undo the post-fs-data work:
#   - the accessibility manager re-enables a service once it starts
#   - anything that re-reads the boot cmdline before our spoof lands
# Re-assert both.
MODDIR=${0%/*}
KSU_SUSFS=/data/adb/ksu/bin/ksu_susfs

log() { echo "venus_fix: $*" >> /data/adb/venus_fix.log; }

sleep 20

if [ -x "$KSU_SUSFS" ]; then
    if ! cat /proc/cmdline 2>/dev/null | grep -q 'androidboot.verifiedbootstate=green'; then
        CUR=$(cat /proc/cmdline 2>/dev/null)
        if echo "$CUR" | grep -q 'androidboot.verifiedbootstate=orange'; then
            echo "$CUR" | sed 's|androidboot\.verifiedbootstate=orange|androidboot.verifiedbootstate=green|g' \
                > /data/adb/susfs4ksu/cmdline
            "$KSU_SUSFS" set_cmdline_or_bootconfig /data/adb/susfs4ksu/cmdline 2>/dev/null
            log "cmdline re-spoofed at service stage"
        fi
    fi
fi

if settings get secure enabled_accessibility_services 2>/dev/null | grep -q 'com.omarea.vtools'; then
    settings put secure enabled_accessibility_services "" 2>/dev/null
    settings put secure accessibility_enabled 0 2>/dev/null
    log "scene accessibility re-disabled at service stage"
fi

log "service done"
