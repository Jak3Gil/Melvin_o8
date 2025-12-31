# SD Card Setup for brain.m

## Current Setup

**SD Card Location:** `/Volumes/512GB/Melvin/brain.m`
**Format:** exFAT
**Available Space:** 476GB+
**Symlink:** `brain.m` → `/Volumes/512GB/Melvin/brain.m`

## File System: exFAT

✅ **Pros:**
- No file size limit (brain.m can grow to 476GB!)
- Cross-platform compatible (Mac/Windows/Linux)
- Good performance
- No 4GB file size restrictions

✅ **Perfect for:** Large brain.m files that will grow with training

## Using brain.m from SD Card

All programs automatically use the SD card location via the symlink:

```bash
# All these commands work as normal
./test/test_dataset_port dataset.txt
./test/test_production dataset.txt

# The brain.m symlink automatically points to SD card
```

## Manual Access

To access brain.m directly:

```bash
# Direct access
ls -lh /Volumes/512GB/Melvin/brain.m

# Copy to local (for backup)
cp /Volumes/512GB/Melvin/brain.m ./brain.m.backup

# Copy from backup
cp ./brain.m.backup /Volumes/512GB/Melvin/brain.m
```

## Monitoring SD Card Space

```bash
# Check available space
df -h /Volumes/512GB

# Check brain.m size
ls -lh /Volumes/512GB/Melvin/brain.m

# Monitor growth
watch -n 5 'ls -lh /Volumes/512GB/Melvin/brain.m && df -h /Volumes/512GB | tail -1'
```

## Backup Recommendations

Even though brain.m is on SD card, still back up regularly:

```bash
# Create timestamped backup
cp /Volumes/512GB/Melvin/brain.m ~/brain_backups/brain_$(date +%Y%m%d_%H%M%S).m

# Or use rsync for incremental backups
rsync -av /Volumes/512GB/Melvin/brain.m ~/brain_backups/
```

## Troubleshooting

### SD Card Not Mounted
If `/Volumes/512GB` doesn't exist:
1. Eject and reinsert SD card
2. Check Disk Utility for SD card
3. Manually mount if needed

### Permission Issues
```bash
# Check permissions
ls -la /Volumes/512GB/Melvin/

# exFAT supports standard Unix permissions
chmod 644 /Volumes/512GB/Melvin/brain.m
```

### Symlink Broken
If symlink breaks (SD card unmounted):
```bash
# Remove broken symlink
rm brain.m

# Recreate when SD card is mounted
ln -s /Volumes/512GB/Melvin/brain.m brain.m
```

## Performance Notes

- SD card may be slightly slower than internal SSD for very large file operations
- exFAT provides good performance for large files
- For intensive training, consider:
  - Using larger chunk sizes to reduce I/O frequency
  - Less frequent auto-saves during training
  - Monitoring SD card performance

## Capacity Planning

With 476GB available space:
- Current brain.m: ~244KB
- Can grow to: 476GB (unlimited within card capacity)
- No file size restrictions (exFAT allows files up to disk capacity)

Ready for large-scale training! 🚀

