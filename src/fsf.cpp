
#include <iostream>
#include <chrono>
#include <ctime>
#include <thread>
#include <vector>
#include <functional>
#include <utility>
#include <iterator>
#include <filesystem>

#include <opencv2/opencv.hpp>

enum class LOG
{
    ERROR,
    INFO,
    OK,
    WARN
};

void log(LOG level, std::string info)
{
    std::time_t t = std::time(0);
    std::tm *now = std::localtime(&t);

    std::cout << "[" << now->tm_hour << ":" << now->tm_min << ":" << now->tm_min << "]";

    switch (level)
    {
    case LOG::ERROR:
        std::cout << "[ ERROR ]";
        break;
    case LOG::INFO:
        std::cout << "[ INFO  ]";
        break;
    case LOG::OK:
        std::cout << "[  OK   ]";
        break;
    case LOG::WARN:
        std::cout << "[ WARN  ]";
        break;
    default:
        std::cout << "[ how tf]";
        break;
    }

    std::cout << ": " << info << "\n";
}

void show_usage()
{
    std::cerr << "Usage:\n"
              << "\t-h,--help\t\t\tShow this message\n"
              << "\t-i,--input <path>\t\tSource to process\n"
              << "\t-d,--destination <path>\t\tDestination location\n"
              << "\t-t,--threshold <0.0 - 1.0>\tThreshold value\n"
              << "\t-m,--mode <0 - 1>\t\tProcessing mode [default - 0]\n"
              << "\t-w,--workers <1 - ?>\t\tThreads to use [default - 1]" 
              << std::endl;
}

class JobManager
{
public:
    JobManager(int jobs)
    {
        this->jobs = jobs;
        finishedJobs.reserve(jobs);
    }

    void write(int id, std::vector<cv::Mat> *done)
    {
        std::lock_guard<std::mutex> lock(_writerMutex);

        finishedJobs.push_back(std::make_pair(id, *done));
    }

    bool getFinished(std::vector<cv::Mat> *output)
    {
        std::cout<<"D: "<<finishedJobs.size()<<" "<<jobs<<std::endl;
        if (finishedJobs.size() != jobs)
        {
            log(LOG::ERROR, "There aren't enough finished jobs as requested jobs.");
            return false;
        }

        std::sort(finishedJobs.begin(), finishedJobs.end(), [](auto &left, auto &right) {
            return left.first < right.first;
        });

        for (auto job : finishedJobs)
        {
            std::copy(job.second.begin(), job.second.end(), std::back_inserter(*output));
        }

        return true;
    }

private:
    std::vector<std::pair<int, std::vector<cv::Mat>>> finishedJobs;
    std::mutex _writerMutex;

    int jobs;
};

double framePixelSum(cv::Mat &theFrame)
{
    unsigned long theSum = 0;

    for (int x = 0; x < theFrame.rows; x++)
    {
        cv::Vec3b *pixel = theFrame.ptr<cv::Vec3b>(x);

        for (int y = 0; y < theFrame.cols; y++)
            theSum += pixel[y][2] + pixel[y][1] + pixel[y][0];
    }

    return ((float)theSum / (theFrame.rows * theFrame.cols));
}

void work(JobManager *manager, cv::VideoCapture _cap, int id, int job_size, int threshold)
{
    cv::VideoCapture cap = _cap;

    std::cout<<id<<" "<<id * job_size<<std::endl;

    //cap.set(cv::CAP_PROP_POS_FRAMES, id * job_size);

    std::vector<cv::Mat> passed;

    auto start = std::chrono::high_resolution_clock::now();
    int passedFrames = 0;

    double prevFrameSum = 0;

    log(LOG::INFO, "Worker -" + std::to_string(id) + "- started.");

    for (int i = 0; i < job_size; i++)
    {
        cv::Mat frame;
        cap >> frame;

        if (frame.empty())
            break;

        double thisFrameSum = framePixelSum(frame);
        if (!(prevFrameSum - threshold < thisFrameSum && prevFrameSum + threshold > thisFrameSum))
        {
            passed.push_back(frame);
            passedFrames++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    log(LOG::INFO, "Worker -" + std::to_string(id) + "- finished in " + std::to_string(std::chrono::duration<float>(end - start).count()) + " seconds with " + std::to_string(passedFrames) + " passed frames.");

    manager->write(id, &passed);
}

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        show_usage();
        return 1;
    }

    std::vector<std::string> args;

    std::string input, destination;
    int mode = 0, workers = 1;
    double threshold = -1;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if ((arg == "-h") || (arg == "--help"))
        {
            show_usage();
            return 0;
        }
        else if ((arg == "-i") || (arg == "--input"))
        {
            if (i + 1 < argc)
            {
                input = argv[++i];
            }
            else
            {
                std::cerr << "--input parameter value is missing." << std::endl;
                return 1;
            }
        }
        else if ((arg == "-d") || (arg == "--destination"))
        {
            if (i + 1 < argc)
            {
                destination = argv[++i];
            }
            else
            {
                std::cerr << "--destination parameter value is missing." << std::endl;
                return 1;
            }
        }
        else if ((arg == "-t") || (arg == "--threshold"))
        {
            if (i + 1 < argc)
            {
                threshold = std::atof((argv[++i]));

                if (threshold < 0.0 || threshold > 1.0)
                {
                    std::cerr << "--threshold parameter value is invalid!" << std::endl;
                    return 1;
                }
            }
            else
            {
                std::cerr << "--threshold parameter value is missing." << std::endl;
                return 1;
            }
        }
        else if ((arg == "-m") || (arg == "--mode"))
        {
            if (i + 1 < argc)
            {
                mode = std::atoi(argv[++i]);

                if (mode < 0 || mode > 1)
                {
                    std::cerr << "--workers parameter value is invalid!" << std::endl;
                    return 1;
                }
            }
            else
            {
                std::cerr << "--mode parameter value is missing." << std::endl;
                return 1;
            }
        }
        else if ((arg == "-w") || (arg == "--workers"))
        {
            if (i + 1 < argc)
            {
                workers = std::atoi(argv[++i]);

                if (workers < 1)
                {
                    std::cerr << "--workers parameter value is invalid!" << std::endl;
                    return 1;
                }
            }
            else
            {
                std::cerr << "--workers parameter value is missing." << std::endl;
                return 1;
            }
        }
    }

    log(LOG::INFO, "Started, preparing stuff...");

    if (!std::filesystem::exists(destination))
    {
        log(LOG::WARN, "The destination folder does not exist, creating it...");

        if (!std::filesystem::create_directory(destination))
        {
            log(LOG::ERROR, "The destination folder couldn't be created, exiting...");
            return 2;
        }
    }

    if (!std::filesystem::exists(input))
    {
        log(LOG::ERROR, "The specified input source doesn't exist, exiting...");
        return 2;
    }

    cv::VideoCapture cap(input);

    if (!cap.isOpened())
    {
        log(LOG::ERROR, "The specified input source couldn't be opened, exiting...");
        return 2;
    }

    log(LOG::INFO, "Everything is ready, starting the job...");

    JobManager manager(workers);

    std::vector<std::thread> active_workers;

    int job_size = cap.get(cv::CAP_PROP_FRAME_COUNT) / workers + 1;

    std::cout<<job_size<<std::endl;

    for (int i = 0; i < workers; i++)
    {
        active_workers.push_back(std::move(std::thread(work, &manager, cap, i, job_size, threshold)));
    }

    log(LOG::INFO, "Wating for workers to finish...");

    for (std::thread &worker : active_workers) {
        worker.join();
    }

    log(LOG::INFO, "All workers finished.");
    log(LOG::INFO, "Getting the results...");

    std::vector<cv::Mat> output;

    if (!manager.getFinished(&output))
    {
        log(LOG::ERROR, "Fatal error, the required job is incomplete, exiting...");
        return 2;
    }

    log(LOG::INFO, "Writing the results to the destination...");

    for (unsigned i = 0; i < output.size(); i++)
    {
        cv::imwrite(destination + "frame" + std::to_string(i) + ".jpg", output[i]);
    }

    log(LOG::OK, "All the frames has been written to the specified destionation, job done.");
    log(LOG::WARN, "Exiting...");

    return 0;
}