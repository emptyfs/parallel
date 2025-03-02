#include <mpi.h>
#include <iostream>
#include <vector>
#include <numeric>

int read_num_len(int, char**, int);

int main(int argc, char* argv[]) 
{
    double start_time, end_time;
    MPI_Init(&argc, &argv);

    int proc_num, proc_rank; // количество процессов и ранг процесса
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_num);    
    
    int num_len = read_num_len(argc, argv, proc_rank); // длина вектора
    const int left_elems = num_len % proc_num; // остаток от деления длины вектора на количество процессов
    const int new_len = num_len + (left_elems == 0 ? 0 : (proc_num - left_elems)); // увеличение длины вектора, чтобы остаток был равен 0
    const int chunk_len = new_len / proc_num; // длины отправляемых через scatter сегментов
    
    std::vector<int> num; // массив

    if (proc_rank == 0) 
    {
        num.resize(new_len, 0);
        std::fill(num.begin(), num.begin() + num_len, 1); // заполнение массива единицами (по исходной длине)
    }

    std::vector<int> local_chunk(chunk_len); // массив под сегмент

    start_time = MPI_Wtime(); 

    // отравление всем процессам в коммутаторе сегментов массива
    MPI_Scatter(num.data(), chunk_len, MPI_INT, local_chunk.data(), chunk_len, MPI_INT, 0, MPI_COMM_WORLD); 

    int local_sum = std::accumulate(local_chunk.begin(), local_chunk.end(), 0); // высчитывание локальной суммы

    int global_sum = 0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD); // высчитывание суммы локальных сумм, отправка ее на нулевой процесс

    end_time = MPI_Wtime();

    if (proc_rank == 0) 
    {
        std::cout << "Total execution time: " << end_time - start_time << " seconds" << std::endl;
        std::cout << "Total sum: " << global_sum << std::endl;
    }

    MPI_Finalize();
    return 0;
}

int read_num_len(int argc, char** argv, int proc_rank)
{
    int num_len;

    if (argc < 2)
    {
        num_len = 25;
    }
    else
    {
        num_len = std::atoi(argv[1]);
    }

    if (num_len < 1)
    {
        if (proc_rank == 0)
        {
            std::cerr << "Error: Array size must be greater than or equal to 1." << std::endl;
        }
        MPI_Finalize();
        exit(1);
    }

    return num_len;
}
