// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

#ifndef CFLAGMANAGER_H
#define CFLAGMANAGER_H

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "sh_list.h"
#include "amxmodx.h"
#include "CLibrarySys.h"

class CFlagEntry
{
private:
	ke::AString	m_strName;			// command name ("amx_slap")
	ke::AString	m_strFlags;			// string flags ("a","b")
	ke::AString	m_strComment;		// comment to write ("; admincmd.amxx")
	int	m_iFlags;			// bitmask flags
	bool m_bNeedWritten;	// write this command on map change?
	bool m_bHidden;			// set to 1 when the command is set to "!" access in
							// the .ini file: this means do not process this command
public:
	CFlagEntry(void)
	{
		m_bNeedWritten = false;
		m_iFlags = 0;
		m_bHidden = false;
	};

	inline const bool NeedWritten(void) const { return m_bNeedWritten; };
	inline void SetNeedWritten(const bool i = true) { m_bNeedWritten = i; };
	inline const ke::AString *GetName(void) const { return &m_strName; };
	inline const ke::AString *GetFlags(void) const { return &m_strFlags; };
	inline const ke::AString *GetComment(void) const { return &m_strComment; };
	inline const int Flags(void) const { return m_iFlags; };
	inline void SetName(const char *data) { m_strName = data; };
	inline void SetFlags(const char *flags)
	{
		// If this is a "!" entry then stop
		if (flags && flags[0] == '!')
		{
			SetHidden(true);
			return;
		}

		m_strFlags = flags;
		m_iFlags = UTIL_ReadFlags(flags);
	};
	inline void SetFlags(const int flags)
	{
		m_iFlags = flags;
		char FlagsString[32];
		UTIL_GetFlags(FlagsString, flags);
		m_strFlags = FlagsString;
	};
	inline void SetComment(const char *comment){ m_strComment = comment; };
	inline void SetHidden(const bool i = true) { m_bHidden = i; };
	inline bool IsHidden(void) const { return m_bHidden; };
};
class CFlagManager
{
private:
	List<CFlagEntry*> m_FlagList;
	ke::AString m_strConfigFile;
	struct stat m_Stat;
	bool m_bForceRead;
	bool m_bDisabled;

	void CreateIfNotExist(void) const
	{
		FILE *fp;
		fp = fopen(GetFile(), "r");
		if (!fp)
		{
			// File does not exist, create the header
			fp = fopen(GetFile(), "a");

			if (fp)
			{
				fprintf(fp,"; This file will store the commands used by plugins, and their access level\n");
				fprintf(fp,"; To change the access of a command, edit the flags beside it and then\n");
				fprintf(fp,";   change the server's map.\n;\n");
				fprintf(fp,"; Example: If I wanted to change the amx_slap access to require\n");
				fprintf(fp,";          RCON access (flag \"l\") I would change this:\n");
				fprintf(fp,";          \"amx_slap\"  \"e\" ; admincmd.amxx\n");
				fprintf(fp,";          To this:\n");
				fprintf(fp,";          \"amx_slap\"  \"l\" ; admincmd.amxx\n;\n");
				fprintf(fp,"; To disable a specific command from being used with the command manager\n");
				fprintf(fp,";   and to only use the plugin-specified access set the flag to \"!\"\n;\n");
				fprintf(fp,"; NOTE: The plugin name at the end is just for reference to what plugin\n");
				fprintf(fp,";       uses what commands.  It is ignored.\n\n");
				fclose(fp);
			};
		}
		else
			fclose(fp);
	};

	/**
	 * returns 1 if the timestamp for the file is different than the one we have loaded
	 * 0 otherwise
	 */
	inline int NeedToLoad(void)
	{
		struct stat TempStat;
		stat(GetFile(), &TempStat);

		// if the modified timestamp does not match the stored
		// timestamp than we need to re-read this file.
		// otherwise, ignore the file.
		if (TempStat.st_mtime != m_Stat.st_mtime)
		{
			// save down the modified timestamp
			m_Stat.st_mtime = TempStat.st_mtime;
			return 1;
		};

		return 0;

	};
public:

	CFlagManager(void)
	{
		memset(&m_Stat, 0x0, sizeof(struct stat));
		m_bDisabled = false;
		m_bForceRead = false;
	};

	~CFlagManager(void) {};

	/**
	 * Sets the filename in relation to amxmodx/configs
	 */
	void SetFile(const char *Filename = "cmdaccess.ini");
	inline const char *GetFile(void) const { return m_strConfigFile.chars(); };
	
	/**
	 * Parse the file, and load all entries
	 * Returns 1 on success, 0 on refusal (no need to), and -1 on error
	 */
	const int LoadFile(const int force = 0);

	/**
	 * Checks if the command exists in the list
	 * If it does, it byrefs the flags for it
	 * If it does not, it adds it to the list
	 * These are added from register_*cmd calls
	 */
	void LookupOrAdd(const char *Command, int &Flags, AMX *Plugin);


	/**
	 * Write the commands back to the file
	 */
	void WriteCommands(void);

	/**
	 * Add this straight from the cmdaccess.ini file
	 */
	void AddFromFile(const char *Command, const char *Flags);

	/**
	 * Checks if this command should be added to flagman or not
	 * This is only checked when adding commands from the register_* natives
	 * If an admin manually adds a command to cmdaccess.ini it will be used
	 *   regardless of whatever this function would say should be done with it
	 */
	int ShouldIAddThisCommand(const AMX *amx, const cell *params, const char *cmdname) const;
	void Clear(void);
	void CheckIfDisabled(void);
};

#endif // CFLAGMANAGER_H
