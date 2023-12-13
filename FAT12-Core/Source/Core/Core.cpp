#include "Core.h"

// Private member function implementations
inline void FAT12::readBootSector()
{
	disk_image.seekg(11);
	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.sector_size), 
		sizeof(boot_sector_contents.sector_size));

	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.sectors_per_cluster),
		sizeof(boot_sector_contents.sectors_per_cluster));

	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.num_reserved_sectors), 
		sizeof(boot_sector_contents.num_reserved_sectors));

	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.num_fats), 
		sizeof(boot_sector_contents.num_fats));

	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.max_num_root_entries), 
		sizeof(boot_sector_contents.max_num_root_entries));

	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.total_sector_count), 
		sizeof(boot_sector_contents.total_sector_count));

	disk_image.seekg(22);
	disk_image.read(reinterpret_cast<char*>(&boot_sector_contents.sectors_per_fat),
		sizeof(boot_sector_contents.sectors_per_fat));
}

inline void FAT12::readFat()
{
	disk_image.seekg(boot_sector_contents.num_reserved_sectors * boot_sector_contents.sector_size);
	uint16_t total_fat_entries = static_cast<uint16_t>(ceil((boot_sector_contents.sector_size * 8) / 1.5));
	fat_table.resize(total_fat_entries);  
	std::vector<uint8_t> buffer(3); // Temporary buffer to read 3 bytes at a time

	// Read each 3-byte group and convert to 2 12-bit entries
	for (int i = 0; i < total_fat_entries / 2; ++i)
	{
		disk_image.read(reinterpret_cast<char*>(buffer.data()), 3);

		// Convert 3 bytes into 2 12-bit entries (little-endian assumption)
		uint16_t entry1 = (buffer[0] | ((buffer[1] & 0x0F) << 8));
		uint16_t entry2 = (((buffer[1] & 0xF0) >> 4) | (buffer[2] << 4));

		// Store the 12-bit entries in the vector
		fat_table[i * 2].value = entry1;
		fat_table[i * 2 + 1].value = entry2;
	}
}

void FAT12::readRootDirectoryEntries()
{
	uint32_t root_dir_offset = (boot_sector_contents.num_reserved_sectors +
		(boot_sector_contents.num_fats * boot_sector_contents.sectors_per_fat)) *
		boot_sector_contents.sector_size;
	disk_image.seekg(root_dir_offset);

	size_t root_dir_entry_count = boot_sector_contents.max_num_root_entries;
	for (size_t i{ 0 }; i < root_dir_entry_count; i++)
	{
		DirectoryEntry entry;
		entry.name = "";
		entry.extension = "";
		entry.attributes = 0;
		entry.reserved = 0;
		entry.creation_time = 0;
		entry.creation_date = 0;
		entry.last_access_date = 0;
		entry.last_write_time = 0;
		entry.last_write_date = 0;
		entry.first_logical_cluster = 0;
		entry.file_size = 0;
		entry.is_directory = false;
		entry.path = "/";
		entry.parent_name = "";
		entry.parent_cluster = root_dir_offset / boot_sector_contents.sector_size;
		char name[9];
		char extension[4];

		disk_image.read(name, 8);
		name[8] = '\0';
		std::string name_without_spaces;
		// Convert name to uppercase
		for (int j = 0; j < 8; j++)
		{
			if (name[j] != ' ')
			{
				name[j] = std::toupper(name[j]);
				name_without_spaces += name[j];
			}
		}
		entry.name = name_without_spaces;

		disk_image.read(extension, 3);
		extension[3] = '\0';
		std::string extension_without_spaces;
		// Convert extension to uppercase
		for (int j = 0; j < 3; j++)
		{
			if (extension[j] != ' ')
			{
				extension[j] = std::toupper(extension[j]);
				extension_without_spaces += extension[j];
			}
		}
		entry.extension = extension_without_spaces;
		
		if (name[0] == 0xE5 || name[0] == 0x00)
		{
			// Include both free entries and end-of-directory entries
			root_directory_entries.push_back(entry);
			// Move to the next entry
			disk_image.seekg(32 - 8 - 3, std::ios::cur);
			continue;
		}
		
		disk_image.read(reinterpret_cast<char*>(&entry.attributes), sizeof(entry.attributes));
		disk_image.read(reinterpret_cast<char*>(&entry.reserved), sizeof(entry.reserved));
		disk_image.read(reinterpret_cast<char*>(&entry.creation_time), sizeof(entry.creation_time));
		disk_image.read(reinterpret_cast<char*>(&entry.creation_date), sizeof(entry.creation_date));
		disk_image.read(reinterpret_cast<char*>(&entry.last_access_date), sizeof(entry.last_access_date));
		disk_image.seekg(2, std::ios::cur); // ignores 2 bytes
		disk_image.read(reinterpret_cast<char*>(&entry.last_write_time), sizeof(entry.last_write_time));
		disk_image.read(reinterpret_cast<char*>(&entry.last_write_date), sizeof(entry.last_write_date));
		disk_image.read(reinterpret_cast<char*>(&entry.first_logical_cluster), sizeof(entry.first_logical_cluster));
		disk_image.read(reinterpret_cast<char*>(&entry.file_size), sizeof(entry.file_size));

		entry.is_directory = (entry.attributes & 0x10) != 0;
		root_directory_entries.push_back(entry);
	}
}

void FAT12::readSubdirectoriesEntries()
{
	for (const DirectoryEntry& entry : root_directory_entries)
	{
		std::vector<DirectoryEntry> subdirectory_entries;
		if (!entry.is_directory || entry.name[0] == 0xE5 || entry.name[0] == 0x00)
			continue;

		DirectoryEntry subdirectory_entry;
		subdirectory_entry.name = "";
		subdirectory_entry.extension = "";
		subdirectory_entry.attributes = 0;
		subdirectory_entry.reserved = 0;
		subdirectory_entry.creation_time = 0;
		subdirectory_entry.creation_date = 0;
		subdirectory_entry.last_access_date = 0;
		subdirectory_entry.last_write_time = 0;
		subdirectory_entry.last_write_date = 0;
		subdirectory_entry.first_logical_cluster = entry.first_logical_cluster;
		subdirectory_entry.file_size = 0;
		subdirectory_entry.is_directory = false;
		subdirectory_entry.path = entry.path + entry.name + "/";
		subdirectory_entry.parent_name = "/";
		subdirectory_entry.parent_cluster = 0;	// Will be updated later to actual parent cluster

		uint32_t subdirectoy_offset = ((33 + subdirectory_entry.first_logical_cluster - 2) *
			boot_sector_contents.sectors_per_cluster * boot_sector_contents.sector_size);
		subdirectoy_offset += 58; // skips to first_logical_cluster of second entry (entry "..")
		disk_image.seekg(subdirectoy_offset);
		
		disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.parent_cluster), sizeof(subdirectory_entry.parent_cluster));
		
		// Updates the parent_name if it isn't the Root Directory
		if (subdirectory_entry.parent_cluster != 0)
		{
			for (const auto &subdir : subdirectories)
			{
				for (const auto& entry : subdir.second)
				{
					if (entry.first_logical_cluster == subdirectory_entry.parent_cluster)
						subdirectory_entry.parent_name = subdir.first;
				}
			}
		}

		disk_image.seekg(4, std::ios::cur);	// skips file size field of entry ".."
		
		size_t remaining_entries = 14;
		for (size_t i{ 0 }; i < remaining_entries; i++)
		{
			char name[9];
			char extension[4];

			disk_image.read(name, 8);
			name[8] = '\0';
			std::string name_without_spaces;
			// Convert name to uppercase
			for (int j = 0; j < 8; j++)
			{
				if (name[j] != ' ')
				{
					name[j] = std::toupper(name[j]);
					name_without_spaces += name[j];
				}
			}
			subdirectory_entry.name = name_without_spaces;

			disk_image.read(extension, 3);
			extension[3] = '\0';
			std::string extension_without_spaces;
			// Convert extension to uppercase
			for (int j = 0; j < 3; j++)
			{
				if (extension[j] != ' ')
				{
					extension[j] = std::toupper(extension[j]);
					extension_without_spaces += extension[j];
				}
			}
			subdirectory_entry.extension = extension_without_spaces;

			if (name[0] == 0xE5 || name[0] == 0x00)
			{
				// Include both free entries and end-of-directory entries
				subdirectory_entries.push_back(subdirectory_entry);
				// Move to the next entry
				disk_image.seekg(32 - 8 - 3, std::ios::cur);
				continue;
			}

			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.attributes), sizeof(subdirectory_entry.attributes));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.reserved), sizeof(subdirectory_entry.reserved));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.creation_time), sizeof(subdirectory_entry.creation_time));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.creation_date), sizeof(subdirectory_entry.creation_date));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.last_access_date), sizeof(subdirectory_entry.last_access_date));
			disk_image.seekg(2, std::ios::cur); // ignores 2 bytes
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.last_write_time), sizeof(subdirectory_entry.last_write_time));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.last_write_date), sizeof(subdirectory_entry.last_write_date));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.first_logical_cluster), sizeof(subdirectory_entry.first_logical_cluster));
			disk_image.read(reinterpret_cast<char*>(&subdirectory_entry.file_size), sizeof(subdirectory_entry.file_size));

			subdirectory_entry.is_directory = (subdirectory_entry.attributes & 0x10) != 0;

			if (subdirectory_entry.is_directory && subdirectory_entry.parent_cluster != 0)
				subdirectory_entry.path = subdirectory_entry.path + "/" + subdirectory_entry.name + "/";

			subdirectory_entries.push_back(subdirectory_entry);
		}
		subdirectories[entry.name] = subdirectory_entries;
	}
}

void FAT12::listDirectory(const std::vector<DirectoryEntry> &directory_entries)
{
	for (const DirectoryEntry& entry : directory_entries)
	{
		if (entry.name[0] == 0xE5 || entry.name[0] == 0x00)
			continue;

		std::string entryPath;
		if (entry.is_directory)
			entryPath = entry.path + entry.name + " (dir)";
		else
			entryPath = entry.path + entry.name + "." + entry.extension;
		std::cout << entryPath << std::endl;
	}
}

inline uint16_t FAT12::findFreeCluster()
{
	for (uint16_t cluster = 2; cluster < fat_table.size(); ++cluster)
	{
		if (fat_table[cluster].value == 0x000)
			return cluster;
	}

	return 0xFFFF; // No free cluster found
}

inline bool FAT12::hasEnoughFreeClusters(uint32_t &required_clusters)
{
	// Count the number of free clusters in the FAT table
	uint32_t free_clusters = 0;

	for (uint16_t cluster = 2; cluster < fat_table.size(); ++cluster)
	{
		if (fat_table[cluster].value == 0x000)
			++free_clusters;
	}

	return free_clusters >= required_clusters;
}

inline void FAT12::findFreeEntry(DirectoryEntry &new_entry, size_t &insert_index, std::ifstream &input_file)
{
	bool found_free_entry = false;

	for (size_t i = 0; i < root_directory_entries.size(); ++i)
	{
		if (root_directory_entries[i].name[0] == 0xE5 || root_directory_entries[i].name[0] == 0x00)
		{
			new_entry = root_directory_entries[i];
			found_free_entry = true;
			insert_index = i;
			break;
		}
	}

	if (!found_free_entry)
	{
		std::cerr << "ERROR: No free entry available in the root directory." << std::endl;
		input_file.close();
		return;
	}
}

inline void FAT12::updateNewEntryFields(DirectoryEntry& new_entry, const std::string& destination)
{
	std::string base_name, ext;
	size_t dot_pos = destination.find('.');

	if (dot_pos != std::string::npos)
	{
		base_name = destination.substr(0, dot_pos);
		ext = destination.substr(dot_pos + 1);
	}

	new_entry.name = base_name;
	new_entry.extension = ext;
	new_entry.attributes = 0x20; // Archive (0x10 for dir)
	new_entry.reserved = 0;
	new_entry.creation_time = 0;
	new_entry.creation_date = 0;
	new_entry.last_access_date = 0;
	new_entry.last_write_time = 0;
	new_entry.last_write_date = 0;

	// Find the first cluster for the new file
	uint16_t new_cluster = findFreeCluster();
	new_entry.first_logical_cluster = new_cluster;
}

inline void FAT12::writeInputFileInDiskImage(std::fstream &writable_disk_image, std::vector<char> &buffer, uint16_t &new_cluster, uint32_t &required_clusters)
{
	uint32_t remaining_bytes = static_cast<uint32_t>(buffer.size());
	uint32_t total_remaining_bytes = static_cast<uint32_t>(required_clusters * boot_sector_contents.sector_size);
	auto buffer_it = buffer.begin();
	
	while (new_cluster < 0xFF8 && remaining_bytes > 0)
	{
		uint32_t data_cluster_offset = ((33 + new_cluster - 2) *
			boot_sector_contents.sectors_per_cluster * boot_sector_contents.sector_size);
		writable_disk_image.seekp(data_cluster_offset);

		uint32_t bytes_to_write = std::min(remaining_bytes,
			static_cast<uint32_t>(boot_sector_contents.sectors_per_cluster *
				boot_sector_contents.sector_size));

		writable_disk_image.write(&(*buffer_it), bytes_to_write);
		remaining_bytes -= bytes_to_write;
		std::advance(buffer_it, bytes_to_write);

		if (remaining_bytes > 0)
		{
			fat_table[new_cluster].value = 0xFF0;
			uint16_t next_new_cluster = findFreeCluster();
			fat_table[new_cluster].value = next_new_cluster;
			new_cluster = next_new_cluster;
		}
	}

	// Update the FAT table to mark the last cluster as end-of-file
	if (new_cluster < 0xFF8)
	{
		fat_table[new_cluster].value = 0xFFF;
		if (total_remaining_bytes - buffer.size() > 0)
		{
			std::vector<char> zeros(total_remaining_bytes - buffer.size(), 0);
			writable_disk_image.write(zeros.data(), zeros.size());
		}
	}
}

inline void FAT12::updateDiskImageFatTable(std::fstream &writable_disk_image)
{
	std::vector<uint8_t> packed_fat_table;

	for (size_t i = 0; i < fat_table.size(); i += 2)
	{
		uint16_t entry1 = fat_table[i].value;
		uint16_t entry2 = (i + 1 < fat_table.size()) ? fat_table[i + 1].value : 0;

		// Pack two 12-bit values into three bytes
		uint8_t byte1 = entry1 & 0xFF;
		uint8_t byte2 = ((entry1 >> 8) & 0x0F) | ((entry2 & 0x0F) << 4);
		uint8_t byte3 = (entry2 >> 4) & 0xFF;

		packed_fat_table.push_back(byte1);
		packed_fat_table.push_back(byte2);
		packed_fat_table.push_back(byte3);
	}

	// Write the packed FAT table to the disk image
	uint32_t fat_offset = boot_sector_contents.num_reserved_sectors * boot_sector_contents.sector_size;
	writable_disk_image.seekp(fat_offset);
	writable_disk_image.write(reinterpret_cast<char*>(packed_fat_table.data()), packed_fat_table.size());
}

inline void FAT12::updateDiskImageRootDirectory(std::fstream &writable_disk_image)
{
	// Calculate the offset to the root directory
	uint32_t root_dir_offset = (boot_sector_contents.num_reserved_sectors +
		(boot_sector_contents.num_fats * boot_sector_contents.sectors_per_fat)) *
		boot_sector_contents.sector_size;

	// Seek to the root directory on the writable disk image
	writable_disk_image.seekp(root_dir_offset);

	// Write the modified root directory entries
	for (const auto& entry : root_directory_entries)
	{
		// Convert the entry's name and extension to the format used in the directory entry
		char name[8], extension[3];
		std::fill(std::begin(name), std::end(name), ' ');
		std::fill(std::begin(extension), std::end(extension), ' ');

		// Copy the characters from the entry's name and extension
		std::copy(entry.name.begin(), entry.name.end(), name);
		std::copy(entry.extension.begin(), entry.extension.end(), extension);

		// Write the name and extension
		writable_disk_image.write(name, sizeof(name));
		writable_disk_image.write(extension, sizeof(extension));

		// Write the remaining fields of the directory entry
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.attributes), sizeof(entry.attributes));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.reserved), sizeof(entry.reserved));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.creation_time), sizeof(entry.creation_time));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.creation_date), sizeof(entry.creation_date));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.last_access_date), sizeof(entry.last_access_date));
		uint16_t zero = 0;
		writable_disk_image.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.last_write_time), sizeof(entry.last_write_time));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.last_write_date), sizeof(entry.last_write_date));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.first_logical_cluster), sizeof(entry.first_logical_cluster));
		writable_disk_image.write(reinterpret_cast<const char*>(&entry.file_size), sizeof(entry.file_size));
	}
}


// Public member function implementations
FAT12::FAT12(const std::string& image)
{
	disk_image_name = image;
	boot_sector_contents.sector_size = 0;
	boot_sector_contents.sectors_per_cluster = 0;
	boot_sector_contents.num_reserved_sectors = 0;
	boot_sector_contents.num_fats = 0;
	boot_sector_contents.max_num_root_entries = 0;
	boot_sector_contents.total_sector_count = 0;
	boot_sector_contents.sectors_per_fat = 0;

	disk_image.open(image, std::ios::binary);
	if (!disk_image.is_open())
	{
		std::cerr << "ERROR: Failed to open the disk image." << std::endl;
		return;
	}

	readBootSector();
	readFat();
	readRootDirectoryEntries();
	readSubdirectoriesEntries();
}

void FAT12::LS()
{
	std::cout << std::left;
	int table_width = 53;  
	int title_padding = (table_width - 32) / 2;

	// Print the top of the table
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	// Print the title with centered text
	std::cout << std::setw(title_padding) << "" << "| Listing Files and Directories |" << std::setw(table_width - title_padding - 32)  << std::endl;

	// Print the separator
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	listDirectory(root_directory_entries);
	for(const auto& subdirectory_entries : subdirectories)
		listDirectory(subdirectory_entries.second);
	std::cout << "\n" << std::endl;
}

void FAT12::LS1()
{
	std::cout << std::left;
	int table_width = 53;
	int title_padding = (table_width - 29) / 2;

	// Print the top of the table
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	// Print the title with centered text
	std::cout << std::setw(title_padding) << "" << "| Listing Root Directory |" << std::setw(table_width - title_padding - 29) << std::endl;

	// Print the separator
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	listDirectory(root_directory_entries);
	std::cout << "\n" << std::endl;
}

void FAT12::copyToSystem(const std::string& file_path)
{
	size_t last_slash = file_path.find_last_of('/');
	std::string last_subdirectory = "";
	std::string path = file_path.substr(0, last_slash+1);

	// Find Last Subdirectory in file_path
	if (last_slash != std::string::npos)
	{
		size_t secondLastSlashPos = file_path.find_last_of('/', last_slash - 1);
		if (secondLastSlashPos != std::string::npos)
			last_subdirectory = file_path.substr(secondLastSlashPos + 1, last_slash - secondLastSlashPos - 1);
	}
	bool is_in_root_directory = (last_subdirectory.empty());

	if (!is_in_root_directory)
	{
		auto valid_subdir = subdirectories.find(last_subdirectory);

		if (valid_subdir == subdirectories.end())
		{
			std::cerr << "ERROR: Subdirectory not found: " << last_subdirectory << std::endl;
			return;
		}

		bool found{ false };
		for (const auto& entry : valid_subdir->second)
		{
			if (entry.path == path)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			std::cerr << "ERROR: Subdirectory not found." << std::endl;
			return;
		}
	
	}
	std::string file_name = file_path.substr(last_slash + 1);

	const std::vector<DirectoryEntry>& directory_entries = is_in_root_directory ? root_directory_entries : subdirectories[last_subdirectory];

	// Find the entry for the specified file in the directory
	for (const DirectoryEntry& entry : directory_entries)
	{
		std::string full_name = entry.name + "." + entry.extension;
		if (full_name == file_name)
		{
			// Open the file on the host system for writing
			std::ofstream output_file(full_name, std::ios::binary);

			if (!output_file.is_open())
			{
				std::cerr << "ERROR: Failed to open output file." << std::endl;
				return;
			}

			// Read the entire file content by traversing the FAT
			uint16_t current_cluster = entry.first_logical_cluster;
			std::vector<char> buffer;

			while (current_cluster < 0xFF8)
			{
				uint32_t cluster_offset = ((33 + current_cluster - 2) *
					boot_sector_contents.sectors_per_cluster *
					boot_sector_contents.sector_size);
				disk_image.seekg(cluster_offset);

				// Read the content of the current cluster
				buffer.resize(boot_sector_contents.sectors_per_cluster * boot_sector_contents.sector_size);
				disk_image.read(buffer.data(), buffer.size());

				// Calculate the amount of data to write
				size_t data_to_write = std::min<size_t>(entry.file_size, buffer.size());

				// Write the actual data to the output file
				output_file.write(buffer.data(), data_to_write);

				// Move to the next cluster in the chain
				current_cluster = fat_table[current_cluster].value;
			}

			output_file.close();
			std::cout << "File copied to system: " << full_name << std::endl;
			return;
		}
	}
	std::cerr << "ERROR: File not found." << std::endl;
}

void FAT12::copyFromSystem(const std::string &source)
{
	size_t file_name_pos = source.find_last_of('/');
	const std::string& destination = source.substr(file_name_pos+1);
	
	// Try to find the entry for the specified destination file in the root directory
	for (DirectoryEntry &entry : root_directory_entries)
	{
		std::string full_name = entry.name + "." + entry.extension;
		if (full_name == destination)
		{
			std::cerr << "ERROR: File already exists in the destination." << std::endl;
			return;
		}
	}

	// Open the file on the host system for reading
	std::ifstream input_file(source, std::ios::binary);
	if (!input_file.is_open())
	{
		std::cerr << "ERROR: Failed to open input file." << std::endl;
		return;
	}

	// Copy the content from the source file to the buffer
	std::vector<char> buffer(std::istreambuf_iterator<char>(input_file), {});

	// Check if fat_table has enough free clusters for file content 
	uint32_t required_clusters = static_cast<uint32_t>(std::ceil(static_cast<double>(buffer.size()) /
		(boot_sector_contents.sectors_per_cluster * boot_sector_contents.sector_size)));
	if (!hasEnoughFreeClusters(required_clusters))
	{
		std::cerr << "ERROR: Not enough free clusters for the new file." << std::endl;
		input_file.close();
		return;
	}

	// Try to find a free entry in the root directory for the new file
	DirectoryEntry new_entry;
	size_t insert_index = 0; // Track the index of the free entry
	findFreeEntry(new_entry, insert_index, input_file);

	// Update the new entry with the destination file name (and other fields)
	updateNewEntryFields(new_entry, destination);

	// Insert the new_entry in root_directory
	root_directory_entries[insert_index] = new_entry;
		
	// Open the disk image in writing mode
	disk_image.close();
	std::fstream writable_disk_image;
	writable_disk_image.open(disk_image_name, std::ios::in | std::ios::out |std::ios::binary);
	if (!writable_disk_image.is_open())
	{
		std::cerr << "ERROR: Failed to open disk image for writing." << std::endl;
		input_file.close();
		return;
	}

	// Write the content of the input file to the disk image
	writeInputFileInDiskImage(writable_disk_image, buffer, new_entry.first_logical_cluster, required_clusters);

	// Update file size of the new entry
	root_directory_entries[insert_index].file_size = static_cast<uint32_t>(buffer.size());

	// After updating the FAT table in memory, pack it and write it to the disk image
	updateDiskImageFatTable(writable_disk_image);

	// After updating the Root Directory in memory, write it to the disk image
	updateDiskImageRootDirectory(writable_disk_image);

	input_file.close();
	writable_disk_image.close();
	disk_image.open(disk_image_name, std::ios::binary);
	std::cout << "File copied from system to disk image: " << destination << std::endl;
}

void FAT12::analyzeDisk()
{
	// Calculate the partition size (capacity of storage)
	uint32_t partition_size = boot_sector_contents.total_sector_count *
		boot_sector_contents.sector_size;

	// Calculate the size of the reserved area (FAT tables and boot sector)
	uint32_t reserved_size = boot_sector_contents.num_reserved_sectors *
		boot_sector_contents.sector_size;

	// Calculate the size of one FAT table
	uint32_t fat_size = boot_sector_contents.num_fats *
		boot_sector_contents.sectors_per_fat *
		boot_sector_contents.sector_size;

	// Calculate the size of the root directory
	uint32_t root_directory_size = boot_sector_contents.max_num_root_entries * 32;

	// Calculate the size of the data area
	uint32_t data_area_size = partition_size - reserved_size - fat_size -
		root_directory_size;

	// Calculate the space used in the data area by the root directory
	uint32_t used_space = 0;
	for (const DirectoryEntry& entry : root_directory_entries)
	{
		if (entry.name[0] != 0xE5 && entry.name[0] != 0x00)
		{
			// Entry is in use
			uint32_t clusters = static_cast<uint32_t>(std::ceil(static_cast<double>(entry.file_size) /
				(boot_sector_contents.sectors_per_cluster *
					boot_sector_contents.sector_size)));

			used_space += clusters * boot_sector_contents.sectors_per_cluster *
				boot_sector_contents.sector_size;
		}
	}

	// Calculate the space used in the data area by subdirectories
	for (const auto& subdirectory : subdirectories)
	{
		for (const DirectoryEntry& entry : subdirectory.second)
		{
			if (entry.name[0] != 0xE5 && entry.name[0] != 0x00)
			{
				// Entry is in use
				uint32_t clusters = static_cast<uint32_t>(std::ceil(static_cast<double>(entry.file_size) /
					(boot_sector_contents.sectors_per_cluster *
						boot_sector_contents.sector_size)));

				used_space += clusters * boot_sector_contents.sectors_per_cluster *
					boot_sector_contents.sector_size;
			}
		}
	}


	// Calculate available space
	uint32_t available_space = data_area_size - used_space;

	std::cout << std::fixed << std::setprecision(2);
	int table_width = 53;
	int title_padding = (table_width - 16) / 2;

	// Print the top of the table
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	// Print the title with centered text
	std::cout << std::setw(title_padding) << "" << "| Disk Analysis |" << std::endl;

	// Print the separator
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;

	// Print the results in a table-like format with a fixed right wall
	std::cout << std::left << std::setw(25) << "| Partition Size:" << std::right << std::setw(20) << partition_size << " bytes |" << std::endl;
	std::cout << std::left << std::setw(25) << "| Reserved Area Size:" << std::right << std::setw(20) << reserved_size << " bytes |" << std::endl;
	std::cout << std::left << std::setw(25) << "| FAT Size:" << std::right << std::setw(20) << fat_size << " bytes |" << std::endl;
	std::cout << std::left << std::setw(25) << "| Root Directory Size:" << std::right << std::setw(20) << root_directory_size << " bytes |" << std::endl;
	std::cout << std::left << std::setw(25) << "| Data Area Size:" << std::right << std::setw(20) << data_area_size << " bytes |" << std::endl;
	std::cout << std::left << std::setw(25) << "| Used Space:" << std::right << std::setw(20) << used_space << " bytes |" << std::endl;
	std::cout << std::left << std::setw(25) << "| Available Space:" << std::right << std::setw(20) << available_space << " bytes |" << std::endl;

	// Print the bottom of the table with a fixed right wall
	std::cout << std::setw(table_width) << std::setfill('-') << "" << std::setfill(' ') << std::endl;
	std::cout << "\n" << std::endl;
}
