/***************************************************************************
 *            TestFrame.cc
 *
 *  Sun Jan 22 22:37:04 2006
 *  Copyright  2006  Chris Wilson
 *  chris-boxisource@qwirx.com
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>

#include "MainFrame.h"
#include "SetupWizard.h"
#include "TestFrame.h"

DECLARE_EVENT_TYPE(CREATE_WINDOW_COMMAND, -1)
DECLARE_EVENT_TYPE(TEST_FINISHED_EVENT,   -1)

DEFINE_EVENT_TYPE (CREATE_WINDOW_COMMAND)
DEFINE_EVENT_TYPE (TEST_FINISHED_EVENT)

bool TestFrame::mTestsInProgress = FALSE;

class TestCase : private wxThread
{
	private:
	TestFrame* mpTestFrame;
	
	void* Entry();
	virtual void RunTest() = 0;

	protected:
	bool WaitForMain();
	MainFrame* OpenMainFrame();
	void CloseWindow(wxWindow* pWindow);
	void ClickButton(wxWindow* pWindow);
	void ClickRadio (wxWindow* pWindow);
	wxTextCtrl* GetTextCtrl(wxWindow* pWindow);
	TestFrame*  GetTestFrame() { return mpTestFrame; }
	
	public:
	TestCase(TestFrame* pTestFrame) 
	: wxThread(wxTHREAD_DETACHED), mpTestFrame(pTestFrame) { }
	void Start()
	{
		assert(Create() == wxTHREAD_NO_ERROR);
		assert(Run()    == wxTHREAD_NO_ERROR);
	}
};

void* TestCase::Entry()
{
	RunTest();
	
	wxCommandEvent fini(TEST_FINISHED_EVENT, mpTestFrame->GetId());
	mpTestFrame->AddPendingEvent(fini);
	
	return 0;
}

bool TestCase::WaitForMain()
{
	if (TestDestroy()) return FALSE;
	mpTestFrame->WaitForIdle();
	return TRUE;
}

MainFrame* TestCase::OpenMainFrame()
{
	wxCommandEvent open(CREATE_WINDOW_COMMAND, mpTestFrame->GetId());
	mpTestFrame->AddPendingEvent(open);	
	assert(WaitForMain());
	
	MainFrame* pMainFrame = mpTestFrame->GetMainFrame();
	assert(pMainFrame);
	
	return pMainFrame;
}

void TestCase::CloseWindow(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxTopLevelWindow* pTopLevelWindow = 
		wxDynamicCast(pWindow, wxTopLevelWindow);
	assert(pTopLevelWindow);
	
	wxWindowID id = pTopLevelWindow->GetId();
	wxCloseEvent close(wxEVT_CLOSE_WINDOW, id);
	close.SetEventObject(pTopLevelWindow);
	pTopLevelWindow->AddPendingEvent(close);
	assert(WaitForMain());
	
	while ( (pWindow = wxWindow::FindWindowById(id)) )
	{
		assert(WaitForMain());
	}
	assert(!pWindow);	
}

void TestCase::ClickButton(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxButton* pButton = wxDynamicCast(pWindow, wxButton);
	assert(pButton);

	wxCommandEvent click(wxEVT_COMMAND_BUTTON_CLICKED, pButton->GetId());
	click.SetEventObject(pButton);
	pButton->AddPendingEvent(click);
	assert(WaitForMain());
}

void TestCase::ClickRadio(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxRadioButton* pButton = wxDynamicCast(pWindow, wxRadioButton);
	assert(pButton);

#ifdef __WXGTK__
	// no way to send an event to change the current value
	// this not thread safe and must be fixed - TODO FIXME
	pButton->SetValue(TRUE);
#else
	wxCommandEvent click(wxEVT_COMMAND_RADIOBUTTON_SELECTED, 
		pButton->GetId());
	click.SetEventObject(pButton);
	click.SetInt(1);
	pButton->AddPendingEvent(click);
	assert(WaitForMain());
#endif
	
	assert(pButton->GetValue());
}

wxTextCtrl* TestCase::GetTextCtrl(wxWindow* pWindow)
{
	assert(pWindow);
	
	wxTextCtrl *pTextCtrl = wxDynamicCast(pWindow, wxTextCtrl);
	assert(pTextCtrl);
	
	return pTextCtrl;
}

class TestCanOpenAndCloseMainWindow : public TestCase
{
	virtual void RunTest()
	{
		MainFrame* pMainFrame = OpenMainFrame();
		CloseWindow(pMainFrame);
	}
	public:
	TestCanOpenAndCloseMainWindow(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }
};

class TestCanOpenAndCloseSetupWizard : public TestCase
{
	virtual void RunTest()
	{
		MainFrame* pMainFrame = OpenMainFrame();
		ClickButton(pMainFrame->FindWindow(ID_General_Setup_Wizard_Button));
		CloseWindow(wxWindow::FindWindowById(ID_Setup_Wizard_Frame));
		CloseWindow(pMainFrame);
	}
	public:
	TestCanOpenAndCloseSetupWizard(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }
};

class TestCanRunThroughSetupWizardCreatingEverything : public TestCase
{
	virtual void RunTest()
	{
		MainFrame* pMainFrame = OpenMainFrame();
		
		assert(!wxWindow::FindWindowById(ID_Setup_Wizard_Frame));
		ClickButton(pMainFrame->FindWindow(ID_General_Setup_Wizard_Button));
		
		SetupWizard* pWizard = (SetupWizard*)
			wxWindow::FindWindowById(ID_Setup_Wizard_Frame);
		assert(pWizard);
		assert(pWizard->GetCurrentPageId() == BWP_INTRO);
		
		wxWindow* pForwardButton = pWizard->FindWindow(wxID_FORWARD);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
		
		wxTextCtrl* pStoreHost;
		wxTextCtrl* pAccountNo;
		{
			wxWizardPage* pPage = pWizard->GetCurrentPage();
			pStoreHost = GetTextCtrl(
				pPage->FindWindow(
					ID_Setup_Wizard_Store_Hostname_Ctrl));
			pAccountNo = GetTextCtrl(
				pPage->FindWindow(
					ID_Setup_Wizard_Account_Number_Ctrl));
		}
		assert(pStoreHost->GetValue().IsSameAs(wxEmptyString));
		assert(pAccountNo->GetValue().IsSameAs(wxEmptyString));
		
		MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_STORE_HOST, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);

		ClientConfig* pConfig = pMainFrame->GetConfig();
		assert(pConfig);
		assert(!pConfig->StoreHostname.IsConfigured());
		
		pStoreHost->SetValue(wxT("no-such-host"));
		MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_STORE_HOST, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
		assert(pConfig->StoreHostname.IsConfigured());
		
		pStoreHost->SetValue(wxT("localhost"));
		MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_ACCOUNT_NO, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
		
		pAccountNo->SetValue(wxT("localhost")); // invalid number
		MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_ACCOUNT_NO, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_ACCOUNT);
		assert(!pConfig->AccountNumber.IsConfigured());
		
		pAccountNo->SetValue(wxT("-1"));
		MessageBoxSetResponse(BM_SETUP_WIZARD_BAD_ACCOUNT_NO, wxOK);
		ClickButton(pForwardButton);
		assert(pConfig->AccountNumber.IsConfigured());

		pAccountNo->SetValue(wxT("1"));
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		wxString StoredHostname;
		assert(pConfig->StoreHostname.GetInto(StoredHostname));
		assert(StoredHostname.IsSameAs(wxT("localhost")));
		
		int AccountNumber;
		assert(pConfig->AccountNumber.GetInto(AccountNumber));
		assert(AccountNumber == 1);
		
		ClickRadio(pWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio));
		MessageBoxSetResponse(BM_SETUP_WIZARD_NO_FILE_NAME, wxOK);
		ClickButton(pForwardButton);
		
		wxFileName tempdir;
		tempdir.AssignTempFileName(wxT("boxi-tempdir."));
		assert(wxRemoveFile(tempdir.GetFullPath()));
		
		wxTextCtrl* pFileName = GetTextCtrl(
			pWizard->GetCurrentPage()->FindWindow(
				ID_Setup_Wizard_File_Name_Text));
		assert(pFileName->GetValue().IsSameAs(wxEmptyString));
		
		assert(tempdir.Mkdir(0700));
		assert(tempdir.DirExists());
		pFileName->SetValue(tempdir.GetFullPath());
		
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		
		wxFileName existingfile(tempdir.GetFullPath(), wxT("existing"));
		pFileName->SetValue(existingfile.GetFullPath());
		
		wxCharBuffer buf = existingfile.GetFullPath().mb_str(wxConvLibc);
		
		/*
		chmod(buf.data(), 0000);
		assert(!wxFile::Access(existingfile.GetFullPath(), wxFile::read));
		
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_NOT_READABLE, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);
		*/
		
		chmod(buf.data(), 0400);
		assert(!wxFile::Access(existingfile.GetFullPath(), wxFile::write));
		
		MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_NOT_WRITABLE, wxOK);
		ClickButton(pForwardButton);
		assert(pWizard->GetCurrentPageId() == BWP_PRIVATE_KEY);

		assert(wxRemoveFile(existingfile.GetFullPath()));
		assert(tempdir.Rmdir());

		CloseWindow(pWizard);
		
		MessageBoxSetResponse(BM_MAIN_FRAME_CONFIG_CHANGED_ASK_TO_SAVE,
			wxNO);
		CloseWindow(pMainFrame);
	}
	public:
	TestCanRunThroughSetupWizardCreatingEverything(TestFrame* pTestFrame) 
	: TestCase(pTestFrame) { }
};

BEGIN_EVENT_TABLE(TestFrame, wxFrame)
	EVT_IDLE (TestFrame::OnIdle)
	EVT_COMMAND(wxID_ANY, CREATE_WINDOW_COMMAND, 
		TestFrame::OnCreateWindowCommand)
	EVT_COMMAND(wxID_ANY, TEST_FINISHED_EVENT, 
		TestFrame::OnTestFinishedEvent)
END_EVENT_TABLE()

TestFrame::TestFrame(const wxString PathToExecutable)
:	wxFrame(NULL, wxID_ANY, wxT("Boxi (test mode)")),
	mPathToExecutable(PathToExecutable),
	mTestThreadIsWaiting(FALSE),
	mMainThreadIsIdle(mMutex),
	mpMainFrame(NULL)
{
	mTests.push_back(new TestCanOpenAndCloseMainWindow(this));
	mTests.push_back(new TestCanOpenAndCloseSetupWizard(this));
	mTests.push_back(new TestCanRunThroughSetupWizardCreatingEverything(this));

	mipCurrentTest = mTests.begin();
	mTestsInProgress = TRUE;
	(*mipCurrentTest)->Start();
}

void TestFrame::OnIdle(wxIdleEvent& rEvent)
{
	wxMutexLocker lock(mMutex);
	
	if (mTestThreadIsWaiting)
	{
		mMainThreadIsIdle.Signal();
		mTestThreadIsWaiting = FALSE;
	}
}

class CreateWindowCommandData : wxClientData
{
	private:
	wxString mFileName;
	bool mHaveFileName;
	
	CreateWindowCommandData(wxChar* pFileNameChars)
	{
		if (pFileNameChars)
		{
			mFileName = pFileNameChars;
			mHaveFileName = TRUE;
		}
		else
		{
			mHaveFileName = FALSE;
		}
	}
};

void TestFrame::OnCreateWindowCommand(wxCommandEvent& rEvent)
{
	wxWindow* pWindow = wxWindow::FindWindowById(ID_Main_Frame);
	assert(!pWindow);
	
	mpMainFrame = new MainFrame 
	(
		NULL, mPathToExecutable,
		wxPoint(50, 50), wxSize(600, 500)
	);
	mpMainFrame->Show(TRUE);
}

void TestFrame::OnTestFinishedEvent(wxCommandEvent& rEvent)
{
	mipCurrentTest++;
	
	if (mipCurrentTest != mTests.end())
	{
		(*mipCurrentTest)->Start();
	}
	else
	{
		mTestsInProgress = FALSE;
	}
}

/*
void TestThread::Entry2()
{
	printf("waiting...\n");
	
	if (!WaitForMain()) return;
	
	printf("started.\n");
	
	wxCommandEvent wizard(wxEVT_COMMAND_BUTTON_CLICKED, 
		ID_General_Setup_Wizard_Button);
	wxWindow::FindWindowById(ID_General_Setup_Wizard_Button)->
		AddPendingEvent(wizard);
	if (!WaitForMain()) return;
	
	CloseWindow(ID_Setup_Wizard_Frame);
	if (!WaitForMain()) return;
		
	CloseWindow(mpMainFrame->GetId());
	if (!WaitForMain()) return;
	
	printf("terminating...\n");
	while (WaitForMain()) { }

	printf("terminated.\n");
	return;
}
*/

void TestFrame::WaitForIdle()
{
	wxMutexLocker lock(mMutex);
	assert(!mTestThreadIsWaiting);
	
	mTestThreadIsWaiting = TRUE;
	::wxWakeUpIdle();
	mMainThreadIsIdle.Wait();
}

static int expMessageId   = 0;
static int expMessageResp = -1;

int MessageBoxHarness
(
	message_t messageId,
	const wxString& message,
	const wxString& caption,
	long style,
	wxWindow *parent
)
{
	if (TestFrame::IsTestsInProgress())
	{
		assert(expMessageId);
	}
	
	if (expMessageId == 0)
	{
		
		// fall through
		return wxMessageBox(message, caption, style, parent);
	}
	
	assert (messageId == expMessageId);
	int response = expMessageResp;
	
	expMessageId = 0;
	expMessageResp = -1;
	
	return response;
}

void MessageBoxSetResponse(message_t messageId, int response)
{
	assert(expMessageId == 0);
	expMessageId   = messageId;
	expMessageResp = response;
}