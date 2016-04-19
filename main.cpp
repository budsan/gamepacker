#include <stdio.h>
#include <string>

#include "gamepacker.h"
#include "gamepackerbuilder.h"

int g_argc;
char** g_argv;

void PrintUsage()
{
	printf("\nUSAGE: %s [PARAMETERS]\n\n", g_argv[0]);
	printf("Packs a directory into a file. The file keeps relative paths. Allows optionally\n"
		   "to compress individually files if compressed data size is less than 75%% of the\n"
		   "original data size.\n\n"
		   "  -i, --input [dir]    Build given directory to a package. If out file is not\n"
		   "                       given, assumes out file is a.out.\n"
		   "  -o, --output [path]  Specifies the output path.\n"
		   "                        - If --input, specifies build file path.\n"
		   "                        - If --output, specifies output dir path.\n"
		   "  -c, --compress       Enables compression while building.\n"
		   "  -t, --test [file]    Check if CRC files match with data file.\n"
		   "  -x, --extract [file] Extract given file to a directory. If out directory is\n"
		   "                       not given, extract in current working directory.\n"
		   "  -l, --list [file]    List all files of a packed file.\n"
		   );
}

// (--input_path|-p path) (--output|-o) (--test|-t path)

//---------------------------------------------------------------------------//

struct Parameters
{
	enum class Operation
	{
		NONE,
		BUILD_FROM_PATH,
		TEST,
		EXTRACT,
		LIST
	};

	Operation op_id;
	std::string op_param;
	std::string out_path;
	bool compress;

	Parameters()
		: op_id(Operation::NONE)
		, compress(false)
	{}
};

void ExecuteParams(const Parameters* params)
{
	switch (params->op_id)
	{
	case Parameters::Operation::NONE:
		PrintUsage();
		break;
	case Parameters::Operation::BUILD_FROM_PATH:
	{
		gpack::BuildParameters buildparams;
		buildparams.path = params->op_param.c_str();
		buildparams.out = params->out_path.c_str();
		buildparams.compression = params->compress ? gpack::FileEntry::Header::LZ4HC : gpack::FileEntry::Header::UNCOMPRESSED;
		gpack::BuildAndWrite(&buildparams);

		break;
	}
	case Parameters::Operation::TEST:
		gpack::TestFile(params->op_param.c_str());
		break;
	case Parameters::Operation::EXTRACT:
	{
		gpack::ExtractParameters extractparams;
		extractparams.file = params->op_param.c_str();
		extractparams.out_path = params->out_path.c_str();
		gpack::Extract(&extractparams);
		break;
	}
	case Parameters::Operation::LIST:
		gpack::List(params->op_param.c_str());
		break;
	}
}

int main(int argc, char** argv)
{
	g_argc = argc;
	g_argv = argv;

	if (argc < 2)
	{
		PrintUsage();
		return 0;
	}

	Parameters params;

	int argn = 1;
	while (argn < argc)
	{
		if (strcmp(argv[argn], "--input-path") == 0 || strcmp(argv[argn], "-i") == 0)
		{
			if (params.op_id != Parameters::Operation::NONE)
			{
				printf("Error: %s unexpected operation.\n", argv[argn]);
				return -1;
			}

			if ((argn + 1) >= argc)
			{
				printf("Error: operation %s expects one more parameter.\n", argv[argn]);
				return -1;
			}

			params.op_id = Parameters::Operation::BUILD_FROM_PATH;
			params.op_param = std::string(argv[argn + 1]);

			argn += 2;
		}
		else if (strcmp(argv[argn], "--test") == 0 || strcmp(argv[argn], "-t") == 0)
		{
			if (params.op_id != Parameters::Operation::NONE)
			{
				printf("Error: %s unexpected operation.\n", argv[argn]);
				return -1;
			}

			if ((argn + 1) >= argc)
			{
				printf("Error: operation %s expects one more parameter.\n", argv[argn]);
				return -1;
			}

			params.op_id = Parameters::Operation::TEST;
			params.op_param = std::string(argv[argn + 1]);

			argn += 2;
		}
		else if (strcmp(argv[argn], "--output") == 0 || strcmp(argv[argn], "-o") == 0)
		{
			if (!params.out_path.empty())
			{
				printf("Error: %s. Output already defined.", argv[argn]);
				return -1;
			}

			if ((argn + 1) >= argc)
			{
				printf("Error: operation %s expects one more parameter.\n", argv[argn]);
				return -1;
			}

			params.out_path = std::string(argv[argn + 1]);
			argn += 2;
		}
		else if (strcmp(argv[argn], "--compress") == 0 || strcmp(argv[argn], "-c") == 0)
		{
			if (params.compress)
			{
				printf("Error: %s. Compression already defined.\n", argv[argn]);
				return -1;
			}

			params.compress = true;
			argn += 1;
		}
		else if (strcmp(argv[argn], "--extract") == 0 || strcmp(argv[argn], "-x") == 0)
		{
			if (params.compress)
			{
				printf("Error: %s unexpected operation.\n", argv[argn]);
				return -1;
			}

			if ((argn + 1) >= argc)
			{
				printf("Error: operation %s expects one more parameter.\n", argv[argn]);
				return -1;
			}

			params.op_id = Parameters::Operation::EXTRACT;
			params.op_param = std::string(argv[argn + 1]);

			argn += 2;
		}
		else if (strcmp(argv[argn], "--list") == 0 || strcmp(argv[argn], "-l") == 0)
		{
			if (params.compress)
			{
				printf("Error: %s unexpected operation.\n", argv[argn]);
				return -1;
			}

			if ((argn + 1) >= argc)
			{
				printf("Error: operation %s expects one more parameter.\n", argv[argn]);
				return -1;
			}

			params.op_id = Parameters::Operation::LIST;
			params.op_param = std::string(argv[argn + 1]);

			argn += 2;
		}
		else
		{
			printf("Error: unknown parameter %s.\n", argv[argn]);
			return -1;
		}
	}

	ExecuteParams(&params);
	return 0;
}
