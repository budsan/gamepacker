#include "gamepackerbuilder.h"
#include <vector>
#include <string>
#include <set>

#include "TinyDir.h"
#include "crcfast.h"

#include "lz4.h"
#include "lz4hc.h"

namespace gpack
{

struct FileEntryBuilder
{
	FileEntry entry;
	unsigned char* compressed_data;

	FileEntryBuilder() : compressed_data(NULL) {}
};

struct FilePackerBuilder
{
	std::vector<FileEntryBuilder> entries;
	uint32_t current_offset;

	FilePackerBuilder()
		: current_offset(0) {}
};

void WriteBuilder(FilePackerBuilder& builder, const std::string& base, const std::string& out_path)
{
	std::string out = out_path;
	if (out.empty())
		out = "a.out";

	FILEPACKER_LOGV("\n -- Writing packed file system on %s --\n", out.c_str());

	FilePackerHeader header;
	header.Init();
	header.file_count = (uint32_t) builder.entries.size();

	FILE* fout = fopen(out.c_str(), "wb+");
	if (fout != NULL)
	{
		fwrite((const void*)&header, sizeof(FilePackerHeader), 1, fout);
		for (size_t i = 0; i < header.file_count; i++)
		{
			FileEntry& entry = builder.entries[i].entry;
			fwrite(&entry.header, sizeof(entry.header), 1, fout);
			size_t str_offset = 0;
			uint32_t length = (uint32_t) entry.path.length();
			if (length > FileEntry::MaxPathLength)
			{
				FILEPACKER_LOGE("WARNING: File %s is too long. Truncating.", entry.path.c_str());
				str_offset = length - FileEntry::MaxPathLength;
				length = FileEntry::MaxPathLength;
			}

			fwrite(&length, sizeof(length), 1, fout);
			fwrite(entry.path.data() + str_offset, sizeof(char), length, fout);
		}

		for (size_t i = 0; i < header.file_count; i++)
		{
			FileEntryBuilder& entrybuilder = builder.entries[i];
			FileEntry& entry = builder.entries[i].entry;
			FILEPACKER_LOGV(" - Writing %s\n", entry.path.c_str());
			if (entrybuilder.compressed_data == NULL)
			{
				std::string full_path = base + "/" + entry.path;
				FILE* file = fopen(full_path.c_str(), "rb");
				char* buffer = new char[entry.header.size];
				if (file != NULL)
				{
					fread(buffer, entry.header.size, 1, file);
					fclose(file);
				}
				else
				{
					FILEPACKER_LOGE("ERROR: Unable to open %s while writing packaged file. Filling with dummy bytes.", full_path.c_str());
					memset(buffer, 0, entry.header.size);
				}

				fwrite(buffer, entry.header.size, 1, fout);
				delete[] buffer;
			}
			else
			{
				fwrite(entrybuilder.compressed_data, entry.header.size, 1, fout);
				delete[] entrybuilder.compressed_data;
				entrybuilder.compressed_data = NULL;
			}
		}
	}
}

void BuildAddFile(FilePackerBuilder& builder, bool compress, const std::string& base, const std::string& path, const char* name)
{
	FileEntryBuilder entrybuilder;
	FileEntry& entry = entrybuilder.entry;
	entry.path = path + name;
	std::string full_path = base + "/" + entry.path;

	FILE* file = fopen(full_path.c_str(), "rb");
	if (file != NULL)
	{
		fseek(file, 0, SEEK_END);
		entry.header.uncompr_size = ftell(file);

		fseek(file, 0, SEEK_SET);
		unsigned char* buffer = new unsigned char[entry.header.uncompr_size];
		fread(buffer, entry.header.uncompr_size, 1, file);
		fclose(file);

		unsigned char* crc_buffer = NULL;
		if (compress)
		{
			unsigned char* lz4_out_bound = NULL;
			int lz4_size = 0;
			int lz4_size_bound = LZ4_compressBound(entry.header.uncompr_size);
			lz4_out_bound = new unsigned char[lz4_size_bound];
			lz4_size = LZ4_compress_HC((const char*)buffer, (char*)lz4_out_bound, entry.header.uncompr_size, lz4_size_bound, 9);

			uint32_t ratio = (entry.header.uncompr_size / 2) + (entry.header.uncompr_size / 4); // < 75% original size is ok
			if (lz4_size < ratio)
			{
				entry.header.compression = FileEntry::Header::LZ4HC;
				entry.header.size = lz4_size;
				entrybuilder.compressed_data = new unsigned char[entry.header.size];
				memcpy(entrybuilder.compressed_data, lz4_out_bound, entry.header.size);
				crc_buffer = entrybuilder.compressed_data;
			}

			delete[] lz4_out_bound;
		}
		
		if (crc_buffer == NULL)
		{
			entry.header.compression = FileEntry::Header::UNCOMPRESSED;
			entry.header.size = entry.header.uncompr_size;
			entrybuilder.compressed_data = NULL;
			crc_buffer = buffer;
		}
			
		crcFast crc;
		for (uint32_t i = 0; i < entry.header.size; i++)
			crc.Append(crc_buffer[i]);

		entry.header.crc = crc.CRC();
		entry.header.offset = builder.current_offset;
		builder.current_offset += entry.header.size;

		delete[] buffer;
		entry.header.crc = crc.CRC();
		builder.entries.push_back(entrybuilder);

		PrintFileEntry(entry);
	}
}

void BuildFromPath(FilePackerBuilder& builder, bool compress, const std::string& base, const std::string& path)
{
	tinydir_dir dir;
	std::string full_path = base + "/" + path;
	if (tinydir_open_sorted(&dir, full_path.c_str()) == -1)
	{
		FILEPACKER_LOGE("Error opening directory");
		goto bail;
	}

	for (uint32_t i = 0; i < dir.n_files; i++)
	{
		tinydir_file file;
		if (tinydir_readfile_n(&dir, &file, i) == -1)
		{
			FILEPACKER_LOGE("Error getting file");
			goto bail;
		}

		if (file.name[0] != '.')
		{
			if (file.is_dir)
			{
				std::string newpath = path + file.name + "/";
				BuildFromPath(builder, compress, base, newpath);
			}
			else if (file.is_reg)
			{
				BuildAddFile(builder, compress, base, path, file.name);
			}
		}
		else
		{
			FILEPACKER_LOGV(" - SKIPPED: %s%s\n", path.c_str(), file.name);
		}
	}

bail:
	tinydir_close(&dir);
}

void BuildAndWrite(BuildParameters* params)
{
	FilePackerBuilder packer;
	BuildFromPath(packer, params->compression != FileEntry::Header::UNCOMPRESSED, params->path, "");
	WriteBuilder(packer, params->path, params->out);
}

#ifdef _MSC_VER
bool CreateDir(const char* path)
{
	BOOL result = CreateDirectory(path, NULL);
	if (result == FALSE)
	{
		DWORD lastErr = GetLastError();
		return lastErr == ERROR_ALREADY_EXISTS;
	}

	return true;
}
#else
#error "CreateDir not defined"
#endif

void Extract(ExtractParameters* extractparams)
{
	FileSystem fs;
	if (fs.Open(extractparams->file))
	{
		if (!CreateDir(extractparams->out_path))
		{
			FILEPACKER_LOGE("ERROR: Unable to create %s\n", extractparams->out_path);
			return;
		}

		std::string out_path(extractparams->out_path);
		std::set<std::string> dirs;
		auto it = fs.Entries().begin();
		for (; it != fs.Entries().end(); it++)
		{
			const FileEntry& entry = it->second;
			size_t found = entry.path.find_last_of("/\\");
			if (found != std::string::npos)
			{
				std::string path = entry.path.substr(0, found);
				found = 0;
				while (1)
				{
					found = path.find_first_of("/\\", found);
					std::string curr = path.substr(0, found);
					if (dirs.find(curr) == dirs.end())
					{
						std::string full_dir = out_path + "/" + curr;
						if (!CreateDir(full_dir.c_str()))
						{
							FILEPACKER_LOGE("ERROR: Unable to create %s\n", full_dir.c_str());
						}
						else
						{
							FILEPACKER_LOGV(" + Created dir %s\n", curr.c_str());
						}

						dirs.insert(curr);
					}

					if (found == std::string::npos)
						break;

					found++;
				}
			}

			unsigned char* buffer = new unsigned char[entry.header.uncompr_size];
			fs.Read(entry, buffer);

			std::string full_dir = out_path + "/" + entry.path;
			FILE* f = fopen(full_dir.c_str(), "wb+");
			if (f != NULL)
			{
				FILEPACKER_LOGV(" + Writing %s\n", entry.path.c_str());
				fwrite(buffer, 1, entry.header.uncompr_size, f);
				fclose(f);
			}
			else
			{
				FILEPACKER_LOGE("ERROR: Unable to write %s\n", entry.path.c_str());
			}
		}
	}
	else
	{
		FILEPACKER_LOGE("ERROR: Unable to open %s\n", extractparams->file);
	}
}

void List(const char* file)
{
	FileSystem fs;
	if (fs.Open(file))
	{
		auto it = fs.Entries().begin();
		for (; it != fs.Entries().end(); it++)
		{
			const FileEntry& entry = it->second;
			FILEPACKER_LOGV("%s\n", entry.path.c_str());
		}
	}
	else
	{
		FILEPACKER_LOGE("ERROR: Unable to open %s\n", file);
	}
}

} //namespace FilePacker