/*
 * Parallel Merge Sort menggunakan MPI
 * =====================================
 * Strategi:
 *   1. Root (Rank 0) membaca file, mencatat waktu, lalu Scatter data ke semua proses.
 *   2. Setiap proses mengurutkan porsinya dengan qsort() secara independen.
 *   3. Root mengumpulkan (Gather) semua porsi yang sudah terurut.
 *   4. Root melakukan k-way merge untuk menggabungkan hasil akhir.
 *   5. Root mencetak 10 elemen pertama & terakhir, lalu mencatat waktu selesai.
 *
 * Kompilasi : mpicc -O2 -o parallel_mergesort parallel_mergesort.c
 * Jalankan  : mpirun -np <N> ./parallel_mergesort <input_file>
 *             Contoh: mpirun -np 4 ./parallel_mergesort data_10000.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

/* ------------------------------------------------------------------ */
/*  Fungsi Pembantu                                                    */
/* ------------------------------------------------------------------ */

/* Komparator untuk qsort() — urutan naik (ascending) */
int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/*
 * merge_sorted_parts()
 * Menggabungkan 'num_parts' sub-array yang sudah terurut menjadi satu
 * array terurut. Setiap sub-array berukuran 'part_size', kecuali yang
 * terakhir bisa berukuran 'last_part_size'.
 *
 * Algoritme: k-way merge dengan pointer indeks per partisi.
 */
void merge_sorted_parts(double *data, int total, int num_parts,
                        int part_size, int last_part_size,
                        double *output) {
    /* Alokasi array indeks (penunjuk posisi saat ini di tiap partisi) */
    int *idx = (int *)calloc(num_parts, sizeof(int));

    /* Hitung batas atas tiap partisi */
    int *limit = (int *)malloc(num_parts * sizeof(int));
    for (int p = 0; p < num_parts - 1; p++) {
        limit[p] = part_size;
    }
    limit[num_parts - 1] = last_part_size;

    /* Pointer awal tiap partisi di dalam array 'data' */
    double **parts = (double **)malloc(num_parts * sizeof(double *));
    for (int p = 0; p < num_parts; p++) {
        parts[p] = data + (size_t)p * part_size;
    }

    /* Isi output satu per satu dengan nilai minimum dari semua partisi */
    for (int i = 0; i < total; i++) {
        int min_part = -1;
        double min_val = 0.0;

        for (int p = 0; p < num_parts; p++) {
            if (idx[p] < limit[p]) {
                double val = parts[p][idx[p]];
                if (min_part == -1 || val < min_val) {
                    min_val = val;
                    min_part = p;
                }
            }
        }
        output[i] = min_val;
        idx[min_part]++;
    }

    free(idx);
    free(limit);
    free(parts);
}

/* ------------------------------------------------------------------ */
/*  main()                                                             */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Pastikan file input diberikan */
    if (argc < 2) {
        if (rank == 0)
            fprintf(stderr, "Penggunaan: mpirun -np <N> %s <input_file>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    /* ---------------------------------------------------------------- */
    /*  TAHAP 1: Root membaca data dari file                            */
    /* ---------------------------------------------------------------- */
    int  total_data = 0;
    double *data_global = NULL;
    double t_start = 0.0;

    if (rank == 0) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            fprintf(stderr, "[Root] Gagal membuka file: %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        /* Hitung jumlah baris terlebih dahulu */
        double tmp;
        while (fscanf(fp, "%lf", &tmp) == 1) total_data++;
        rewind(fp);

        data_global = (double *)malloc(total_data * sizeof(double));
        for (int i = 0; i < total_data; i++)
            fscanf(fp, "%lf", &data_global[i]);
        fclose(fp);

        printf("=====================================================\n");
        printf("  Parallel Merge Sort — MPI\n");
        printf("  File        : %s\n", argv[1]);
        printf("  Total data  : %d elemen\n", total_data);
        printf("  Proses aktif: %d\n", size);
        printf("=====================================================\n");

        /* Catat waktu mulai setelah I/O selesai (ukur murni komputasi) */
        t_start = MPI_Wtime();
    }

    /* ---------------------------------------------------------------- */
    /*  TAHAP 2: Broadcast total_data agar semua proses tahu ukurannya  */
    /* ---------------------------------------------------------------- */
    MPI_Bcast(&total_data, 1, MPI_INT, 0, MPI_COMM_WORLD);

    /* Hitung ukuran porsi tiap proses */
    int base_size = total_data / size;   /* bagian rata */
    int remainder = total_data % size;   /* sisa (diberikan ke proses terakhir) */

    /* Ukuran porsi proses ini */
    int my_size = base_size + (rank == size - 1 ? remainder : 0);

    /* ---------------------------------------------------------------- */
    /*  TAHAP 3: Scatter — bagikan data ke semua proses                 */
    /* ---------------------------------------------------------------- */
    /*
     * MPI_Scatter standar mengharuskan setiap proses menerima jumlah
     * yang sama. Karena ada kemungkinan sisa (remainder), kita pakai
     * MPI_Scatterv agar bisa mengirim jumlah berbeda ke proses terakhir.
     */
    int    *sendcounts = NULL;
    int    *displs     = NULL;
    double *my_data    = (double *)malloc(my_size * sizeof(double));

    if (rank == 0) {
        sendcounts = (int *)malloc(size * sizeof(int));
        displs     = (int *)malloc(size * sizeof(int));
        int offset = 0;
        for (int p = 0; p < size; p++) {
            sendcounts[p] = base_size + (p == size - 1 ? remainder : 0);
            displs[p]     = offset;
            offset       += sendcounts[p];
        }
    }

    MPI_Scatterv(data_global, sendcounts, displs, MPI_DOUBLE,
                 my_data, my_size, MPI_DOUBLE,
                 0, MPI_COMM_WORLD);

    /* ---------------------------------------------------------------- */
    /*  TAHAP 4: Komputasi Paralel — setiap proses mengurutkan porsinya */
    /* ---------------------------------------------------------------- */
    qsort(my_data, my_size, sizeof(double), cmp_double);

    printf("[Rank %d] Selesai mengurutkan %d elemen.\n", rank, my_size);

    /* ---------------------------------------------------------------- */
    /*  TAHAP 5: Gather — kumpulkan semua porsi yang sudah terurut      */
    /* ---------------------------------------------------------------- */
    /*
     * Serupa dengan Scatter, kita gunakan MPI_Gatherv karena ukuran
     * setiap porsi bisa berbeda.
     */
    MPI_Gatherv(my_data, my_size, MPI_DOUBLE,
                data_global, sendcounts, displs, MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    /* ---------------------------------------------------------------- */
    /*  TAHAP 6: Root melakukan k-way merge final                       */
    /* ---------------------------------------------------------------- */
    if (rank == 0) {
        double *sorted = (double *)malloc(total_data * sizeof(double));

        int last_size = base_size + remainder;  /* ukuran porsi proses terakhir */
        merge_sorted_parts(data_global, total_data, size,
                           base_size, last_size, sorted);

        double t_end = MPI_Wtime();

        /* ---- Validasi: cetak 10 pertama dan 10 terakhir ---- */
        printf("\n--- VALIDASI HASIL ---\n");
        printf("10 elemen PERTAMA:\n  ");
        for (int i = 0; i < 10 && i < total_data; i++)
            printf("%.4f ", sorted[i]);
        printf("\n");

        printf("10 elemen TERAKHIR:\n  ");
        for (int i = total_data - 10; i < total_data; i++)
            printf("%.4f ", sorted[i]);
        printf("\n");

        /* Verifikasi cepat: pastikan terurut */
        int valid = 1;
        for (int i = 1; i < total_data; i++) {
            if (sorted[i] < sorted[i - 1]) { valid = 0; break; }
        }
        printf("Status urutan : %s\n", valid ? "VALID (ascending)" : "ERROR — tidak terurut!");

        printf("\nWaktu eksekusi (komputasi): %.6f detik\n", t_end - t_start);
        printf("=====================================================\n");

        free(sorted);
        free(sendcounts);
        free(displs);
        free(data_global);
    }

    free(my_data);
    MPI_Finalize();
    return 0;
}
