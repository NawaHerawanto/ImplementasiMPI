#!/bin/bash
# run_benchmark.sh
# Menjalankan semua skenario pengujian secara otomatis dan merangkum hasilnya.
#
# Prasyarat:
#   mpicc dan mpirun tersedia (OpenMPI atau MPICH)
#   Python 3 tersedia untuk generate_data.py
#
# Penggunaan: bash run_benchmark.sh

set -e

echo "========================================"
echo "  BENCHMARK Parallel Merge Sort (MPI)"
echo "========================================"

# --- 1. Generate data ---
echo ""
echo "[1] Membuat file data..."
python3 generate_data.py

# --- 2. Kompilasi ---
echo ""
echo "[2] Mengompilasi parallel_mergesort.c ..."
mpicc -O2 -o parallel_mergesort parallel_mergesort.c
echo "    => binary: ./parallel_mergesort"

# --- 3. Jalankan semua kombinasi ---
echo ""
echo "[3] Menjalankan benchmark..."
echo ""
echo "| Dataset         | Proses (np) | Waktu (detik)              |"
echo "|-----------------|-------------|----------------------------|"

FILES=("data_10000.txt" "data_100000.txt" "data_1000000.txt")
NPS=(1 2 4 8)

for FILE in "${FILES[@]}"; do
    for NP in "${NPS[@]}"; do
        # Jalankan dan ambil baris waktu eksekusi
        OUTPUT=$(mpirun --oversubscribe -np "$NP" ./parallel_mergesort "$FILE" 2>/dev/null)
        WAKTU=$(echo "$OUTPUT" | grep "Waktu eksekusi" | awk '{print $NF}')
        printf "| %-15s | np=%-2d       | %-26s |\n" "$FILE" "$NP" "$WAKTU detik"
    done
done

echo ""
echo "Benchmark selesai."
