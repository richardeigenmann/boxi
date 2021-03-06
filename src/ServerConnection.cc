/***************************************************************************
 *            ServerConnection.cc
 *
 *  Mon Mar 28 20:56:20 2005
 *  Copyright 2005-2006 Chris Wilson
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

#include "SandBox.h"

#include <sys/types.h>

#include <wx/wx.h>

#define NDEBUG
#include "Box.h"
#include "BackupClientCryptoKeys.h"
#include "BackupStoreConstants.h"
#include "BackupStoreFile.h"
#include "BoxException.h"
#include "BoxPortsAndFiles.h"
#undef NDEBUG

#define TLS_CLASS_IMPLEMENTATION_CPP
#include "ServerConnection.h"
#undef TLS_CLASS_IMPLEMENTATION_CPP

#include "BoxiApp.h"

ServerConnection::ServerConnection(ClientConfig* pConfig)
{
	mpConfig = pConfig;
	mpSocket = NULL;
	mpConnection = NULL;
	mIsConnected = FALSE;
	mIsWritable = FALSE;
	mConnectionIndex = 0;
}

ServerConnection::~ServerConnection()
{
	if (mIsConnected) Disconnect();
}

void ServerConnection::HandleException(message_t code, const wxString& when, 
	BoxException& e)
{
	wxString msg(when);
	msg.append(_(": "));
	
	int type = 0, subtype = 0;
	
	if (mpConnection != NULL)
		mpConnection->GetLastError(type, subtype);
	
	if (type == 1000) {
		msg.append(wxString(ErrorString(type, subtype), wxConvBoxi)); 
	} else {
		msg.Append(wxString(e.what(), wxConvBoxi));
	}
	
	wxGetApp().ShowMessageBox(code, msg, _("Boxi Error"), 
		wxOK | wxICON_ERROR, NULL);

	if (e.GetType() == ConnectionException::ExceptionType &&
		e.GetSubType() == ConnectionException::TLSReadFailed) 
	{
		Disconnect();
	}
}

bool ServerConnection::Connect(bool Writable) 
{
	if (mIsConnected && mIsWritable == Writable) return TRUE;
	
	if (mIsConnected) Disconnect();
	
	bool result;

	try {
		result = Connect2(Writable);
	} catch (BoxException &e) {
		wxString msg(_("Error connecting to server"));
		HandleException(BM_SERVER_CONNECTION_CONNECT_FAILED, msg, e);
		return FALSE;
	}

	if (result) {
		mIsConnected = TRUE;
		mIsWritable = Writable;
		mConnectionIndex++;
	} else {
		mIsConnected = FALSE;
		mIsWritable  = FALSE;
		wxString msg = _("Error connecting to server: ");
		msg.Append(GetErrorMessage());
		wxLogDebug(msg);
	}
	
	return result;
}

bool ServerConnection::InitTlsContext(TLSContext& target, wxString& rErrorMsg)
{
	std::string certFile;
	mpConfig->CertificateFile.GetInto(certFile);

	if (certFile.length() == 0) {
		rErrorMsg = _("You have not configured the Certificate File!");
		return FALSE;
	}

	std::string privKeyFile;
	mpConfig->PrivateKeyFile.GetInto(privKeyFile);

	if (privKeyFile.length() == 0) {
		rErrorMsg = _("You have not configured the Private Key File!");
		return FALSE;
	}

	std::string caFile;
	mpConfig->TrustedCAsFile.GetInto(caFile);

	if (caFile.length() == 0) {
		rErrorMsg = _("You have not configured the Trusted CAs File!");
		return FALSE;
	}

	try {	
		target.Initialise(false /* as client */, certFile.c_str(),
			privKeyFile.c_str(), caFile.c_str());
	} catch (BoxException &e) {
		rErrorMsg = _(
			"There is something wrong with your Certificate "
			"File, Private Key File, or Trusted CAs File. (");
		rErrorMsg.Append(wxString(e.what(), wxConvBoxi));
		rErrorMsg.Append(_(")"));
		return FALSE;
	}
	
	return TRUE;
}

bool ServerConnection::Connect2(bool Writable) 
{
	std::string storeHost;
	mpConfig->StoreHostname.GetInto(storeHost);

	if (storeHost.length() == 0) {
		mErrorMessage = _("You have not configured the Store Hostname!");
		return FALSE;
	}

	std::string keysFile;
	mpConfig->KeysFile.GetInto(keysFile);

	if (keysFile.length() == 0) {
		mErrorMessage = _("You have not configured the Keys File!");
		return FALSE;
	}

	TLSContext tlsContext;
	if (!InitTlsContext(tlsContext, mErrorMessage))
		return FALSE;
	
	// Initialise keys
	BackupClientCryptoKeys_Setup(keysFile.c_str());

	// 2. Connect to server
	mpSocket = new SocketStreamTLS();
	int storePort = BOX_PORT_BBSTORED; 
	mpSocket->Open(tlsContext, Socket::TypeINET, storeHost.c_str(), storePort);
	
	// 3. Make a protocol, and handshake
	mpConnection = new BackupProtocolClient(*mpSocket);
	mpConnection->Handshake();
	
	// Check the version of the server
	{
		std::auto_ptr<BackupProtocolVersion> serverVersion(
			mpConnection->QueryVersion(BACKUP_STORE_SERVER_VERSION));
		
		if (serverVersion->GetVersion() != BACKUP_STORE_SERVER_VERSION)
		{
			mErrorMessage.Printf(_(
				"Wrong server version: "
				"expected %d but found %d"), 
				BACKUP_STORE_SERVER_VERSION, 
				serverVersion->GetVersion());
			return FALSE;
		}
	}

	// Login -- if this fails, the Protocol will exception
	int acctNo;
	if (!mpConfig->AccountNumber.GetInto(acctNo))
		return FALSE;
	
	mpConnection->QueryLogin(acctNo, 
		Writable ? 0 : BackupProtocolLogin::Flags_ReadOnly);

	return TRUE;
}

void ServerConnection::Disconnect() {
	if (mpConnection != NULL) {
		try {
			mpConnection->QueryFinished();
		} catch (BoxException &e) {
			// ignore exceptions - may be disconnected from server
		}
		delete mpConnection;
		mpConnection = NULL;
	}
	
	if (mpSocket != NULL) {
		try {
			mpSocket->Close();
		} catch (BoxException &e) {
			// ignore exceptions - may be disconnected from server
		}
		delete mpSocket;
		mpSocket = NULL;
	}
	
	mIsConnected = FALSE;
}

bool ServerConnection::GetFile(
	int64_t parentDirectoryId,
	int64_t theFileId,
	const char * destFileName)
{
	if (!Connect(FALSE)) return FALSE;
	
	try 
	{
		mpConnection->QueryGetFile(parentDirectoryId, theFileId);
	
		// Stream containing encoded file
		std::auto_ptr<IOStream> objectStream(mpConnection->ReceiveStream());
		
		// Decode it
		BackupStoreFile::DecodeFile(*objectStream, destFileName, 
			mpConnection->GetTimeout());
		
		return TRUE;
	}
	catch (BoxException& e) 
	{
		wxString msg(_("Error retrieving file from server: "));
		msg.Append(wxString(destFileName, wxConvBoxi));
		HandleException(BM_SERVER_CONNECTION_RETRIEVE_FAILED, msg, e);
		return FALSE;
	}	
}

bool ServerConnection::ListDirectory(
	int64_t theDirectoryId, 
	int16_t excludeFlags, 
	BackupStoreDirectory& rDirectoryObject) 
{
	if (!Connect(FALSE)) return FALSE;

	try {
		mpConnection->QueryListDirectory(
			theDirectoryId,
			// both files and directories:
			BackupProtocolListDirectory::Flags_INCLUDE_EVERYTHING,
			excludeFlags,
			// want attributes:
			true);

		// Retrieve the directory from the stream following
		std::auto_ptr<IOStream> dirstream(mpConnection->ReceiveStream());
		
		rDirectoryObject.ReadFromStream(*dirstream, mpConnection->GetTimeout());	
		
		return TRUE;
	} 
	catch (BoxException& e) 
	{
		HandleException(BM_SERVER_CONNECTION_LIST_FAILED,
			_("Error listing directory on server"), e);
		return FALSE;
	}
}

std::auto_ptr<BackupProtocolAccountUsage> ServerConnection::GetAccountUsage() 
{
	std::auto_ptr<BackupProtocolAccountUsage> apUsage;
	
	if (!Connect(FALSE)) return apUsage;

	try 
	{
		apUsage = mpConnection->QueryGetAccountUsage();
	} 
	catch (BoxException &e) 
	{
		HandleException(BM_SERVER_CONNECTION_GET_ACCT_FAILED,
			_("Error getting account information from server"), 
			e);
	}
	
	return apUsage;
}

bool ServerConnection::UndeleteDirectory(int64_t theDirectoryId) {
	if (!Connect(TRUE)) return FALSE;

	try 
	{
		mpConnection->QueryUndeleteDirectory(theDirectoryId);
		return TRUE;
	} 
	catch (BoxException &e) 
	{
		HandleException(BM_SERVER_CONNECTION_UNDELETE_FAILED,
			_("Error undeleting directory on server"), e);
		return FALSE;
	}
}

bool ServerConnection::DeleteDirectory(int64_t theDirectoryId) {
	if (!Connect(TRUE)) return FALSE;

	try 
	{
		mpConnection->QueryDeleteDirectory(theDirectoryId);
		return TRUE;
	} 
	catch (BoxException &e) 
	{
		HandleException(BM_SERVER_CONNECTION_DELETE_FAILED,
			_("Error deleting directory on server"), e);
		return FALSE;
	}
}

static wxCharBuffer buf;

const char * ServerConnection::ErrorString(int type, int subtype) 
{
	if (type == 1000) 
	{
		switch (subtype) 
		{
		case BackupProtocolError::Err_WrongVersion:
			return "Wrong version";
		case BackupProtocolError::Err_NotInRightProtocolPhase:	
			return "Wrong protocol phase";
		case BackupProtocolError::Err_BadLogin:					
			return "Bad login";
		case BackupProtocolError::Err_CannotLockStoreForWriting:	
			return "Cannot lock store for writing";
		case BackupProtocolError::Err_SessionReadOnly:			
			return "Session is read-only";
		case BackupProtocolError::Err_FileDoesNotVerify:			
			return "File verification failed";
		case BackupProtocolError::Err_DoesNotExist:				
			return "Object does not exist";
		case BackupProtocolError::Err_DirectoryAlreadyExists:	
			return "Directory already exists";
		case BackupProtocolError::Err_CannotDeleteRoot:			
			return "Cannot delete root directory";
		case BackupProtocolError::Err_TargetNameExists:			
			return "Target name exists";
		case BackupProtocolError::Err_StorageLimitExceeded:		
			return "Storage limit exceeded";
		case BackupProtocolError::Err_DiffFromFileDoesNotExist:	
			return "Diff From File does not exist";
		case BackupProtocolError::Err_DoesNotExistInDirectory:	
			return "Does not exist in directory";
		case BackupProtocolError::Err_PatchConsistencyError:		
			return "Patch consistency error";
		default:
			mErrorMessage.Printf(
				_("Unknown protocol error: %d/%d"), 
				type, subtype);
			buf = mErrorMessage.mb_str(wxConvBoxi);
			return buf.data();
		}
	} 
	else 
	{
		mErrorMessage.Printf(_("Unknown error: %d/%d"), 
			type, subtype);
		buf = mErrorMessage.mb_str(wxConvBoxi);
		return buf.data();
	}
}
