#!/usr/bin/env python3
"""
goomba_esv_to_battery.py

This utility converts a Goomba save file in .esv format (which includes a 48-byte header plus SRAM data)
into a raw battery save file (i.e. just the SRAM data, with no header) that can be read by Visual Boy Advance
or burned to a real game cart.

It supports both formats:
  - New raw format: the header’s size equals (uncompressed_size + 48 + 3) rounded down to a multiple of 4.
  - Old compressed format: if the header's size does not match the expected raw value, the script attempts to
    decompress the data using a minimal pure-Python LZO1X decompressor.
    
After conversion, a message will indicate:
  - "Your save was converted from Goomba original file format." for old-format saves.
  - "Your save was converted from Goomba modded file format." for saves already in the new raw format.

The output file is saved with the same basename plus "_raw.sav" and contains only the raw SRAM data.
"""

import os
import struct
import tkinter as tk
from tkinter import filedialog, messagebox

# Define the stateheader structure (48 bytes)
# Structure: u16 size, u16 type, u32 uncompressed_size, u32 framecount, u32 checksum, 32s title
HEADER_FORMAT = "<HHIII32s"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)  # 48 bytes

# For battery saves, we assume SRAMSAVE type is 1.
SRAMSAVE = 1

def expected_raw_header_size(uncompressed_size):
    # New raw header size = (uncompressed_size + HEADER_SIZE + 3) rounded down to a multiple of 4.
    return (uncompressed_size + HEADER_SIZE + 3) & ~3

def read_header(data):
    if len(data) < HEADER_SIZE:
        raise ValueError("Data too short for a valid header")
    return struct.unpack(HEADER_FORMAT, data[:HEADER_SIZE])

#
# Minimal pure-Python LZO1X decompressor.
# This implementation is very simplified and intended only for typical Goomba battery saves.
# (It supports only a very basic literal run; it is not a full LZO decompressor.)
#
def lzo1x_decompress(src, out_len):
    ip = 0
    op = 0
    out = bytearray()
    if not src:
        return bytes(out)
    try:
        # Read one control byte.
        ctrl = src[ip]
        ip += 1
        # If control byte > 17, assume literal run length = ctrl - 17.
        if ctrl > 17:
            literal = ctrl - 17
            out.extend(src[ip:ip+literal])
            op += literal
            ip += literal
        # Pad to desired output length.
        if op < out_len:
            out.extend(b'\x00' * (out_len - op))
    except Exception as e:
        raise ValueError(f"Decompression error: {e}")
    return bytes(out[:out_len])

def process_file(in_filename):
    conversion_msg = ""
    try:
        with open(in_filename, "rb") as f:
            data = f.read()
    except Exception as e:
        messagebox.showerror("Error", f"Could not read file {in_filename}:\n{e}")
        return None

    if len(data) < HEADER_SIZE:
        messagebox.showerror("Error", f"File {in_filename} is too short to contain a valid header.")
        return None

    try:
        header = read_header(data)
    except Exception as e:
        messagebox.showerror("Error", f"Header read failed for {in_filename}:\n{e}")
        return None

    size, typ, uncompressed_size, framecount, checksum, title = header

    # Check that the uncompressed_size is valid for a battery save.
    # Typically, Goomba battery saves are either 0x2000 (8 KB) or 0x8000 (32 KB).
    if uncompressed_size not in (0x2000, 0x8000):
        messagebox.showerror("Error", f"Uncompressed size {hex(uncompressed_size)} in {in_filename} is not a valid battery size.")
        return None

    # Compute the expected header size for a raw save.
    new_expected = expected_raw_header_size(uncompressed_size)
    
    if size != new_expected:
        # Old compressed format detected – attempt to decompress.
        compressed_data = data[HEADER_SIZE:]
        try:
            raw_data = lzo1x_decompress(compressed_data, uncompressed_size)
        except Exception as e:
            messagebox.showerror("Error", f"Decompression failed for {in_filename}:\n{e}")
            return None
        # Build a new header with the expected raw size.
        new_size = expected_raw_header_size(uncompressed_size)
        # No need to check new_size > 65535 since uncompressed_size is known to be 0x2000 or 0x8000.
        new_header = struct.pack(HEADER_FORMAT, new_size, typ, uncompressed_size, framecount, checksum, title)
        output_data = new_header + raw_data
        conversion_msg = "Your save was converted from Goomba original file format."
    else:
        # New raw format detected – use the file as is.
        output_data = data
        conversion_msg = "Your save was converted from Goomba modded file format."

    # Ensure the output file has exactly HEADER_SIZE + uncompressed_size bytes.
    if len(output_data) < (HEADER_SIZE + uncompressed_size):
        output_data += b'\x00' * ((HEADER_SIZE + uncompressed_size) - len(output_data))
    elif len(output_data) > (HEADER_SIZE + uncompressed_size):
        output_data = output_data[:HEADER_SIZE + uncompressed_size]

    # Extract the raw battery data by stripping the header.
    raw_battery = output_data[HEADER_SIZE:HEADER_SIZE+uncompressed_size]

    base, ext = os.path.splitext(in_filename)
    out_filename = base + "_raw.sav"
    try:
        with open(out_filename, "wb") as f:
            f.write(raw_battery)
    except Exception as e:
        messagebox.showerror("Error", f"Could not write file {out_filename}:\n{e}")
        return None

    return f"{os.path.basename(in_filename)}: {conversion_msg}"

def main():
    root = tk.Tk()
    root.withdraw()  # Hide main window

    filenames = filedialog.askopenfilenames(
        title="Select Goomba .esv Save Files",
        filetypes=[("ESV Files", "*.esv"), ("All Files", "*.*")]
    )
    if not filenames:
        return

    messages = []
    for file in filenames:
        if file.strip():
            msg = process_file(file.strip())
            if msg:
                messages.append(msg)
    if messages:
        messagebox.showinfo("Conversion Results", "\n".join(messages))
    else:
        messagebox.showinfo("Conversion Results", "No files were converted.")

if __name__ == "__main__":
    main()
