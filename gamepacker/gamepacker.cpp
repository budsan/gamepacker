#include "gamepacker.h"
#include "crcfast.h"
#include <sstream>

#include "lz4.h"

int fopen_read(void *_handle, unsigned char *_ptr, int _nbytes)
{
	return fread(_ptr, 1, _nbytes, static_cast<FILE*>(_handle));
}

int fopen_seek(void *_handle, long _offset, int _whence)
{
	return fseek(static_cast<FILE*>(_handle), _offset, _whence);
}

long fopen_tell(void *_handle)
{
	return ftell(static_cast<FILE*>(_handle));
}

int fopen_close(void *_handle)
{
	return fclose(static_cast<FILE*>(_handle));
}

gpack::FileCallbacks fopen_callback = 
{
	fopen_read,
	fopen_seek,
	fopen_tell,
	fopen_close
};

namespace gpack
{

std::string HumanizeByteSize(std::size_t bytes)
{
	const char* str_mag[] = { "b", "kb", "mb", "gb" };

	std::size_t mag = 0;
	std::size_t val = bytes;
	for (int i = 0; i < 3 && val > 16384; i++)
	{
		val /= 1024;
		mag++;
	}

	std::stringstream ss;
	ss << val << str_mag[mag];
	return ss.str();
}

void PrintFileEntry(FileEntry& entry)
{
	std::string sub = entry.path.substr(0, 60);
	std::string sz = HumanizeByteSize(entry.header.size);
	int written = FILEPACKER_LOGV("%s", sub.c_str());
	for (; written < 60; written++)
		putchar(' ');

	written += FILEPACKER_LOGV("%s", sz.c_str());
	for (; written < 70; written++)
		putchar(' ');

	float compr_ratio = (entry.header.size / (float) entry.header.uncompr_size) * 100.0f;
	written += FILEPACKER_LOGV("%3.2f%%\n", compr_ratio);
}

void FilePackerHeader::Init()
{
	header[0] = 'G';
	header[1] = 'p';
	header[2] = 'a';
	header[3] = 'k';
	version = 1;
}

bool FilePackerHeader::CheckHeader()
{
	return 
		header[0] == 'G' &&
		header[1] == 'p' &&
		header[2] == 'a' &&
		header[3] == 'k';
}

bool FilePackerHeader::CheckVersion()
{
	return version == 1;
}

FileSystem::FileSystem() : handle(NULL), cb(NULL), data_offset(0)
{
}

FileSystem::~FileSystem()
{
	Close();
}

bool FileSystem::Open(const char* _path)
{
	FILE* file = fopen(_path, "rb");
	return Open(file, &fopen_callback);
}

bool FileSystem::Open(void* _handle, FileCallbacks* _callbacks)
{
	if (_handle != NULL)
	{
		Close();

		handle = _handle;
		cb = _callbacks;

		FilePackerHeader header;
		cb->read(handle, (unsigned char*)&header, sizeof(FilePackerHeader));
		if (!header.CheckHeader())
		{
			FILEPACKER_LOGE(" - File is not FilePacker file.\n");
			Close();
			return false;
		}

		if (!header.CheckVersion())
		{
			FILEPACKER_LOGE(" - File version doesn't match.\n");
			Close();
			return false;
		}

		for (std::size_t i = 0; i < header.file_count; i++)
		{
			FileEntry entry;
			cb->read(handle, (unsigned char*) &entry.header, sizeof(entry.header));

			uint32_t length;
			cb->read(handle, (unsigned char*)&length, sizeof(length));

			if (length > FileEntry::MaxPathLength)
			{
				FILEPACKER_LOGE("ERROR: Invalid lenght file %d.\n", i);
				length = FileEntry::MaxPathLength;
			}

			char buffer[FileEntry::MaxPathLength];
			cb->read(handle, (unsigned char*)buffer, length);
			buffer[length] = '\0';
			entry.path = buffer;

			entries[entry.path] = entry;
		}

		data_offset = cb->tell(handle);
	}
	else
	{
		FILEPACKER_LOGE("File not found.\n");
		return false;
	}

	return true;
}

void FileSystem::Close()
{
	if (handle != NULL)
	{
		if (cb->close != NULL)
		{
			cb->close(handle);
			handle = NULL;
		}
		
		cb = NULL;
		entries.clear();
		data_offset = 0;
	}
}

void FileSystem::Read(const FileEntry& entry, unsigned char* out) const
{
	cb->seek(handle, data_offset + entry.header.offset, SEEK_SET);
	if (entry.header.compression == FileEntry::Header::LZ4HC)
	{
		unsigned char* buffer = new unsigned char[entry.header.size];
		cb->read(handle, buffer, entry.header.size);
		int result = LZ4_decompress_safe((const char*) buffer, (char*) out, entry.header.size, entry.header.uncompr_size);
		delete[] buffer;

		if (result < 0)
		{
			FILEPACKER_LOGE("Error decompressing file");
		}
	}
	else
	{
		cb->read(handle, out, entry.header.uncompr_size);
	}
}

void FileSystem::ReadRaw(const FileEntry& entry, unsigned char* out) const
{
	cb->seek(handle, data_offset + entry.header.offset, SEEK_SET);
	cb->read(handle, out, entry.header.size);
}

const FileSystem::EntryMap& FileSystem::Entries() const
{
	return entries;
}

void TestFile(const char* path)
{
	FileSystem fs;
	if (fs.Open(path))
	{
		auto it = fs.Entries().begin();
		for (; it != fs.Entries().end(); it++)
		{
			const FileEntry& entry = it->second;
			unsigned char* buffer = new unsigned char[entry.header.size];
			fs.ReadRaw(entry, buffer);

			crcFast crc;
			for (size_t i = 0; i < entry.header.size; i++)
				crc.Append(buffer[i]);

			delete[] buffer;
			unsigned short expected = crc.CRC();
			if (entry.header.crc == expected)
			{
				FILEPACKER_LOGV(" + File %s CRC(%d) is OK.\n", entry.path.c_str(), entry.header.crc);
			}
			else
			{
				FILEPACKER_LOGE(" - File %s CRC(%d) is WRONG. Expected: %d\n", entry.path.c_str(), entry.header.crc, expected);
			}
		}
	}
}

} // namespace FilePacker
