// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#ifndef __AMXXFILE_H__
#define __AMXXFILE_H__

#define MAGIC_HEADER 0x414D5842
#define MAGIC_HEADER2 0x414D5858
#define	MAGIC_VERSION 0x0300

typedef int_fast8_t mint8_t;
typedef int16_t	mint16_t;
typedef int_fast32_t mint32_t;

struct PluginEntry
{
	mint8_t cellsize; // cell size
	int_fast32_t imagesize; // uncompressed image size
	int_fast32_t disksize; // compressed image size
	int_fast32_t memsize; // memory image size
	int_fast32_t offs; // file offset
};

struct BinHeader
{
	int_fast32_t magic;
	mint16_t version;
	mint8_t	numPlugins;
	PluginEntry* plugins;
};

class CAmxxReader
{
public:
	enum Error
	{
		Err_None = 0,
		Err_InvalidParam,
		Err_FileOpen,
		Err_FileRead,
		Err_FileInvalid,
		Err_SectionNotFound,
		Err_DecompressorInit,
		Err_Decompress,
		Err_OldFile,
		Err_Memory
	};
private:
	Error m_Status;
	FILE *m_pFile;
	bool m_OldFile; // old .amx file
	bool m_AmxxFile; // new 'AMXX' header format
	BinHeader m_Bh; // binary header
	int m_Entry; // entry #
	int m_CellSize;
	int m_SectionHdrOffset; // offset to the table in the header that describes the required section
	int m_SectionLength;
public:
	CAmxxReader(const char *filename, const int cellsize);
	~CAmxxReader(void);
	Error GetStatus(void); // get the current status
	size_t GetBufferSize(void); // get the size for the buffer
	Error GetSection(void *buffer); // copy the currently selected section to the buffer
	inline bool IsOldFile(void) const { return m_OldFile; }
};

#endif // __AMXXFILE_H__
