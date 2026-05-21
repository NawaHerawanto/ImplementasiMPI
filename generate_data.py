#!/usr/bin/env python3
"""
generate_data.py
Membuat file teks berisi angka pecahan acak untuk pengujian Parallel Merge Sort.
Penggunaan: python3 generate_data.py
"""

import random
import os

def generate_file(filename, count, lo=0.0, hi=1_000_000.0):
    print(f"Membuat {filename} ({count:,} angka)...", end=" ", flush=True)
    with open(filename, "w") as f:
        for _ in range(count):
            f.write(f"{random.uniform(lo, hi):.6f}\n")
    size_mb = os.path.getsize(filename) / (1024 * 1024)
    print(f"selesai ({size_mb:.1f} MB)")

if __name__ == "__main__":
    random.seed(42)
    generate_file("data_10000.txt",    10_000)
    generate_file("data_100000.txt",  100_000)
    generate_file("data_1000000.txt", 1_000_000)
    print("\nSemua file berhasil dibuat.")
