#pragma once

#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cmath>

class FAT12
{
private:
	struct BootSector
	{
		uint16_t sector_size;
		uint8_t sectors_per_cluster;
		uint16_t num_reserved_sectors;
		uint8_t num_fats;
		uint16_t max_num_root_entries;
		uint16_t total_sector_count;
		uint16_t sectors_per_fat;
	};
	struct DirectoryEntry
	{
		std::string name;
		std::string extension;
		uint8_t attributes;
		uint16_t reserved;
		uint16_t creation_time;
		uint16_t creation_date;
		uint16_t last_access_date;
		uint16_t last_write_time;
		uint16_t last_write_date;
		uint16_t first_logical_cluster;
		uint32_t file_size;
		bool is_directory;
		std::string path;
		std::string parent_name;
		uint16_t parent_cluster;
	};
	struct FATEntry
	{
		uint16_t value;
	};
	std::string disk_image_name;
	std::ifstream disk_image;
	BootSector boot_sector_contents;
	std::vector<FATEntry> fat_table;
	std::vector<DirectoryEntry> root_directory_entries;
	std::map<std::string, std::vector<DirectoryEntry>> subdirectories;

	inline void readBootSector();
	inline void readFat();
	void readRootDirectoryEntries();
	void readSubdirectoriesEntries();
	void listDirectory(const std::vector<DirectoryEntry> &directory_entries);
	inline uint16_t findFreeCluster();
	inline bool hasEnoughFreeClusters(uint32_t &required_clusters);
	inline void findFreeEntry(DirectoryEntry &new_entry, size_t &insert_index, std::ifstream &input_file);
	inline void updateNewEntryFields(DirectoryEntry &new_entry, const std::string &destination);
	inline void writeInputFileInDiskImage(std::fstream &writable_disk_image, std::vector<char> &buffer, uint16_t &new_cluster, uint32_t &required_clusters);
	inline void updateDiskImageFatTable(std::fstream &writable_disk_image);
	inline void updateDiskImageRootDirectory(std::fstream &writable_disk_image);
public:
	FAT12(const std::string &image);
	void LS();
	void LS1();
	void copyToSystem(const std::string &file_name);
	void copyFromSystem(const std::string &source);
	void analyzeDisk();
};