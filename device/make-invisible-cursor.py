"""Write a 1x1 fully transparent Xcursor theme ("none") so sway never shows a pointer."""
import os, struct, sys

root = sys.argv[1] if len(sys.argv) > 1 else '/storage/.icons/none'
os.makedirs(root + '/cursors', exist_ok=True)
img = struct.pack('<9I', 36, 0xfffd0002, 1, 1, 1, 1, 0, 0, 0) + struct.pack('<I', 0)
data = b'Xcur' + struct.pack('<3I', 16, 0x10000, 1) + struct.pack('<3I', 0xfffd0002, 1, 28) + img
names = ['default', 'left_ptr', 'pointer', 'hand2', 'text', 'xterm', 'wait', 'watch', 'progress',
         'crosshair', 'move', 'grabbing', 'all-scroll', 'not-allowed', 'help', 'e-resize', 'n-resize']
for n in names:
    with open(f'{root}/cursors/{n}', 'wb') as f:
        f.write(data)
with open(root + '/index.theme', 'w') as f:
    f.write('[Icon Theme]\nName=none\nComment=invisible cursor\n')
print('wrote', len(names), 'cursors to', root)
