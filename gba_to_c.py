import os

def gba_to_c_array(gba_file_name, output_file_name):
    # Get the current working directory
    current_directory = os.path.dirname(os.path.abspath(__file__))
    
    # Construct full paths for input and output files
    gba_file_path = os.path.join(current_directory, gba_file_name)
    output_file_path = os.path.join(current_directory, output_file_name)

    # Read the .gba file as binary
    with open(gba_file_path, "rb") as gba_file:
        gba_data = gba_file.read()

    gba_size = len(gba_data)

    # Write the C array to the output file
    with open(output_file_path, "w") as output_file:
        output_file.write(f"/* Contents of file {gba_file_name} */\n")
        output_file.write(f"const long int goomba_gba_size = {gba_size};\n")
        output_file.write(f"const unsigned char goomba_gba[{gba_size}] = {{\n")

        # Write the data as a series of hex values, 16 per line
        for i in range(0, gba_size, 16):
            chunk = gba_data[i:i + 16]
            hex_values = ", ".join(f"0x{byte:02X}" for byte in chunk)
            output_file.write(f"    {hex_values},\n")

        output_file.write("};\n")

# Use relative file names
gba_to_c_array("jagoombacolor.gba", "goomba.h")
