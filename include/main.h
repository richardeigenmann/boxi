/***************************************************************************
 *            main.h
 *
 *  Sun Feb 27 21:27:17 2005
 *  Copyright 2005-2009 Chris Wilson
 *  Email chris-boxisource@qwirx.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _MAIN_H
#define _MAIN_H

// #include <stdlib.h>
// #include "dmalloc.h"

#include "wx/defs.h" // for wxID_HIGHEST

enum {
	ID_Main_Frame = wxID_HIGHEST + 1,
	ID_Test_Frame,

	ID_File_New,
	ID_File_Open,
	ID_File_Save,
	ID_File_SaveAs,
	ID_View_Old,
	ID_View_Deleted,
	
	ID_Top_Notebook,
	ID_Backup_Files_Panel,
	ID_Backup_Files_Tree,
	ID_Restore_Files_Panel,
	ID_Compare_Files_Panel,
	ID_Client_Panel,
	ID_Backup_Panel,
	ID_Restore_Panel,
	ID_Compare_Panel,
	ID_Backup_Progress_Panel,
	ID_Restore_Progress_Panel,
	ID_Compare_Progress_Panel,

	ID_General_Setup_Wizard_Button,
	ID_General_Setup_Advanced_Button,
	ID_General_Backup_Button,
	ID_General_Restore_Button,
	ID_General_Compare_Button,

	ID_Backup_Start_Button,
	ID_Backup_Locations_Button,
	ID_Backup_Config_Button,

	ID_BackupLoc_List_Panel,
	ID_BackupLoc_Excludes_Panel,
	ID_Backup_Locations_Tree,
	ID_Backup_LocationsAddButton,
	ID_Backup_LocationsEditButton,
	ID_Backup_LocationsDelButton,
	ID_Backup_LocationsList,
	ID_Backup_LocationNameCtrl,
	ID_Backup_LocationPathCtrl,
	ID_BackupLoc_ExcludeLocList,
	ID_BackupLoc_ExcludeTypeList,
	ID_BackupLoc_ExcludePathCtrl,
	ID_BackupLoc_ExcludeAddButton,
	ID_BackupLoc_ExcludeEditButton,
	ID_BackupLoc_ExcludeRemoveButton,
	ID_BackupLoc_ExcludeList,
	ID_Server_Splitter,
	ID_Server_File_Tree,
	ID_Server_File_List,
	ID_Server_File_RestoreButton,
	ID_Server_File_CompareButton,
	ID_Server_File_DeleteButton,
	
	ID_Local_ServerConnectCheckbox,
	
	ID_Daemon_Connect_Timer,
	ID_Daemon_Start,
	ID_Daemon_Stop,
	ID_Daemon_Kill,
	ID_Daemon_Restart,
	ID_Daemon_Reload,
	ID_Daemon_Sync,
	
	ID_Compare_List,
	ID_Compare_Button,
	
	ID_Function_Source_List,
	ID_Function_Source_Button,
	ID_Function_Dest_Button,
	ID_Function_Start_Button,
	
	ID_Setup_Wizard_Frame,
	ID_Setup_Wizard_Store_Hostname_Ctrl,
	ID_Setup_Wizard_Account_Number_Ctrl,
	ID_Setup_Wizard_New_File_Radio,
	ID_Setup_Wizard_Existing_File_Radio,
	ID_Setup_Wizard_File_Selector_Button,
	ID_Setup_Wizard_File_Name_Text,
	ID_Setup_Wizard_Certificate_File_Name_Text,
	ID_Setup_Wizard_CA_File_Name_Text,
	ID_Setup_Wizard_Backed_Up_Checkbox,
	
	ID_BackupProgress_ErrorList,
	
	ID_Restore_Panel_New_Location_Radio,
	ID_Restore_Panel_New_Location_Text,
	ID_Restore_Panel_To_Date_Checkbox,
	ID_Restore_Panel_Date_Picker,
	ID_Restore_Panel_Hour_Spin,
	ID_Restore_Panel_Min_Spin,
	ID_Restore_Panel_Restore_Later_Checkbox,
	ID_Restore_Panel_Restore_Deleted_Checkbox,
	
	ID_Compare_Panel_Old_Location_Radio,
	ID_Compare_Panel_New_Location_Radio,
	ID_Compare_Panel_New_Location_Text,
	ID_Compare_Panel_New_Location_Button,
	ID_Compare_Panel_All_Locs_Radio,
	ID_Compare_Panel_One_Loc_Radio,
	ID_Compare_Panel_One_Loc_Choice,
	ID_Compare_Panel_Dir_Radio,
	/*
	ID_Compare_Panel_Dir_Local_Path_Text,
	ID_Compare_Panel_Dir_Local_Path_Button,
	ID_Compare_Panel_Dir_Remote_Path_Text,
	ID_Compare_Panel_Dir_Remote_Path_Button,
	*/
	ID_Compare_Panel_Dir_Splitter,
	ID_Compare_Panel_Dir_Local_Tree,
	ID_Compare_Panel_Dir_Remote_Tree,
};

typedef enum
{
	BM_UNKNOWN = 0,
	BM_SETUP_WIZARD_BAD_ACCOUNT_NO,
	BM_SETUP_WIZARD_BAD_STORE_HOST,
	BM_SETUP_WIZARD_NO_FILE_NAME,
	BM_SETUP_WIZARD_FILE_NOT_FOUND,
	BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY,
	BM_SETUP_WIZARD_FILE_NOT_READABLE,
	BM_SETUP_WIZARD_FILE_OVERWRITE,
	BM_SETUP_WIZARD_FILE_NOT_WRITABLE,
	BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND,
	BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE,
	BM_SETUP_WIZARD_BAD_PRIVATE_KEY,
	BM_SETUP_WIZARD_OPENSSL_ERROR,
	BM_SETUP_WIZARD_RANDOM_WARNING,
	BM_SETUP_WIZARD_ACCOUNT_NUMBER_NOT_SET,
	BM_SETUP_WIZARD_PRIVATE_KEY_FILE_NOT_SET,
	BM_SETUP_WIZARD_ERROR_LOADING_PRIVATE_KEY,
	BM_SETUP_WIZARD_ERROR_READING_COMMON_NAME,
	BM_SETUP_WIZARD_WRONG_COMMON_NAME,
	BM_SETUP_WIZARD_FAILED_VERIFY_SIGNATURE,
	BM_SETUP_WIZARD_ERROR_LOADING_OPENSSL_CONFIG,
	BM_SETUP_WIZARD_ERROR_SETTING_STRING_MASK,
	BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR,
	BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID,
	BM_SETUP_WIZARD_MUST_CHECK_THE_BOX_KEYS_BACKED_UP,
	BM_SETUP_WIZARD_DIR_CREATE,
	BM_SETUP_WIZARD_DIR_CREATE_FAILED,
	BM_SETUP_WIZARD_BAD_FILE_PERMISSIONS,
	BM_MAIN_FRAME_CONFIG_CHANGED_ASK_TO_SAVE,
	BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
	BM_MAIN_FRAME_CONFIG_LOAD_FAILED,
	BM_SERVER_CONNECTION_CONNECT_FAILED,
	BM_SERVER_CONNECTION_RETRIEVE_FAILED,
	BM_SERVER_CONNECTION_LIST_FAILED,
	BM_SERVER_CONNECTION_GET_ACCT_FAILED,
	BM_SERVER_CONNECTION_UNDELETE_FAILED,
	BM_SERVER_CONNECTION_DELETE_FAILED,
	BM_BACKUP_FILES_DELETE_LOCATION_QUESTION,
	BM_EXCLUDE_NO_TYPE_SELECTED,
	BM_EXCLUDE_MUST_REMOVE_ALWAYSINCLUDE,
	BM_INTERNAL_PATH_MISMATCH,
	BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
	BM_INTERNAL_EXCLUDE_ENTRY_NOT_SELECTED,
	BM_BACKUP_FAILED_CONFIG_ERROR,
	BM_BACKUP_FAILED_CONNECT_FAILED,
	BM_BACKUP_FAILED_INTERRUPTED,
	BM_BACKUP_FAILED_UNKNOWN_ERROR,
	BM_BACKUP_FAILED_STORE_FULL,
	BM_RESTORE_FAILED_CANNOT_INIT_ENCRYPTION,
	BM_RESTORE_FAILED_NO_STORE_HOSTNAME,
	BM_RESTORE_FAILED_NO_KEYS_FILE,
	BM_RESTORE_FAILED_NO_ACCOUNT_NUMBER,
	BM_RESTORE_FAILED_INVALID_DESTINATION_PATH,
	BM_RESTORE_FAILED_OBJECT_ALREADY_EXISTS,
	BM_RESTORE_FAILED_TO_CREATE_OBJECT,
	BM_TEST_WAIT_FOR_THREAD_FAILED,
}
message_t;

#include <wx/string.h>

class wxPanel;
class wxWindow;
class wxSizer;

void AddParam(wxPanel* panel, const wxChar* label, wxWindow* editor, 
	bool growToFit, wxSizer *sizer);

extern int     g_argc;
extern char ** g_argv;

/*
On most platforms, the encoding of filenames in the config file
is the native encoding, and therefore supported by wxConvBoxi.
However Windows uses Unicode for all filenames, which can therefore
contain characters not supported by the native encoding, so Box Backup
uses UTF8 in all configuration files, and Boxi must do the same.
*/

#ifdef WIN32
	#define wxConvBoxi wxConvUTF8
#else
	#define wxConvBoxi wxConvLibc
#endif

#endif /* _MAIN_H */
