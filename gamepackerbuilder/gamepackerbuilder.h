#pragma once

#include "gamepacker.h"

namespace gpack
{
	struct BuildParameters
	{
		const char* path;
		const char* out;
		FileEntry::Header::Compression compression;
	};

	void BuildAndWrite(BuildParameters* params);

	struct ExtractParameters
	{
		const char* file;
		const char* out_path;
	};

	void Extract(ExtractParameters* extractparams);

	void List(const char* file);
}
