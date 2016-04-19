#pragma once
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <map>

#ifndef FILEPACKER_LOGV
#define FILEPACKER_LOGV(...) fprintf(stdout, __VA_ARGS__)
#endif

#ifndef FILEPACKER_LOGE
#define FILEPACKER_LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace gpack
{

#define FILE_PACKER_HEADER_SIZE 4

struct FilePackerHeader
{
	uint8_t header[FILE_PACKER_HEADER_SIZE];
	uint32_t version;
	uint32_t file_count;

	void Init();
	bool CheckHeader();
	bool CheckVersion();
};

typedef int(*read_func)(void *_handle, unsigned char *_ptr, int _nbytes);
typedef int(*seek_func)(void *_handle, long _offset, int _whence);
typedef long(*tell_func)(void *_handle);
typedef int(*close_func)(void *_handle);

struct FileCallbacks
{
	read_func  read;
	seek_func  seek;
	tell_func  tell;
	close_func close;
};

struct FileEntry
{
	enum
	{
		MaxPathLength = 1024
	};

	struct Header
	{
		uint32_t size;
		uint32_t uncompr_size;
		uint32_t offset;

		enum Compression
		{
			UNCOMPRESSED,
			LZ4HC
		};

		uint8_t compression;
		uint8_t unused;
		uint16_t crc;
	};

	std::string path;
	Header header;
};

struct FileSystem
{
	FileSystem();
	~FileSystem();

	bool Open(const char* path);
	bool Open(void* handle, FileCallbacks* callbacks);
	void Close();

	void Read(const FileEntry& entry, unsigned char* out) const;
	void ReadRaw(const FileEntry& entry, unsigned char* out) const;

	typedef std::map<std::string, FileEntry> EntryMap;
	const EntryMap& Entries() const;

private:
	void* handle;
	FileCallbacks* cb;
	EntryMap entries;
	uint32_t data_offset;
};

void PrintFileEntry(FileEntry& entry);
void TestFile(const char* file);

}
