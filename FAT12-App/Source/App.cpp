#include "Core/Core.h"
#include <string>

class FAT12Frontend
{
private:
    FAT12 fat12;

    void displayHelp()
    {
        std::cout << std::left << std::setw(20) << "| Command" << std::left << std::setw(40) << "| Description" << "|\n";
        std::cout << std::left << std::setw(20) << std::setfill('-') << "|" << std::setw(40) << "|" << std::setfill(' ') << "|\n";

        std::cout << std::left << std::setw(20) << "| ?" << std::left << std::setw(40) << "| Show available commands" << "|\n";
        std::cout << std::left << std::setw(20) << "| ls" << std::left << std::setw(40) << "| LS()" << "|\n";
        std::cout << std::left << std::setw(20) << "| ls-1" << std::left << std::setw(40) << "| LS-1()" << "|\n";
        std::cout << std::left << std::setw(20) << "| export \"file_path\"" << std::left << std::setw(40) << "| copyToSystem(file_path)" << "|\n";
        std::cout << std::left << std::setw(20) << "| import \"file_name\"" << std::left << std::setw(40) << "| copyFromSystem(file_name)" << "|\n";
        std::cout << std::left << std::setw(20) << "| status" << std::left << std::setw(40) << "| analyzeDisk()" << "|\n";
    }

public:
    FAT12Frontend(const std::string& imageFilePath) : fat12("./" + imageFilePath + ".img")
    { 
        std::cout << "Type '?' for help.\n" << std::endl; 
    }

    void run() 
    {
        std::string command;
        while (true) 
        {
            std::cout << "> ";
            std::getline(std::cin, command);

            if (command == "?") 
                displayHelp();
            else if (command == "ls") 
                fat12.LS();
            else if (command == "ls-1") 
                fat12.LS1();
            else if (command.find("export ") == 0)
            {
                std::string file_path = command.substr(7);
                fat12.copyToSystem(file_path);
            }
            else if (command.find("import ") == 0)
            {
                std::string file_name = command.substr(7);
                fat12.copyFromSystem(file_name);
            }
            else if (command == "status") 
                fat12.analyzeDisk();
            else 
                std::cout << "Unknown command. Type '?' for help.\n";
        }
    }
};

int main()
{
    while (true)
    {
        std::cout << "Please enter Disk Image Name: " << std::endl;
        std::string diskImagePath;
        std::getline(std::cin, diskImagePath);

        if (std::filesystem::exists("./" +diskImagePath + ".img"))
        {
            FAT12Frontend fat12Frontend(diskImagePath);
            fat12Frontend.run();
        }
        else
            std::cout << "\nERROR: Disk Image Not Found!\n" << std::endl;
    }
	return 0;
}