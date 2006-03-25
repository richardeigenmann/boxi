/***************************************************************************
 *            SetupWizard.cc
 *
 *  Created 2005-12-24 01:05
 *  Copyright 2005-2006 Chris Wilson <chris-boxisource@qwirx.com>
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
 *
 *  Includes (small) portions of OpenSSL by Eric Young (eay@cryptsoft.com) and
 *  Tim Hudson (tjh@cryptsoft.com), under their source redistribution license.
 */

#include <wx/html/htmlwin.h>
#include <wx/progdlg.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "ParamPanel.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestFrame.h"

class SetupWizardPage : public wxWizardPageSimple
{
	private:
	wxHtmlWindow* mpText;
	
	public:
	SetupWizardPage(ClientConfig *pConfig, wxWizard* pParent = NULL,
		const wxString& rText = wxT("No page text defined"))
 	: wxWizardPageSimple(pParent)
	{
		mpConfig = pConfig;

		mpSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(mpSizer);
		mpText = new wxHtmlWindow(this, wxID_ANY, 
			wxDefaultPosition, wxDefaultSize, 
			wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION);
		mpText->SetStandardFonts(10);
		mpText->SetPage(rText);
		mpSizer->Add(mpText, 1, wxGROW | wxALL, 0);
	}
	virtual SetupWizardPage_id_t GetPageId() = 0;
 
	protected:
	ClientConfig* mpConfig;
	wxBoxSizer*   mpSizer;
	
	void SetText(const wxString& rNewText)
	{
		mpText->SetPage(rNewText);
	}

	void ShowError   (const wxString& rErrorMsg, message_t MessageId);
	void ShowSslError(const wxString& rMsgBase,  message_t MessageId);
};

void SetupWizardPage::ShowError(const wxString& rErrorMsg, message_t MessageId)
{
	MessageBoxHarness(MessageId, rErrorMsg, 
		wxT("Boxi Error"), wxICON_ERROR | wxOK, this);
}

void SetupWizardPage::ShowSslError(const wxString& rMsgBase, 
	message_t MessageId)
{
	unsigned long err = ERR_get_error();
	wxString sslErrorMsg(ERR_error_string(err, NULL), wxConvLibc);
	
	wxString msg = rMsgBase;
	
	const char* reason = ERR_reason_error_string(err);
	if (reason != NULL)
	{
		msg.Append(wxT(": "));
		msg.Append(wxString(reason, wxConvLibc));
	}
	
	msg.Append(wxT("\n\nThe full error code is: "));
	msg.Append(wxString(ERR_error_string(err, NULL), wxConvLibc));
	
	ShowError(msg, MessageId);
}	

class IntroPage : public SetupWizardPage
{
	public:
	IntroPage(ClientConfig *pConfig, wxWizard* pParent)
 	: SetupWizardPage(pConfig, pParent, wxT(
		"<html><body><h1>Boxi Setup Wizard</h1>"
		"<p>This wizard can help you configure Boxi for a new server, "
		"or change your settings, as simply as possible.</p>"
		"<p>Please click on the <b>Next</b> button below to continue, "
		"or <b>Cancel</b> if you don't want to reconfigure Boxi.</p>"
		"</body></html>"))
	{ }
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_INTRO; }
};

class AccountPage : public SetupWizardPage
{
	private:
	BoundStringCtrl* mpStoreHostnameCtrl;
	BoundIntCtrl*    mpAccountNumberCtrl;
	
	public:
	AccountPage(ClientConfig *pConfig, wxWizard* pParent)
 	: SetupWizardPage(pConfig, pParent, wxT(
		"<html><body><h1>Account Details</h1>"
		"<p>To use a particular server, you must contact the server's operator "
		"and ask them for an account. If they accept, they will give you a "
		"store hostname and account number. Please enter them here.</p>"
		"</body></html>"))
	{
		wxGridSizer* pLocationSizer = new wxGridSizer(2, 8, 8);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);

		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY,
			wxT("Store hostname:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		mpStoreHostnameCtrl = new BoundStringCtrl(this, 
			ID_Setup_Wizard_Store_Hostname_Ctrl, 
			mpConfig->StoreHostname);
		pLocationSizer->Add(mpStoreHostnameCtrl, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 8);
		
		pLabel = new wxStaticText(this, wxID_ANY, wxT("Account number:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		mpAccountNumberCtrl = new BoundIntCtrl(this, 
			ID_Setup_Wizard_Account_Number_Ctrl, 
			mpConfig->AccountNumber, "%d");
		pLocationSizer->Add(mpAccountNumberCtrl, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 8);
		
		Connect(wxID_ANY, wxEVT_WIZARD_PAGE_CHANGING,
			wxWizardEventHandler(AccountPage::OnWizardPageChanging));
	}

	private:
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;
		
		if (!CheckAccountDetails())
		{
			event.Veto();
		}
	}
	
	bool CheckAccountDetails()
	{
		wxString msg;
		
		if (!mpConfig->CheckStoreHostname(&msg))
		{
			ShowError(msg, BM_SETUP_WIZARD_BAD_STORE_HOST);
			return FALSE;
		}
		
		if (!mpConfig->CheckAccountNumber(&msg))
		{
			ShowError(msg, BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
			return FALSE;
		}
		
		return TRUE;
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_ACCOUNT; }
};

static int numChecked = 0;

void MySslProgressCallback(int p, int n, void* cbinfo)
{
	wxProgressDialog* pProgress = (wxProgressDialog *)cbinfo;
	wxString msg;
	msg.Printf(wxT("Generating your new private key (2048 bits).\n"
			"This may take a minute.\n\n"
			"(%d potential primes checked)"),
			++numChecked);
	pProgress->Update(0, msg);
}

class FileSavingPage : public SetupWizardPage
{
	private:
	wxRadioButton*   mpExistingRadio;
	wxRadioButton*   mpNewRadio;
	FileSelButton*   mpFileSelButton;
	BoundStringCtrl* mpBoundCtrl;
	StringProperty&  mrProperty;
	
	public:
	FileSavingPage(ClientConfig *pConfig, wxWizard* pParent, 
		const wxString& rDescriptionText, const wxString& rFileDesc,
		const wxString& rFileNamePattern, StringProperty& rProperty);
	
	const wxString&   GetFileNameString() { return mFileNameString; }
	const wxFileName& GetFileName()       { return mFileName; }
	void SetFileName(const wxString& rFileName)
	{
		mFileNameString = rFileName;
		mFileName = wxFileName(rFileName);
		mpBoundCtrl->SetValue(rFileName);
	}
	
	private:
	wxString   mFileNameString;
	wxFileName mFileName;
	
	void OnWizardPageChanging(wxWizardEvent& event);

	bool CheckExistingFile();
	virtual bool CheckExistingFileImpl() = 0;
		
	bool CreateNewFile();
	virtual bool CreateNewFileImpl() = 0;
	
	void OnRadioButtonClick(wxCommandEvent& rEvent)
	{
		if (mpNewRadio->GetValue())
		{
			mpFileSelButton->SetFileMustExist(FALSE);
		}
		else
		{
			mpFileSelButton->SetFileMustExist(TRUE);
		}
	}
		
	DECLARE_EVENT_TABLE()
};

FileSavingPage::FileSavingPage(ClientConfig *pConfig, wxWizard* pParent, 
	const wxString& rDescriptionText, const wxString& rFileDesc,
	const wxString& rFileNamePattern, StringProperty& rProperty)
: SetupWizardPage(pConfig, pParent, rDescriptionText),
  mrProperty(rProperty)
{
	wxString label;
	label.Printf(wxT("Existing %s"), rFileDesc.c_str());
	mpExistingRadio = new wxRadioButton(this, 
		ID_Setup_Wizard_Existing_File_Radio, label,
		wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	mpSizer->Add(mpExistingRadio, 0, wxGROW | wxTOP, 8);

	label.Printf(wxT("New %s"), rFileDesc.c_str());
	mpNewRadio = new wxRadioButton(this, 
		ID_Setup_Wizard_New_File_Radio, label, 
		wxDefaultPosition, wxDefaultSize);
	mpSizer->Add(mpNewRadio, 0, wxGROW | wxTOP, 8);
	
	wxSizer* pLocationSizer = new wxBoxSizer(wxHORIZONTAL);
	mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);
	
	label.Printf(wxT("%s file location:"), rFileDesc.c_str());
	wxStaticText* pLabel = new wxStaticText(this, wxID_ANY, label);
	pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
	
	mpBoundCtrl = new BoundStringCtrl(this, 
		ID_Setup_Wizard_File_Name_Text, rProperty);
	pLocationSizer->Add(mpBoundCtrl, 1, 
		wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
	
	wxString fileSpec;
	fileSpec.Printf(wxT("%s (%s)|%s"), rFileDesc.c_str(), 
		rFileNamePattern.c_str(), rFileNamePattern.c_str());
		
	wxString openDialogTitle;
	openDialogTitle.Printf(wxT("Open %s File"), rFileDesc.c_str());
	
	mpFileSelButton = new FileSelButton(this, wxID_ANY, 
		mpBoundCtrl, fileSpec, rFileNamePattern, 
		openDialogTitle);
	pLocationSizer->Add(mpFileSelButton, 0, wxGROW | wxLEFT, 8);
	
	Connect(wxID_ANY, wxEVT_WIZARD_PAGE_CHANGING,
		wxWizardEventHandler(FileSavingPage::OnWizardPageChanging));

	rProperty.GetInto(mFileNameString);
}

void FileSavingPage::OnWizardPageChanging(wxWizardEvent& event)
{
	// always allow moving backwards
	if (!event.GetDirection())
		return;

	bool isOk = FALSE;
	
	if (!mrProperty.GetInto(mFileNameString))
	{
		ShowError(wxT("You must specify a location for the file!"),
			BM_SETUP_WIZARD_NO_FILE_NAME);
	}
	else if (wxFileName::DirExists(mFileNameString))
	{
		ShowError(wxT("The specified file is a directory."),
			BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY);
	}
	else
	{
		mFileName = wxFileName(mFileNameString);
		
		if (mpNewRadio->GetValue())
		{
			isOk = CreateNewFile() && CheckExistingFile();
		}
		else
		{
			isOk = CheckExistingFile();
		}
	}
	
	if (!isOk) 
	{
		event.Veto();
	}
}

bool FileSavingPage::CheckExistingFile()
{	
	if (!wxFileName::FileExists(mFileNameString))
	{
		ShowError(wxT("The specified file was not found."),
			BM_SETUP_WIZARD_FILE_NOT_FOUND);
		return FALSE;
	}

	if (!wxFile::Access(mFileNameString, wxFile::read))
	{
		ShowError(wxT("The specified file is not readable."),
			BM_SETUP_WIZARD_FILE_NOT_READABLE);
		return FALSE;
	}
	
	return CheckExistingFileImpl();
}

bool FileSavingPage::CreateNewFile()
{	
	if (!wxFileName::DirExists(mFileName.GetPath()))
	{
		ShowError(wxT("The directory where you want to create the "
			"file does not exist, so the file cannot be created."),
			BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND);
		return FALSE;
	}
	
	if (wxFileName::FileExists(mFileNameString))
	{
		if (! wxFile::Access(mFileNameString, wxFile::write))
		{
			ShowError(wxT("The specified file is not writable."),
				BM_SETUP_WIZARD_FILE_NOT_WRITABLE);
			return FALSE;
		}

		int result = MessageBoxHarness(
			BM_SETUP_WIZARD_FILE_OVERWRITE,
			wxT(
			"The specified file already exists!\n"
			"\n"
			"If you overwrite an important file, "
			"you may lose access to the server and your backups. "
			"Are you REALLY SURE you want to overwrite it?\n"
			"\n"
			"This operation CANNOT be undone!"), 
			wxT("Boxi Warning"), wxICON_WARNING | wxYES_NO, this);
		
		if (result != wxYES)
		{
			return FALSE;
		}			
	}
	else // does not exist yet
	{
		if (! wxFile::Access(mFileName.GetPath(), wxFile::write))
		{
			ShowError(wxT("The directory where you want to create the "
				"file is read-only, so the file cannot be created."),
				BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE);
			return FALSE;
		}
	}
	
	return CreateNewFileImpl();
}

BEGIN_EVENT_TABLE(FileSavingPage, wxWizardPageSimple)
	EVT_RADIOBUTTON(ID_Setup_Wizard_New_File_Radio, 
		FileSavingPage::OnRadioButtonClick)
	EVT_RADIOBUTTON(ID_Setup_Wizard_Existing_File_Radio, 
		FileSavingPage::OnRadioButtonClick)
END_EVENT_TABLE()

class PrivateKeyPage : public FileSavingPage
{
	public:
	PrivateKeyPage(ClientConfig *pConfig, wxWizard* pParent)
 	: FileSavingPage(pConfig, pParent, wxT(
		"<html><body><h1>Private Key</h1>"
		"<p>You need a private key to prove your identity to the server. "
		"The private key is stored in a file on your hard disk.</p>"
		"<p>If you already have a private key, you should continue to "
		"use it unless it has been compromised (stolen) or the "
		"server administrator tells you to generate a new one.</p>"
		"<p>If you want to use an existing key, select the "
		"<b>Existing Private Key</b> option below, and tell Boxi "
		"where to find the key file.</p>"
		"<p>Otherwise, select the <b>New Private Key</b> option, and tell Boxi "
		"where the new key file should be stored.</p>"
		"</body></html>"),
		wxT("Private Key"), wxT("*-key.pem"), pConfig->PrivateKeyFile)
	{ }
	
	bool CheckExistingFileImpl()
	{
		wxString msg;
		if (!mpConfig->CheckPrivateKeyFile(&msg))
		{
			ShowError(msg, BM_SETUP_WIZARD_BAD_PRIVATE_KEY);
			return FALSE;
		}
		
		return TRUE;
	}

	bool CreateNewFileImpl()
	{	
		BIGNUM* pBigNum = BN_new();
		if (!pBigNum)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BN object"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		RSA* pRSA = RSA_new();
		if (!pRSA)
		{
			ShowSslError(wxT("Failed to create an OpenSSL RSA object"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			BN_free(pBigNum);
			return FALSE;
		}
		
		bool isOk = CreateNewKeyWithBigNumRSA(pBigNum, pRSA);
		
		RSA_free(pRSA);
		BN_free(pBigNum);
		
		return isOk;
	}
	
	bool CreateNewKeyWithBigNumRSA(BIGNUM* pBigNum, RSA* pRSA)
	{
		BIO* pKeyOutBio = BIO_new(BIO_s_file());
		if (!pKeyOutBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO object "
				"for key output"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		bool isOk = CreateNewKeyWithBio(pBigNum, pRSA, pKeyOutBio);
		
		BIO_free_all(pKeyOutBio);
		
		return isOk;
	}
	
	bool CreateNewKeyWithBio(BIGNUM* pBigNum, RSA* pRSA, BIO* pKeyOutBio)
	{
		char* filename = strdup(GetFileNameString().mb_str(wxConvLibc).data());		
		int result = BIO_write_filename(pKeyOutBio, filename);
		free(filename);
		
		if (result <= 0)
		{
			ShowSslError(wxT("Failed to open key file using an OpenSSL BIO"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}

		if (!RAND_status())
		{
			int result = MessageBoxHarness(
				BM_SETUP_WIZARD_RANDOM_WARNING,
				wxT("Your system is not configured with a "
				"random number generator. It may be possible "
				"for an attacker to guess your keys and gain "
				"access to your data. Do you want to continue "
				"anyway?"), wxT("Boxi Warning"), 
				wxICON_WARNING | wxYES_NO, this);
			
			if (result != wxYES)
				return FALSE;
		}
		
		if (!BN_set_word(pBigNum, RSA_F4))
		{
			ShowSslError(wxT("Failed to set key type to RSA F4"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}

		wxProgressDialog progress(wxT("Boxi: Generating Private Key"),
			wxT("Generating your new private key (2048 bits).\n"
			"This may take a minute.\n\n"
			"(0 potential primes checked)"), 2, this, 
			wxPD_APP_MODAL | wxPD_ELAPSED_TIME);
		numChecked = 0;
		
		RSA* pRSA2 = RSA_generate_key(2048, 0x10001, 
			MySslProgressCallback, &progress);
		
		if (!pRSA2)
		{
			ShowSslError(wxT("Failed to generate an RSA key"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}

		progress.Hide();
		
		if (!PEM_write_bio_RSAPrivateKey(pKeyOutBio, pRSA2, NULL, 
			NULL, 0, NULL, NULL))
		{
			ShowSslError(wxT("Failed to write the key "
				"to the specified file"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		return TRUE;
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_PRIVATE_KEY; }
};

class CertRequestPage : public FileSavingPage, ConfigChangeListener
{
	private:
	wxString mKeyFileName;
	int mAccountNumber;
	
	public:
	CertRequestPage(ClientConfig *pConfig, wxWizard* pParent)
 	: FileSavingPage(pConfig, pParent, wxT(
		"<html><body><h1>Certificate Request</h1>"
		"<p>You must send a certificate request to the server operator.</p>"
		"<p>If you already have a certificate request, you can continue to "
		"use it unless the server administrator tells you to generate "
		"a new one.</p>"
		"<p>If you want to use an existing certificate request, select the "
		"<b>Existing Certificate Request</b> option below, and tell Boxi "
		"where to find the file.</p>"
		"<p>Otherwise, select the <b>New Certificate Request</b> option, "
		"and tell Boxi where the new file should be stored.</p>"
		"</body></html>"),
		wxT("Certificate Request"), wxT("*-csr.pem"), pConfig->CertRequestFile)
	{
		mpConfig->AddListener(this);
		SetDefaultValues();
	}
	
	~CertRequestPage() { mpConfig->RemoveListener(this); }
	
	void SetDefaultValues()
	{
		mpConfig->AccountNumber.GetInto(mAccountNumber);
		
		if (mpConfig->CertRequestFile.IsConfigured())
		{
			return;
		}
		
		if (!mpConfig->PrivateKeyFile.IsConfigured())
		{
			return;
		}
		
		if (!mpConfig->PrivateKeyFile.GetInto(mKeyFileName))
		{
			ShowError(wxT("The private key filename is not set, "
				"but it should be!"),
				BM_SETUP_WIZARD_PRIVATE_KEY_FILE_NOT_SET);
			return;
		}
		
		wxString oldSuffix = wxT("-key.pem");
		wxString baseName = mKeyFileName;
		
		if (mKeyFileName.Right(oldSuffix.Length()).IsSameAs(oldSuffix))
		{
			baseName = mKeyFileName.Left(mKeyFileName.Length() - 
				oldSuffix.Length());
		}
		
		wxString reqFileName;
		reqFileName.Printf(wxT("%s-csr.pem"), baseName.c_str());
		
		SetFileName(reqFileName);
	}
	
	void NotifyChange() 
	{
		SetDefaultValues();
	}
	
	bool CheckExistingFileImpl()
	{
		BIO* pRequestBio = BIO_new(BIO_s_file());
		if (!pRequestBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO "
				"to read the certificate request"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		bool isOk = CheckCertRequestWithBio(pRequestBio);
		
		BIO_free(pRequestBio);
		
		return isOk;
	}
	
	bool CheckCertRequestWithBio(BIO* pRequestBio)
	{
		wxCharBuffer buf = GetFileNameString().mb_str(wxConvLibc);
		
		if (BIO_read_filename(pRequestBio, buf.data()) <= 0)
		{
			ShowSslError(wxT("Failed to set the BIO filename to the "
				"certificate request file"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;			
		}
		
		X509_REQ* pRequest = PEM_read_bio_X509_REQ(pRequestBio, NULL, NULL, NULL);
		if (!pRequest)
		{
			ShowSslError(wxT("Failed to read the certificate "
				"request file"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;			
		}
		
		bool isOk = CheckCertRequest(pRequest);
		
		X509_REQ_free(pRequest);
		
		return isOk;
	}
	
	bool CheckCertRequest(X509_REQ* pRequest)
	{
		wxString keyFileName;
		if (!mpConfig->PrivateKeyFile.GetInto(keyFileName))
		{
			ShowError(wxT("The private key filename is not set, "
				"but it should be"),
				BM_SETUP_WIZARD_PRIVATE_KEY_FILE_NOT_SET);
			return FALSE;
		}
		
		wxString msg;
		EVP_PKEY* pKey = LoadKey(keyFileName, &msg);
		if (!pKey)
		{
			ShowError(msg, 
				BM_SETUP_WIZARD_ERROR_LOADING_PRIVATE_KEY);
			return FALSE;
		}

		bool isOk = CheckCertRequestWithKey(pRequest, pKey);
		
		FreeKey(pKey);
		
		return isOk;
	}

	bool CheckCertRequestWithKey(X509_REQ* pRequest, EVP_PKEY* pKey)
	{
		wxString commonNameActual;
		wxString msg;
		if (!GetCommonName(X509_REQ_get_subject_name(pRequest), 
			&commonNameActual, &msg))
		{
			ShowError(msg, BM_SETUP_WIZARD_ERROR_READING_COMMON_NAME);
			return FALSE;
		}
		
		wxString commonNameExpected;
		commonNameExpected.Printf(wxT("BACKUP-%d"), mAccountNumber);
		
		if (!commonNameExpected.IsSameAs(commonNameActual))
		{
			wxString msg;
			msg.Printf(wxT("The certificate request does not match your "
				"account number. The common name of the certificate request "
				"should be '%s', but is actually '%s'."),
				commonNameExpected.c_str(), commonNameActual.c_str());
			ShowError(msg, BM_SETUP_WIZARD_WRONG_COMMON_NAME);
			return FALSE;
		}

		/*		
		EVP_PKEY* pRequestKey = X509_REQ_get_pubkey(pRequest);
		EVP_PKEY_free(pRequestKey);
		*/
		
		if (!X509_REQ_verify(pRequest, pKey))
		{
			ShowSslError(wxT("Failed to verify signature on "
				"certificate request. Perhaps the private key does not match "
				"the certificate request?"),
				BM_SETUP_WIZARD_FAILED_VERIFY_SIGNATURE);
			return FALSE;
		}
		
		return TRUE;
	}

	bool CreateNewFileImpl()
	{
		wxString msg;
		std::auto_ptr<SslConfig> apConfig(SslConfig::Get(&msg));
		if (!apConfig.get())
		{
			ShowError(msg, 
				BM_SETUP_WIZARD_ERROR_LOADING_OPENSSL_CONFIG);
			return FALSE;
		}			

		char* pExtensions = NCONF_get_string(apConfig->GetConf(), "req", 
			"x509_extensions");
		if (pExtensions) 
		{
			/* Check syntax of file */
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, apConfig->GetConf());
			
			if (!X509V3_EXT_add_nconf(apConfig->GetConf(), &ctx, pExtensions, 
				NULL)) 
			{
				wxString msg;
				msg.Printf(wxT("Error in OpenSSL configuration: "
					"Failed to load certificate extensions "
					"section (%s) specified in OpenSSL "
					"configuration file (%s)"), 
					pExtensions, 
					apConfig->GetFileName().c_str());
				ShowSslError(msg, 
					BM_SETUP_WIZARD_ERROR_LOADING_OPENSSL_CONFIG);
				return FALSE;
			}
		}
		else
		{
			ERR_clear_error();
		}
		
		if(!ASN1_STRING_set_default_mask_asc("nombstr")) 
		{
			ShowSslError(wxT("Failed to set the OpenSSL string mask to "
				"'nombstr'"), 
				BM_SETUP_WIZARD_ERROR_SETTING_STRING_MASK);
			return FALSE;
		}

		EVP_PKEY* pKey = LoadKey(mKeyFileName, &msg);
		if (!pKey)
		{
			ShowError(msg, BM_SETUP_WIZARD_ERROR_LOADING_PRIVATE_KEY);
			return FALSE;
		}

		BIO* pRequestBio = BIO_new(BIO_s_file());
		if (!pRequestBio)
		{
			ShowError(wxT("Failed to create an OpenSSL BIO"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			FreeKey(pKey);
			return FALSE;
		}
			
		bool isOk = GenerateCertRequestWithKeyAndBio(apConfig, pKey, 
			pRequestBio);
		
		FreeKey(pKey);
		BIO_free(pRequestBio);
		
		return isOk;
	}
	
	bool GenerateCertRequestWithKeyAndBio(std::auto_ptr<SslConfig>& rapConfig, 
		EVP_PKEY* pKey, BIO* pRequestBio)
	{
		X509_REQ* pRequest = X509_REQ_new();
		if (!pRequest)
		{
			ShowSslError(wxT("Failed to create a new "
				"OpenSSL request object"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		bool isOk = GenerateCertRequestWithObject(rapConfig, pRequest, pKey, 
			pRequestBio);
		
		X509_REQ_free(pRequest);
		
		return isOk;
	}
	
	bool GenerateCertRequestWithObject(std::auto_ptr<SslConfig>& rapConfig, 
		X509_REQ* pRequest, EVP_PKEY* pKey, BIO* pRequestBio)
	{
		if (!X509_REQ_set_version(pRequest,0L))
		{
			ShowSslError(wxT("Failed to set version in "
				"OpenSSL request object"), 
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		int commonNameNid = OBJ_txt2nid("commonName");
		if (commonNameNid == NID_undef)
		{
			ShowSslError(wxT("Failed to find node ID of "
				"X.509 CommonName attribute"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		wxString certSubject;
		certSubject.Printf(wxT("BACKUP-%d"), mAccountNumber);
		
		X509_NAME* pX509SubjectName = X509_REQ_get_subject_name(pRequest);

		unsigned char* subject = (unsigned char *)strdup(
			certSubject.mb_str(wxConvLibc).data());
		int result = X509_NAME_add_entry_by_NID(pX509SubjectName, commonNameNid, 
			MBSTRING_ASC, subject, -1, -1, 0);
		free(subject);
		
		// X509_NAME_free(pX509SubjectName);
		
		if (!result)
		{
			ShowSslError(wxT("Failed to add common name to "
				"certificate request"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		if (!X509_REQ_set_pubkey(pRequest, pKey))
		{
			ShowSslError(wxT("Failed to set public key of "
				"certificate request"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		X509V3_CTX ext_ctx;
		X509V3_set_ctx(&ext_ctx, NULL, NULL, pRequest, NULL, 0);
		X509V3_set_nconf(&ext_ctx, rapConfig->GetConf());
		
		char *pReqExtensions = NCONF_get_string(rapConfig->GetConf(), 
			"section", "req_extensions");
		if (pReqExtensions)
		{
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, rapConfig->GetConf());
			
			if (!X509V3_EXT_REQ_add_nconf(rapConfig->GetConf(), &ctx, 
				pReqExtensions, pRequest)) 
			{
				wxString msg;
				msg.Printf(wxT("Failed to load request extensions "
					"section (%s) specified in OpenSSL configuration file "
					"(%s)"), pReqExtensions, rapConfig->GetFileName().c_str());
				ShowSslError(msg,
					BM_SETUP_WIZARD_ERROR_LOADING_OPENSSL_CONFIG);
				return FALSE;
			}
		}
		else
		{
			ERR_clear_error();
		}

		const EVP_MD* pDigestAlgo = EVP_get_digestbyname("sha1");
		if (!pDigestAlgo)
		{
			ShowSslError(wxT("Failed to find OpenSSL digest "
				"algorithm SHA1"), BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		if (!X509_REQ_sign(pRequest, pKey, pDigestAlgo))
		{
			ShowSslError(wxT("Failed to sign certificate request"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		if (!X509_REQ_verify(pRequest, pKey))
		{
			ShowSslError(wxT("Failed to verify signature on "
				"certificate request"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		wxCharBuffer buf = GetFileNameString().mb_str(wxConvLibc);
		char* filename = strdup(buf.data());
		result = BIO_write_filename(pRequestBio, filename);
		free(filename);
		
		if (result <= 0)
		{
			ShowSslError(wxT("Failed to set certificate request "
				"output filename"), BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		if (!PEM_write_bio_X509_REQ(pRequestBio, pRequest))
		{
			ShowSslError(wxT("Failed to write certificate request file"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		return TRUE;
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_CERT_REQUEST; }
};

class CertificatePage : public SetupWizardPage, ConfigChangeListener
{
	private:
	BoundStringCtrl* mpCertFileCtrl;
	BoundStringCtrl* mpCAFileCtrl;
	
	public:
	CertificatePage(ClientConfig *pConfig,
		wxWizard* pParent = NULL)
 	: SetupWizardPage(pConfig, pParent, wxT(""))
	{
		wxFlexGridSizer* pLocationSizer = new wxFlexGridSizer(3, 8, 8);
		pLocationSizer->AddGrowableCol(1);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);

		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY,
			wxT("Certificate file:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		mpCertFileCtrl = new BoundStringCtrl(this, wxID_ANY, 
			mpConfig->CertificateFile);
		pLocationSizer->Add(mpCertFileCtrl, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 0);
			
		FileSelButton* pFileSel = new FileSelButton(this, wxID_ANY, 
			mpCertFileCtrl, 
			wxT("Certificate files (*-cert.pem)|*-cert.pem"),
			wxT("-cert.pem"));
		pLocationSizer->Add(pFileSel, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
				
		pLabel = new wxStaticText(this, wxID_ANY, wxT("CA file:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		mpCAFileCtrl = new BoundStringCtrl(this, wxID_ANY, 
			mpConfig->TrustedCAsFile);
		pLocationSizer->Add(mpCAFileCtrl, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 0);
		
		pFileSel = new FileSelButton(this, wxID_ANY, mpCAFileCtrl, 
			wxT("CA files (serverCA.pem)|serverCA.pem"),
			wxT("serverCA.pem"));
		pLocationSizer->Add(pFileSel, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                wxWizardEventHandler(
					CertificatePage::OnWizardPageChanging));

		mpConfig->AddListener(this);
		UpdateText();
	}
	
	~CertificatePage() { mpConfig->RemoveListener(this); }

	void NotifyChange() { UpdateText(); }
	
	void UpdateText()
	{
		wxString certReqPath;
		mpConfig->CertRequestFile.GetInto(certReqPath);
		
		wxString keyPath;
		mpConfig->KeysFile.GetInto(keyPath);
		
		wxFileName keyPathName(keyPath);
		
		wxString text;
		text.Printf(wxT(
			"<html><body><h1>Certificate File</h1>"
			"<p>You need a certificate to connect to the server. "
			"The certificate will be supplied by the server operator.</p>"
			"<p>Please follow the operator's instructions to send the "
			"certificate request file (<strong>%s</strong>) to them.</p>"
			"<p>They should send you back two files: a "
			"<strong>certificate</strong> and a <strong>CA</strong> file. "
			"Save both these files in the same directory as your key files "
			"(<strong>%s</strong>).</p>"
			"<p>When you have these files, tell Boxi where to find them "
			"using the boxes below.</p>"),
			certReqPath.c_str(), keyPathName.GetPath().c_str());
		SetText(text);
	}
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;

		bool isOk = CheckServerFiles();
		
		if (!isOk) 
		{
			event.Veto();
		}
	}
	
	bool CheckServerFiles()
	{
		wxString certFile;
		if (!mpConfig->CertificateFile.GetInto(certFile))
		{
			ShowError(wxT("The certificate file is not set"),
				BM_SETUP_WIZARD_NO_FILE_NAME);
			return FALSE;
		}
		
		if (!wxFileName::FileExists(certFile))
		{
			ShowError(wxT("The certificate file does not exist"),
				BM_SETUP_WIZARD_FILE_NOT_FOUND);
			return FALSE;
		}
		
		if (!wxFile::Access(certFile, wxFile::read))
		{
			ShowError(wxT("The certificate file is not readable"),
				BM_SETUP_WIZARD_FILE_NOT_READABLE);
			return FALSE;
		}

		wxString caFile;
		if (!mpConfig->TrustedCAsFile.GetInto(caFile))
		{
			ShowError(wxT("The CA file is not set"),
				BM_SETUP_WIZARD_NO_FILE_NAME);
			return FALSE;
		}
		
		if (!wxFileName::FileExists(caFile))
		{
			ShowError(wxT("The CA file does not exist"),
				BM_SETUP_WIZARD_FILE_NOT_FOUND);
			return FALSE;
		}
		
		if (!wxFile::Access(caFile, wxFile::read))
		{
			ShowError(wxT("The CA file is not readable"),
				BM_SETUP_WIZARD_FILE_NOT_READABLE);
			return FALSE;
		}

		wxString msg;
		if (!mpConfig->CheckCertificateFile(&msg))
		{
			ShowError(msg, BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
			return FALSE;
		}
		
		return TRUE;
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_CERTIFICATE; }
};

class CertExistsPage : public SetupWizardPage
{
	private:
	wxRadioButton* mpExistingRadio;
	wxRadioButton* mpNewRadio;
	wxWizardPage*  mpCertRequestPage;
	wxWizardPage*  mpCertFilePage;
	
	public:
	CertExistsPage(ClientConfig *pConfig, wxWizard* pParent = NULL, 
		CertRequestPage* pCertRequestPage, CertificatePage* pCertFilePage)
 	: SetupWizardPage(pConfig, pParent, wxT(
		"<html><body><h1>Certificate</h1>"
		"<p>You must have a certificate to tell the server operator who you "
		"are. The certificate contains your account number and the "
		"public part of your private key.</p>"
		"<p>You cannot generate the certificate yourself. You must submit a "
		"<b>certificate request</b> to the server operator, who will send "
		"back the certificate.</p>"
		"<p>If you already have a certificate, you can continue to "
		"use it unless the server administrator tells you to generate a new "
		"certificate request, or sends you a new certificate, or you "
		"change your private key.</p>"
		"<p>If you want to use an existing certificate, or a new certificate "
		"which you have already been sent by the server operator, select the "
		"<b>Existing Certificate</b> option below. Otherwise, select the "
		"<b>New Certificate Request</b> option.</p>"
		"</body></html>")),
		mpCertRequestPage(pCertRequestPage),
		mpCertFilePage   (pCertFilePage)

	{
		mpExistingRadio = new wxRadioButton(this, 
			ID_Setup_Wizard_Existing_File_Radio, 
			wxT("Existing Certificate"),
			wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		mpSizer->Add(mpExistingRadio, 0, wxGROW | wxTOP, 8);

		mpNewRadio = new wxRadioButton(this, 
			ID_Setup_Wizard_New_File_Radio, 
			wxT("New Certificate Request"),
			wxDefaultPosition, wxDefaultSize);
		mpSizer->Add(mpNewRadio, 0, wxGROW | wxTOP, 8);
	}

	virtual wxWizardPage* GetNext() const
	{
		if (mpExistingRadio->GetValue())
		{
			return mpCertFilePage;
		}
		else if (mpNewRadio->GetValue())
		{
			return mpCertRequestPage;
		}
		else
		{
			return NULL;
		}
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_CERT_EXISTS; }
};

class CryptoKeyPage : public FileSavingPage, ConfigChangeListener
{
	public:
	CryptoKeyPage(ClientConfig *pConfig, wxWizard* pParent)
 	: FileSavingPage(pConfig, pParent, wxT(
		"<html><body><h1>Encryption Key</h1>"
		"<p>You need an encryption key to protect your backups. "
		"The encryption key is stored in a file on your hard disk.</p>"
		"<p>Without this key, nobody can read your backups, "
		"<strong>NOT EVEN YOU</strong>. You <strong>MUST</strong> "
		"keep a copy of this key somewhere safe, or your backups "
		"will be <strong>useless!</strong></p>"
		"<p>If you already have an encryption key, you should continue to "
		"use it unless it has been compromised (stolen).</p>"
		"<p>If you want to use an existing key, select the "
		"<b>Existing Encryption Key</b> option below, and tell Boxi "
		"where to find the key file.</p>"
		"<p>Otherwise, select the <b>New Encryption Key</b> option, "
		"and tell Boxi where the new key file should be stored.</p>"
		"</body></html>"),
		wxT("Encryption Key"), wxT("*-FileEncKeys.raw"), 
		pConfig->KeysFile)
	{
		mpConfig->AddListener(this);
		SetDefaultValues();
	}
	
	~CryptoKeyPage() { mpConfig->RemoveListener(this); }
	
	void SetDefaultValues()
	{
		if (mpConfig->KeysFile.IsConfigured())
		{
			return;
		}

		wxString privateKeyFileName;
			
		if (!mpConfig->PrivateKeyFile.GetInto(privateKeyFileName))
		{
			return;
		}
		
		wxString oldSuffix = wxT("-key.pem");
		wxString baseName = privateKeyFileName;
		
		if (privateKeyFileName.Right(oldSuffix.Length()).IsSameAs(oldSuffix))
		{
			baseName = privateKeyFileName.Left(privateKeyFileName.Length() - 
				oldSuffix.Length());
		}
		
		wxString encFileName;
		encFileName.Printf(wxT("%s-FileEncKeys.raw"), baseName.c_str());
		SetFileName(encFileName);
	}
	
	void NotifyChange() 
	{
		SetDefaultValues();
	}

	bool CheckExistingFileImpl()
	{
		wxFile keysFile(GetFileName().GetFullPath());
		if (keysFile.Length() != 1024)
		{
			ShowError(wxT("The specified encryption key file "
				"is not valid (should be 1024 bytes long)"),
				BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID);
			return FALSE;
		}

		return TRUE;
	}

	bool CreateNewFileImpl()
	{
		BIO* pKeyOutBio = BIO_new(BIO_s_file());
		if (!pKeyOutBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO "
				"to write the new encryption key"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			return FALSE;
		}
		
		char* filename = strdup(
			GetFileNameString().mb_str(wxConvLibc).data());
		int result = BIO_write_filename(pKeyOutBio, filename);
		free(filename);
		
		if (!result)
		{
			ShowSslError(wxT("Failed to set the filename of the "
				"OpenSSL BIO to write the new encryption key"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			BIO_free(pKeyOutBio);
			return FALSE;
		}
		
		unsigned char bytes[1024];
		if (RAND_bytes(bytes, sizeof(bytes)) <= 0)
		{
			ShowSslError(wxT("Failed to generate random data "
				"for the new encryption key"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			BIO_free(pKeyOutBio);
			return FALSE;
		}
		
		if (BIO_write(pKeyOutBio, bytes, sizeof(bytes)) != sizeof(bytes))
		{
			ShowSslError(wxT("Failed to write random data to the "
				"new encryption key file"),
				BM_SETUP_WIZARD_OPENSSL_ERROR);
			BIO_free(pKeyOutBio);
			return FALSE;
		}

		BIO_flush(pKeyOutBio);
		BIO_free(pKeyOutBio);

		return TRUE;
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_CRYPTO_KEY; }
};

class BackedUpPage : public SetupWizardPage, ConfigChangeListener
{
	private:
	wxCheckBox* mpBackedUp;
	
	public:
	BackedUpPage(ClientConfig *pConfig, wxWizard* pParent)
 	: SetupWizardPage(pConfig, pParent, wxT(
		"<html><body><h1>Back Up Your Keys</h1>"
		"<p>Your backups will be protected by "
		"<strong>strong encryption</strong>. "
		"Without the key, <strong>nobody</strong> can read them: "
		"not us, not you, not the server operator, <strong>NOBODY</strong>.</p>"
		"<p>If your computer is stolen or corrupted, you may lose access to "
		"the encryption key stored on the hard disk. Unless you "
		"<strong>keep safe copies</strong> of this key, you will "
		"<strong>NOT</strong> be able to use your backups.</p>"
		"<p>We recommend that you write your keys onto at least "
		"<strong>TWO</strong> CD-ROMs, keep one safe yourself, "
		"and give the others to a friend for safe keeping.</p>"
		"<p><strong>Please back up your keys NOW!</strong></p>"		
		"</body></html>"))
	{
		mpBackedUp = new wxCheckBox(this, wxID_ANY, 
			wxT("I have backed up my keys in two locations."));
		mpSizer->Add(mpBackedUp, 0, wxGROW | wxTOP, 8);

		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                	wxWizardEventHandler(BackedUpPage::OnWizardPageChanging));
	}

	private:
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;

		if (!mpBackedUp->GetValue())
		{
			ShowError(wxT("You must check the box to say that you "
				"have backed up your keys before proceeding"),
				BM_SETUP_WIZARD_MUST_CHECK_THE_BOX_KEYS_BACKED_UP);
			event.Veto();
		}
	}
	
	virtual SetupWizardPage_id_t GetPageId() { return BWP_BACKED_UP; }
};

SetupWizard::SetupWizard(ClientConfig *config, wxWindow* parent)
: wxWizard(parent, ID_Setup_Wizard_Frame, wxT("Boxi Setup Wizard"), 
	wxNullBitmap, wxDefaultPosition, 
	wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	mpConfig = config;

	CRYPTO_malloc_init();
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	SSL_load_error_strings();

	GetPageAreaSizer()->SetMinSize(500, 400);
	
	mpIntroPage = new IntroPage(config, this);
	
	wxWizardPageSimple* pAccountPage = new AccountPage(config, this);
	wxWizardPageSimple::Chain(mpIntroPage, pAccountPage);
	
	wxWizardPageSimple* pPrivateKeyPage = new PrivateKeyPage(config, this);
	wxWizardPageSimple::Chain(pAccountPage, pPrivateKeyPage);
	
	CertRequestPage* pCertRequestPage = new CertRequestPage(config, this);

	CertificatePage* pCertificatePage = new CertificatePage(config, this);

	wxWizardPageSimple* pCertExistsPage = new CertExistsPage(config, this,
		pCertRequestPage, pCertificatePage);
		
	wxWizardPageSimple::Chain(pPrivateKeyPage,  pCertExistsPage);
	
	pCertRequestPage->SetPrev(pCertExistsPage);
	pCertRequestPage->SetNext(pCertificatePage);
	pCertificatePage->SetPrev(pCertExistsPage);

	wxWizardPageSimple* pCryptoKeyPage = new CryptoKeyPage(config, this);
	wxWizardPageSimple::Chain(pCertificatePage, pCryptoKeyPage);
	
	wxWizardPageSimple* pBackedUpPage = new BackedUpPage(config, this);
	wxWizardPageSimple::Chain(pCryptoKeyPage,  pBackedUpPage);
}

SetupWizardPage_id_t SetupWizard::GetCurrentPageId()
{
	SetupWizardPage* pPage = (SetupWizardPage*)GetCurrentPage();
	return pPage->GetPageId();
}

/*
void SetupWizard::RunModeless()
{
	wxCHECK_MSG( mpIntroPage, false, wxT("can't run empty wizard") );
	
	// This cannot be done sooner, because user can change layout options
	// up to this moment
	FinishLayout();
	
	// can't return false here because there is no old page
	(void)ShowPage(mpIntroPage, true / * forward * /);

	return Show() == wxID_OK;
}
*/
