/***************************************************************************
 *            ClientInfoPanel.cc
 *
 *  Tue Mar  1 00:16:12 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
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

#include <wx/wx.h>
#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/notebook.h>

#include "ClientInfoPanel.h"
#include "StaticImage.h"
#include "ParamPanel.h"

BEGIN_EVENT_TABLE(ClientInfoPanel, wxPanel)
	EVT_TEXT(ID_ClientProp_StoreHostname,   
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_AccountNumber,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_KeysFile,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_CertificateFile,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_PrivateKeyFile,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_TrustedCAsFile,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_DataDirectory,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_NotifyScript,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_UpdateStoreInterval,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_MinimumFileAge,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_MaxUploadWait,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_FileTrackingSizeThreshold,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_DiffingUploadSizeThreshold,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_MaximumDiffingTime,
		ClientInfoPanel::OnControlChange)
	EVT_CHECKBOX(ID_ClientProp_ExtendedLogging,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_SyncAllowScript,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_CommandSocket,
		ClientInfoPanel::OnControlChange)
	EVT_TEXT(ID_ClientProp_PidFile,
		ClientInfoPanel::OnControlChange)
END_EVENT_TABLE()

static void LoadBitmap(const struct StaticImage& rImageData, wxBitmap& rDest)
{
	wxMemoryInputStream byteStream(rImageData.data, rImageData.size);
	wxImage img(byteStream, wxBITMAP_TYPE_PNG);
	rDest = wxBitmap(img);
}

ClientInfoPanel::ClientInfoPanel(ClientConfig *pConfig,
	wxWindow* parent, wxWindowID id,
	const wxPoint& pos, const wxSize& size,
	long style, const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	mpConfig = pConfig;
	mpConfig->AddListener(this);
	
	wxBoxSizer *pTabSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer(pTabSizer);
	
	wxScrolledWindow *pVisibleArea = new wxScrolledWindow(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, wxVSCROLL);
	pTabSizer->Add(pVisibleArea, 1, wxGROW | wxALL, 4);
	
	wxBoxSizer *pScrollablePanelSizer = new wxBoxSizer( wxVERTICAL );
	pVisibleArea->SetSizer(pScrollablePanelSizer);
	pVisibleArea->SetScrollRate(0, 1);
	
	wxBoxSizer* pStateSizer = new wxBoxSizer( wxHORIZONTAL );
	pScrollablePanelSizer->Add(pStateSizer, 0, wxGROW | wxALL, 4);

	// duplicate in BackupFilesPanel
	wxInitAllImageHandlers();
	
	LoadBitmap(tick16a_png, mTickBitmap);
	LoadBitmap(cross16a_png, mCrossBitmap);
	
	wxSize BitmapSize(16, 16);
	mpConfigStateBitmap = new wxStaticBitmap(pVisibleArea, -1, mCrossBitmap, 
		wxDefaultPosition, BitmapSize);
	pStateSizer->Add(mpConfigStateBitmap, 0, wxGROW | wxALL, 4);

	mpConfigStateLabel = new wxStaticText(pVisibleArea, -1, wxT("Unknown state"));
	pStateSizer->Add(mpConfigStateLabel, 1, wxGROW | wxALL, 4);
	
	wxNotebook *pClientPropsNotebook = new wxNotebook(pVisibleArea, -1);
	pScrollablePanelSizer->Add(pClientPropsNotebook, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	ParamPanel *pBasicPanel = new ParamPanel(pClientPropsNotebook);
	pClientPropsNotebook->AddPage(pBasicPanel, wxT("Basic"));
	
	ParamPanel *pAdvancedPanel = new ParamPanel(pClientPropsNotebook);
	pClientPropsNotebook->AddPage(pAdvancedPanel, wxT("Advanced"));

	mpStoreHostnameCtrl = pBasicPanel->AddParam(
		wxT("Store Host:"), pConfig->StoreHostname, 
		ID_ClientProp_StoreHostname, 
		FALSE, FALSE, NULL);

	mpAccountNumberCtrl = pBasicPanel->AddParam(
		wxT("Account Number:"), pConfig->AccountNumber, 
		"0x%x", ID_ClientProp_AccountNumber);
		
	mpKeysFileCtrl = pBasicPanel->AddParam(
		wxT("Keys File:"), pConfig->KeysFile, 
		ID_ClientProp_KeysFile, TRUE, FALSE, 
		wxT("Encryption key files (*-FileEncKeys.raw)|"
			"*-FileEncKeys.raw"));
		
	mpCertificateFileCtrl = pBasicPanel->AddParam(
		wxT("Certificate File:"), pConfig->CertificateFile, 
		ID_ClientProp_CertificateFile, TRUE, FALSE,
		wxT("Certificate files (*-cert.pem)|*-cert.pem"));

	mpPrivateKeyFileCtrl = pBasicPanel->AddParam(
		wxT("Private Key File:"), pConfig->PrivateKeyFile, 
		ID_ClientProp_PrivateKeyFile, TRUE, FALSE,
		wxT("Private key files (*-key.pem)|*-key.pem"));

	mpTrustedCAsFileCtrl = pBasicPanel->AddParam(
		wxT("Trusted CAs File:"), pConfig->TrustedCAsFile, 
		ID_ClientProp_TrustedCAsFile, TRUE, FALSE,
		wxT("Server CA certificate (serverCA.pem)|serverCA.pem"));

	mpDataDirectoryCtrl = pBasicPanel->AddParam(
		wxT("Data Directory:"), pConfig->DataDirectory, 
		ID_ClientProp_DataDirectory, FALSE, TRUE, NULL);

	mpCommandSocketCtrl = pBasicPanel->AddParam(
		wxT("Command Socket:"), pConfig->CommandSocket, 
		ID_ClientProp_CommandSocket, TRUE, FALSE, 
		wxT("Command socket (bbackupd.sock)|bbackupd.sock"));

	mpExtendedLoggingCtrl = pBasicPanel->AddParam(
		wxT("Extended Logging:"), pConfig->ExtendedLogging, 
		ID_ClientProp_ExtendedLogging);

	mpUpdateStoreIntervalCtrl = pAdvancedPanel->AddParam(
		wxT("Update Store Interval:"), pConfig->UpdateStoreInterval, 
		"%d", ID_ClientProp_UpdateStoreInterval);
		
	mpMinimumFileAgeCtrl = pAdvancedPanel->AddParam(
		wxT("Minimum File Age:"), pConfig->MinimumFileAge, 
		"%d", ID_ClientProp_MinimumFileAge);

	mpMaxUploadWaitCtrl = pAdvancedPanel->AddParam(
		wxT("Maximum Upload Wait:"), 
		pConfig->MaxUploadWait, "%d", ID_ClientProp_MaxUploadWait);

	mpFileTrackingSizeThresholdCtrl = pAdvancedPanel->AddParam(
		wxT("File Tracking Size Threshold:"), 
		pConfig->FileTrackingSizeThreshold, 
		"%d", ID_ClientProp_FileTrackingSizeThreshold);
		
	mpDiffingUploadSizeThresholdCtrl = pAdvancedPanel->AddParam(
		wxT("Diffing Upload Size Threshold:"), 
		pConfig->DiffingUploadSizeThreshold, 
		"%d", ID_ClientProp_DiffingUploadSizeThreshold);

	mpMaximumDiffingTimeCtrl = pAdvancedPanel->AddParam(
		wxT("Maximum Diffing Time:"), pConfig->MaximumDiffingTime, 
		"%d", ID_ClientProp_MaximumDiffingTime);

	mpNotifyScriptCtrl = pAdvancedPanel->AddParam(
		wxT("Notify Script:"), pConfig->NotifyScript, 
		ID_ClientProp_NotifyScript, TRUE, FALSE,
		wxT("Default script (NotifySysadmin.sh)|NotifySysadmin.sh|"
			"All files (*)|*"));
	
	mpSyncAllowScriptCtrl = pAdvancedPanel->AddParam(
		wxT("Sync Allow Script:"), pConfig->SyncAllowScript, 
		ID_ClientProp_SyncAllowScript, TRUE, FALSE,
		wxT("All files (*)|*"));

	mpPidFileCtrl = pAdvancedPanel->AddParam(wxT("Process ID File:"), 
		pConfig->PidFile, ID_ClientProp_PidFile, TRUE, FALSE,
		wxString(wxT("Client PID files (bbackupd.pid)|bbackupd.pid")));
		
	// pScrollablePanelSizer->SetSizeHints(this);
	
	UpdateConfigState();
}

void ClientInfoPanel::OnControlChange(wxCommandEvent &event)
{
	// only one of these is validly typed, depending on the actual source 
	// of the event, determined by ID below.
	BoundStringCtrl* pString = (BoundStringCtrl*)( event.GetEventObject() );
	BoundIntCtrl*    pInt    = (BoundIntCtrl*   )( event.GetEventObject() );
	BoundBoolCtrl*   pBool   = (BoundBoolCtrl*  )( event.GetEventObject() );
	
	switch(event.GetId()) {
	case ID_ClientProp_StoreHostname:
	case ID_ClientProp_KeysFile:
	case ID_ClientProp_CertificateFile:
	case ID_ClientProp_PrivateKeyFile:
	case ID_ClientProp_TrustedCAsFile:
	case ID_ClientProp_DataDirectory:
	case ID_ClientProp_NotifyScript:
	case ID_ClientProp_SyncAllowScript:
	case ID_ClientProp_CommandSocket:
	case ID_ClientProp_PidFile:
		pString->OnChange();
		break;
	
	case ID_ClientProp_AccountNumber:
	case ID_ClientProp_UpdateStoreInterval:
	case ID_ClientProp_MinimumFileAge:
	case ID_ClientProp_MaxUploadWait:
	case ID_ClientProp_FileTrackingSizeThreshold:
	case ID_ClientProp_DiffingUploadSizeThreshold:
	case ID_ClientProp_MaximumDiffingTime:
		pInt->OnChange();
		break;

	case ID_ClientProp_ExtendedLogging:
		pBool->OnChange();
		break;

	default:
		wxMessageBox(
			wxT("Update to unknown control"), 
			wxT("Internal error"), 
			wxOK | wxICON_ERROR, this);
		
	}
}

void ClientInfoPanel::Reload() 
{
	mpStoreHostnameCtrl             ->Reload();
	mpAccountNumberCtrl             ->Reload();
	mpKeysFileCtrl                  ->Reload();
	mpCertificateFileCtrl           ->Reload();
	mpPrivateKeyFileCtrl            ->Reload();
	mpTrustedCAsFileCtrl            ->Reload();
	mpDataDirectoryCtrl             ->Reload();
	mpNotifyScriptCtrl              ->Reload();
	mpUpdateStoreIntervalCtrl       ->Reload();
	mpMinimumFileAgeCtrl            ->Reload();
	mpMaxUploadWaitCtrl             ->Reload();
	mpFileTrackingSizeThresholdCtrl ->Reload();
	mpDiffingUploadSizeThresholdCtrl->Reload();
	mpMaximumDiffingTimeCtrl        ->Reload();
	mpExtendedLoggingCtrl           ->Reload();
	mpSyncAllowScriptCtrl           ->Reload();
	mpCommandSocketCtrl             ->Reload();
	mpPidFileCtrl                   ->Reload();
}

void ClientInfoPanel::NotifyChange()
{
	UpdateConfigState();
}

void ClientInfoPanel::UpdateConfigState()
{
	wxString msg;
	if (mpConfig->Check(msg))
	{
		mpConfigStateBitmap->SetBitmap(mTickBitmap);
		msg = wxT("Configuration looks OK");
	}
	else 
	{
		mpConfigStateBitmap->SetBitmap(mCrossBitmap);
	}
	mpConfigStateLabel->SetLabel(msg);
}
