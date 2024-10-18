#include <mpi.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm> 
#include <random> 
#include <unordered_map>  

void plus(std::vector<int>&, int, int);
std::vector<int> get_fragments(int);

int main(int argc, char* argv[]) 
{
    double start_time, end_time;
    int proc_num, proc_rank;

    MPI_Init(&argc, &argv);

    MPI_Status Status;

    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_num); 

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
        return 1;
    }

    std::vector<int> num;  // массив 
    std::vector<int> fragments = get_fragments(num_len); // массив вида ([num_len/2], [num_len/4],.. , 1) (длины фрагментов)
    const int needed_proc = fragments.size();
    const int working_procs = std::min(needed_proc, proc_num-1); // процессы, учавствующие в вычислениях

    if (proc_rank == 0) 
    {
        num.resize(num_len);
        // Начальный массив от 0 до num_len
        std::iota(num.begin(), num.end(), 0);

        // Массив номеров процессов от 1 до working_procs
        std::vector<int> proc_ind(working_procs);
        std::iota(proc_ind.begin(), proc_ind.end(), 1);
        // Перемешивание массива, mt19937 - генератор случайных чисел, std::random_device{}() - случайный сид для генератора
        std::shuffle(proc_ind.begin(), proc_ind.end(), std::mt19937{std::random_device{}()});

        int offset = 0; // смещение для фрагмента
        start_time = MPI_Wtime(); 

        for (int i = 0; i < working_procs; i++) 
        {
            int dest = proc_ind[i]; // какому процессу отправлять
            int chunk_len = fragments[i]; // длина фрагмента

            MPI_Send(&chunk_len, 1, MPI_INT, dest, 0, MPI_COMM_WORLD);  // размер фрагмента
            MPI_Send(&num[offset], chunk_len, MPI_INT, dest, 0, MPI_COMM_WORLD);  // фрагмент

            offset += chunk_len;  // Смещение на размер фрагмента
        }

        if (offset < num_len) // если процессов не хватило
        {
            plus(num, offset, num_len);
        }

        std::unordered_map<int, int> hash_table; // мапа вида <размер массива> : <смещение>
        int cur = 0;
        for (int i = 0; i < needed_proc; i++) 
        {
            hash_table[fragments[i]] = cur;
            cur += fragments[i];
        }

        for (int i = 0; i < working_procs; i++) 
        {
            int chunk_len;
            // получение размера фрагмента через джокер
            MPI_Recv(&chunk_len, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &Status);
            std::vector<int> local_chunk(chunk_len);
            int dest = Status.MPI_SOURCE;
            offset = hash_table[chunk_len];
            // получение фрагмента
            MPI_Recv(local_chunk.data(), chunk_len, MPI_INT, dest, MPI_ANY_TAG, MPI_COMM_WORLD, &Status);

            // Преобразованные данные помещаются в исходную последовательность
            std::copy(local_chunk.begin(), local_chunk.end(), num.begin() + offset);
        }
        end_time = MPI_Wtime();
        std::cout << "Total execution time: " << end_time - start_time << " seconds" << std::endl;

        std::cout << "Sending sequence: ";
        for (int el : proc_ind) 
        {
            std::cout << el << " ";
        }
        std::cout << std::endl;

        std::cout << "Processed array: ";
        for (int el : num) 
        {
            std::cout << el << " ";
        }
        std::cout << std::endl;
    } 
    
    else if (proc_rank <= working_procs)
    {
        int chunk_len;
        MPI_Recv(&chunk_len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &Status);  // размер фрагмента
        std::vector<int> local_chunk(chunk_len);
        MPI_Recv(local_chunk.data(), chunk_len, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &Status);  // фрагмент

        // Обрабатка фрагмента
        plus(local_chunk, 0, chunk_len);
        MPI_Send(&chunk_len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD); // размер фрагмента
        MPI_Send(local_chunk.data(), chunk_len, MPI_INT, 0, 0, MPI_COMM_WORLD); // фрагмент
    }

    MPI_Finalize();
    return 0;
}

std::vector<int> get_fragments(int num_len)
{
    std::vector<int> vec;
    int cur_len = num_len >> 1; // деление на 2 с округлением в меньшую сторону 

    while (cur_len > 0)
    {
        vec.push_back(cur_len);
        cur_len = cur_len >> 1;
    }

    return vec;
}

void plus(std::vector<int>& num, int start, int finish) 
{
    for (start; start<finish; start++)
    {
        num[start]+=1;
    }
}