#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <random>
#include <map>
#include <vector>
#include <atomic>
#include <utility>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem> 

// Тип события: вход или выход
enum class EventType 
{
    ENTRY,
    EXIT
};

// события турникета
struct Event 
{
    int card_ID;    // идентификатор транспортной карты
    int station_ID; // идентификатор станции (номер станции)
    EventType type;// тип события: ENTRY или EXIT
    std::chrono::system_clock::time_point timestamp; // время события
};

// Потокобезопасная очередь
template <typename T>
class ThreadSafeQueue 
{
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
public:
    void push(const T&); // Добавление элемента в очередь
    void pop(T&); // Извлечение элемента из очереди. Если очередь пуста, ожидается появление элемента
    bool empty();
};

ThreadSafeQueue<Event> event_queue; // очередь событий
Event termination_event {-1, 0, EventType::ENTRY, std::chrono::system_clock::now()}; // событие для завершения работы (терминатор); Если card_ID == -1, то событие используется как сигнал завершения работы обработчика
std::atomic<bool> simulation_running{true}; // флаг для остановки симуляции

std::map<int, Event> pending_journeys; // для хранения незавершённых маршрутов (только входы)
std::mutex pending_journeys_mutex;

std::map<std::pair<int,int>, int> flow_matrix; // сводная таблица пассажиропотока между парами станций; Ключ – пара {начальная станция, конечная станция}, значение – счетчик пассажиров
std::mutex flow_matrix_mutex;

std::vector<std::string> event_log;
std::mutex log_mutex;

std::pair<int, int> read_params(int, char**);
std::chrono::milliseconds random_interval();
void update_flow_matrix(int, int);
void turnstile_simulator(int);
void event_processor();
void report_generator();
void save_log(std::string);

int main(int argc, char* argv[]) 
{
    auto [num_stations, sleep_time] = read_params(argc, argv);
    
    // потоки-симуляторы для каждой станции
    std::vector<std::thread> producer_threads;
    for (int i = 1; i <= num_stations; i++) 
        producer_threads.emplace_back(turnstile_simulator, i);
    
    // поток-обработчик событий
    std::thread consumer_thread(event_processor);
    
    // cимуляция работает заданное время
    std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
    simulation_running.store(false);
    
    // ожидание завершения потоков-симуляторов
    for (auto& t : producer_threads)
        if (t.joinable())
            t.join();
    
    // для завершения потока-обработчика отправляется терминальное событие
    event_queue.push(termination_event);
    if (consumer_thread.joinable())
        consumer_thread.join();
    
    // вывод сводной таблицы пассажиропотока
    report_generator();
    save_log("event_log.txt");
    
    return 0;
}

void save_log(std::string file_name)
{
    std::string dir = "data/";

    if (!std::filesystem::exists(dir))
    {
        std::filesystem::create_directory(dir);
    }

    file_name = dir + file_name;
    std::ofstream log_file(file_name); 

    if (log_file.is_open())
    {
        log_file << "\nDetailed Event Log:\n";
        for (const auto& entry : event_log)
            log_file << entry << std::endl;
    
        log_file.close();
    
        std::cout << "Log saved to " << file_name << std::endl;
    }
    else
    {
        std::cerr << "Error opening file " << file_name << std::endl;
    }
}

template <typename T> void ThreadSafeQueue<T>::push(const T& item) // добавление элемента в очередь
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(item);
    }
    cv.notify_one();
}
    
template <typename T> void ThreadSafeQueue<T>::pop(T & item) // извлечение элемента из очереди
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]{return !queue.empty();});
    item = queue.front();
    queue.pop();
}
    
template <typename T> bool ThreadSafeQueue<T>::empty() // проверка, что очередь пуста
{
    std::lock_guard<std::mutex> lock(mtx);
    return queue.empty();
}

// чтение количества станций и времени симуляции из командой строки
// по умолчанию 5 и 10
std::pair<int, int> read_params(int argc, char** argv)
{
    int num_stations;
    int sleep_time;

    if (argc < 2)
    {
        num_stations = 5;
        sleep_time = 10;
    }
    else
    {
        num_stations = std::atoi(argv[1]);
        sleep_time = std::atoi(argv[2]);
    }

    if (num_stations < 1 || sleep_time < 1)
    {
        std::cerr << "Error: num_stations and sleep_time must be greater than or equal to 1." << std::endl;
        exit(1);
    }

    return {num_stations, sleep_time};
}

// генерации событий турникетов
// симулирует работу турникета на конкретной станции.
void turnstile_simulator(int stationID) 
{
    static thread_local std::mt19937 generator(std::random_device{}()); // локальный генератор
    std::uniform_int_distribution<int> cardDistribution(1, 50);    // 50 возможных транспортных карт
    std::uniform_int_distribution<int> eventTypeDistribution(0, 1);  // 0 - ENTRY, 1 - EXIT
    
    while (simulation_running.load()) 
    {
        std::this_thread::sleep_for(random_interval()); // симуляция, чтобы пассажир проходил турникет в случайный момент времени
        
        Event ev;
        ev.card_ID = cardDistribution(generator);
        ev.station_ID = stationID;
        ev.timestamp = std::chrono::system_clock::now();
        int type = eventTypeDistribution(generator);
        ev.type = (type == 0) ? EventType::ENTRY : EventType::EXIT;
        
        event_queue.push(ev);
    }
}

// обработки событий
// cобытия извлекаются из очереди. При событии ENTRY данные сохраняются, при EXIT ищется соответствие и обновляется сводная таблица
void event_processor() 
{
    while (true) 
    {
        Event ev;
        event_queue.pop(ev);
        
        // Если получено терминальное событие, обработка завершается
        if (ev.card_ID == -1)
            break;
        
        std::string log_entry;

        std::time_t event_time = std::chrono::system_clock::to_time_t(ev.timestamp);
        std::tm* tm_event = std::localtime(&event_time);
        std::ostringstream time_stream;
        time_stream << std::put_time(tm_event, "%H:%M:%S"); // Формат: часы:минуты:секунды
        std::string time_string = time_stream.str();

        if (ev.type == EventType::ENTRY) 
        {
            {
                std::lock_guard<std::mutex> lock(pending_journeys_mutex);
                pending_journeys[ev.card_ID] = ev;
            }
            log_entry = "Passenger " + std::to_string(ev.card_ID) +
                       " entered station " + std::to_string(ev.station_ID) + 
                       " at time " + time_string;
        }
        else if (ev.type == EventType::EXIT) 
        {
            bool matched = false;
            {
                std::lock_guard<std::mutex> lock(pending_journeys_mutex);
                auto it = pending_journeys.find(ev.card_ID);
                if (it != pending_journeys.end()) 
                {
                    // обновление сводной таблицы: пара (входная станция, текущая станция выхода)
                    update_flow_matrix(it->second.station_ID, ev.station_ID);
                    log_entry = "Passenger " + std::to_string(ev.card_ID) +
                               " traveled from station " + std::to_string(it->second.station_ID) + 
                               " to station " + std::to_string(ev.station_ID) + 
                               " at time " + time_string;
                    pending_journeys.erase(it);
                    matched = true;
                }
            }
            if (!matched)
            {
                log_entry = "Passenger " + std::to_string(ev.card_ID) +
                           " exited station " + std::to_string(ev.station_ID) +
                           " without a recorded entry at time " + time_string;
            }
        }
        // добавление запись в лог
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            event_log.push_back(log_entry);
        }
    }
}

// оновление сводной таблицы с потокобезопасной синхронизацией
void update_flow_matrix(int entryStation, int exitStation) 
{
    std::lock_guard<std::mutex> lock(flow_matrix_mutex);
    std::pair<int, int> key = std::make_pair(entryStation, exitStation);
    flow_matrix[key]++;
}

// генерация случайного интервала ожидания 
std::chrono::milliseconds random_interval() 
{
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(100, 500); // интервал от 100 до 500 мс
    return std::chrono::milliseconds(distribution(generator));
}

// по завершении симуляции выводится сводная таблица пассажиропотока.
void report_generator() 
{
    std::lock_guard<std::mutex> lock(flow_matrix_mutex);
    std::cout << "\nWater table of passenger traffic (start station -> end station : number):\n";
    for (const auto& entry : flow_matrix) 
        std::cout << "Station " << entry.first.first << " -> Station " << entry.first.second << " : " << entry.second << std::endl;
}


