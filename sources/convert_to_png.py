"""
Converts all image files in the asset sources directory to png to be consumed by b7enc AND DELETES ORIGINAL SOURCE FILES!
"""

import os, sys
from PIL import Image

extensions = (".jpg", ".jpeg", ".tga", ".bmp")

walk_dir = sys.argv[1]

for root, sub_dirs, files in os.walk(walk_dir):
    for file_name in files:
        if not file_name.endswith(extensions):
            continue

        in_file = os.path.join(root, file_name)

        f, e = os.path.splitext(in_file)
        out_file = f + ".png"

        try:
            with Image.open(in_file) as im:
                im.save(out_file)
                print("Converted", in_file, "to", out_file)
        except OSError:
            print("Failed to convert", in_file)

        try:
            os.remove(in_file)
        except OSError:
            print("Failed to delete", in_file)
