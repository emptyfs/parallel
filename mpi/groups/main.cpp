#include <mpi.h>
#include <iostream>
#include <vector>

int main(int argc, char* argv[])
{
    double start_time, end_time;
    MPI_Init(&argc, &argv);

    int world_proc_num, world_proc_rank; // количество процессов и ранг в глобальном коммуникаторе
    MPI_Comm_rank(MPI_COMM_WORLD, &world_proc_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_proc_num);

    float numbers[2]; // две вещественных числа у нечетных процессов
    if (world_proc_rank % 2 == 1) 
    {
        numbers[0] = static_cast<float>(world_proc_rank) + 0.1;  
        numbers[1] = static_cast<float>(world_proc_rank) + 0.2;  
    }

    start_time = MPI_Wtime(); 

    MPI_Group world_group; // группа всех процессов
    MPI_Comm_group(MPI_COMM_WORLD, &world_group); 

    std::vector<int> even_ranks; // массив четных процессов
    for (int i = 0; i < world_proc_num; i += 2) 
    {
        even_ranks.push_back(i);
    }

    MPI_Group odd_group; // новая группа, исключая четные ранги
    MPI_Group_excl(world_group, even_ranks.size(), even_ranks.data(), &odd_group);

    MPI_Comm odd_comm; // новый коммуникатор на основе извелеченной группы и глобального коммуникатора
    MPI_Comm_create(MPI_COMM_WORLD, odd_group, &odd_comm);

    if (odd_comm != MPI_COMM_NULL) // если процесс принадлежит к созданному выше коммуникатору
    {
        int odd_proc_num, odd_proc_rank; // количество процессов и ранг в созданном коммуникаторе
        MPI_Comm_rank(odd_comm, &odd_proc_rank);
        MPI_Comm_size(odd_comm, &odd_proc_num);

        // массив для хранения всех чисел от всех нечетных процессов
        std::vector<float> all_numbers(2 * odd_proc_num);

        // Каждому нечетному процессу пересылаются все вещественные числа
        MPI_Allgather(numbers, 2, MPI_FLOAT, all_numbers.data(), 2, MPI_FLOAT, odd_comm);

        end_time = MPI_Wtime();
        if (world_proc_rank == 1) 
        {
            std::cout << "Total execution time: " << end_time - start_time << " seconds" << std::endl;
        }

        // Вывод чисел в порядке возрастания рангов
        std::cout << "Process " << world_proc_rank << " received numbers: ";
        for (int i = 0; i < all_numbers.size(); i += 2) 
        {
            std::cout << "(" << all_numbers[i] << ", " << all_numbers[i + 1] << ") ";
        }
        std::cout << std::endl;
    }

    // Освобождение групп и коммуникаторов
    MPI_Group_free(&odd_group);
    MPI_Group_free(&world_group);
    if (odd_comm != MPI_COMM_NULL) 
    {
        MPI_Comm_free(&odd_comm);
    }

    MPI_Finalize();

    return 0;
}