import os
import sys


def convert_tflite_to_cc(tflite_path, cc_path, array_name, len_name):
    with open(tflite_path, 'rb') as f:
        data = f.read()
    
    with open(cc_path, 'w') as f:
        f.write('#include <stddef.h>\n')
        f.write(f'// Generated from {os.path.basename(tflite_path)}\n')
        f.write('extern "C" {\n')
        f.write(f'extern const unsigned char {array_name}[] __attribute__((aligned(16), section(".ddr_data"))) = {{\n')
        
        for i, byte in enumerate(data):
            f.write(f'0x{byte:02x}, ')
            if (i + 1) % 12 == 0:
                f.write('\n')
        
        f.write('\n};\n')
        f.write(f'extern const unsigned int {len_name} = {len(data)};\n')
        f.write('}\n')

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <tflite_path> <cc_path> <array_name> <len_name>")
        sys.exit(1)
    
    convert_tflite_to_cc(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
