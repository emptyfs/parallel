#include <mpi.h>
#include <iostream>
#include <vector>
#include <cstdlib>

int sum(const std::vector<int>& num);

int main(int argc, char* argv[]) 
{
    double start_time, end_time;

    MPI_Init(&argc, &argv);

    int proc_num, proc_rank;
    MPI_Status Status;

    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_num);

    int num_len;

    if (argc < 2) 
    {
        num_len = 1000000;
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

    const int working_procs = std::min(proc_num - 1, num_len);
    const int chunk_len = (working_procs > 0) ? (num_len / working_procs) : num_len; 
    const int left_elems = num_len - (working_procs - 1) * chunk_len;

    std::vector<int> num;  
    std::vector<int> local_chunk;

    if (proc_rank == 0) 
    {
        num.resize(num_len);
        int total_sum = 0;

        for (int i = 0; i < num_len; i++) 
        {
            num[i] = 1;
        }

        start_time = MPI_Wtime(); 

        if (proc_num == 1)
        {
            total_sum = sum(num);
        }
        else
        {
            int i;
            for (i = 1; i < working_procs; i++) 
            {
                MPI_Send(&num[(i - 1) * chunk_len], chunk_len, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
            MPI_Send(&num[(i - 1) * chunk_len], left_elems, MPI_INT, i, 0, MPI_COMM_WORLD);

            for (int i = 1; i <= working_procs; i++) 
            {
                int local_sum = 0;
                MPI_Recv(&local_sum, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &Status);
                total_sum += local_sum;
            }
        }

        end_time = MPI_Wtime();
        std::cout << "Total sum: " << total_sum << std::endl;
        std::cout << "Total execution time: " << end_time - start_time << " seconds";

    } 
    else if (proc_rank <= working_procs) 
    {
        if (proc_rank == working_procs)
        {
            local_chunk.resize(left_elems);
            MPI_Recv(local_chunk.data(), left_elems, MPI_INT, 0, 0, MPI_COMM_WORLD, &Status);
        } 
        else
        {
            local_chunk.resize(chunk_len);  
            MPI_Recv(local_chunk.data(), chunk_len, MPI_INT, 0, 0, MPI_COMM_WORLD, &Status);
        }
        int local_sum = sum(local_chunk);
        MPI_Send(&local_sum, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}

int sum(const std::vector<int>& num) 
{
    int sum = 0;
    for (int i = 0; i < num.size(); i++) 
    {
        sum += num[i];
    }
    return sum;
}
