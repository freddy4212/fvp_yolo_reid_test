import os
import sys


def convert_to_c_array(input_path, output_path, array_name):
    with open(input_path, 'rb') as f:
        data = f.read()
    
    with open(output_path, 'w') as f:
        f.write(f'#include <stddef.h>\n')
        f.write(f'// Generated from {os.path.basename(input_path)}\n')
        f.write(f'extern "C" {{\n')
        f.write(f'extern const unsigned char {array_name}[] __attribute__((aligned(16), section(".ddr_data"))) = {{\n')
        
        for i, byte in enumerate(data):
            f.write(f'0x{byte:02x}, ')
            if (i + 1) % 12 == 0:
                f.write('\n')
        
        f.write(f'\n}};\n')
        f.write(f'extern const unsigned int {array_name}_len = {len(data)};\n')
        f.write(f'}}\n')

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python tflite_to_cc.py <input_tflite> <output_cc> <array_name>")
        sys.exit(1)
    
    convert_to_c_array(sys.argv[1], sys.argv[2], sys.argv[3])
