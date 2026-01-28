import argparse
import math
import sys

def generate_trace(mat_rows, mat_cols, num_bank_groups, output_file):
   
    BYTES_PER_COMP = 0x20  # 32 bytes = 256 bits per COMP
    COMP_INCREMENT = 0x40
    BANKS_PER_BG = 4
    GLOBAL_BUFFER_SIZE_BYTES = 1024 # 512 elements * 2 bytes/element (e.g., FP16)
    CHUNK_SIZE_ELEMENTS = 512
    
    ROW_ADDRESS_INCREMENT = 0x800
    BANK_GROUP_ADDRESS_INCREMENT = 0x08000000
    CHUNK_NUMBER_INCREMENT = 0x00080000
    
    #Calculate Column Chunking (The Outer Loop)
    num_chunks = math.ceil(mat_cols / CHUNK_SIZE_ELEMENTS)
    
    rows_per_pass = num_bank_groups * BANKS_PER_BG
    num_row_passes = math.ceil(mat_rows / rows_per_pass)
    
    # Since each COMP instruction takes 256 bits, hence to fully consume 1KB we need 32 Comps.
    comps_per_pass = 32

    print(f"Configuration:")
    print(f"  - Matrix: {mat_rows} rows x {mat_cols} columns")
    print(f"  - Bank Groups: {num_bank_groups} ({rows_per_pass} banks in parallel)")
    print(f"  - Global Buffer Size: {GLOBAL_BUFFER_SIZE_BYTES} bytes")
    print(f"  - Column Chunks Needed: {num_chunks}")
    print(f"  - Row Passes per Chunk: {num_row_passes}")
    print(f"  - Total Passes (G_ACT phases): {num_chunks * num_row_passes}\n")

    f = open(output_file, 'w') if output_file else sys.stdout
    
    try:
        current_address = 0
        
        for chunk_idx in range(num_chunks):
            
            gwrite_address = chunk_idx * ROW_ADDRESS_INCREMENT 
            f.write(f"0x{gwrite_address:08x} GWRITE\n")
            
            base_chunk_address = chunk_idx * CHUNK_NUMBER_INCREMENT
            for pass_idx in range(num_row_passes):
                
                base_row_address = pass_idx * ROW_ADDRESS_INCREMENT
                
                for j in range(num_bank_groups):
                    act_address = j * BANK_GROUP_ADDRESS_INCREMENT + base_row_address + base_chunk_address
                    f.write(f"0x{act_address:08x} G_ACT{j}\n")
                
                for k in range(comps_per_pass):
                    comp_address = base_chunk_address + base_row_address + k * COMP_INCREMENT
                    f.write(f"0x{comp_address:08x} COMP\n")
                

                if(chunk_idx == num_chunks - 1 and pass_idx == num_row_passes - 1):
                    f.write("READRES")
                else:
                    f.write("READRES\n")
            
    finally:
        if output_file:
            f.close()
            print(f"Trace successfully written to {output_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a Newton-aware PIM memory trace.")
    parser.add_argument("--rows", type=int, required=True, help="Number of rows in the weight matrix.")
    parser.add_argument("--cols", type=int, required=True, help="Total bytes per row in the weight matrix.")
    parser.add_argument("--bank-groups", type=int, required=True, help="Number of bank groups (e.g., 4 or 8).")
    parser.add_argument("--output", type=str, help="Name of the output file.")
    
    args = parser.parse_args()
    
    generate_trace(args.rows, args.cols, args.bank_groups, args.output)