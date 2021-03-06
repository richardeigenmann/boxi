/***************************************************************************
 *            BackupLocationsPanel.cc
 *
 *  Tue Mar  1 00:26:30 2005
 *  Copyright 2005-2008 Chris Wilson
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

#include <wx/dynarray.h>
#include <wx/filename.h>
#include <wx/notebook.h>
// #include <wx/mstream.h>

#include "BackupLocationsPanel.h"
#include "FileTree.h"
#include "MainFrame.h"
#include "BoxiApp.h"

class BackupTreeNode : public LocalFileNode 
{
	private:
	BoxiLocation* mpLocation;
	const BoxiExcludeEntry* mpExcludedBy;
	const BoxiExcludeEntry* mpIncludedBy;
	ClientConfig* mpConfig;
	int mIconId;
	
	public:
	BackupTreeNode(ClientConfig* pConfig,   const wxString& path);
	BackupTreeNode(BackupTreeNode* pParent, const wxString& path);
	virtual LocalFileNode* CreateChildNode(LocalFileNode* pParent, 
		const wxString& rPath);

	BoxiLocation*           GetLocation()   const { return mpLocation; }
	ClientConfig*           GetConfig()     const { return mpConfig; }
	const BoxiExcludeEntry*   GetExcludedBy() const { return mpExcludedBy; }
	const BoxiExcludeEntry*   GetIncludedBy() const { return mpIncludedBy; }

	virtual int UpdateState(FileImageList& rImageList, bool updateParents);
};

BackupTreeNode::BackupTreeNode(ClientConfig* pConfig, const wxString& path)
: LocalFileNode  (path),
  mpLocation     (NULL),
  mpExcludedBy   (NULL),
  mpIncludedBy   (NULL),
  mpConfig       (pConfig),
  mIconId        (-1)
{ }

BackupTreeNode::BackupTreeNode(BackupTreeNode* pParent, const wxString& path) 
: LocalFileNode  (pParent, path),
  mpLocation     (pParent->GetLocation()),
  mpExcludedBy   (pParent->GetExcludedBy()),
  mpIncludedBy   (pParent->GetIncludedBy()),
  mpConfig       (pParent->GetConfig()),
  mIconId        (-1)
{ }

LocalFileNode* BackupTreeNode::CreateChildNode(LocalFileNode* pParent, 
	const wxString& rPath)
{
	return new BackupTreeNode((BackupTreeNode *)pParent, rPath);
}

int BackupTreeNode::UpdateState(FileImageList& rImageList, bool updateParents) 
{
	int iconId = LocalFileNode::UpdateState(rImageList, updateParents);
	
	BackupTreeNode* pParentNode = (BackupTreeNode*)GetParentNode();
	
	// by default, inherit our include/exclude state
	// from our parent node, if we have one
	
	if (pParentNode) 
	{
		mpLocation     = pParentNode->mpLocation;
		mpExcludedBy   = pParentNode->mpExcludedBy;
		mpIncludedBy   = pParentNode->mpIncludedBy;
	} 
	else 
	{
		mpLocation   = NULL;
		mpExcludedBy = NULL;
		mpIncludedBy = NULL;
	}
		
	const BoxiLocation::List& rLocations = mpConfig->GetLocations();

	if (!mpLocation) 
	{
		// determine whether or not this node's path
		// is inside a backup location.
	
		for (BoxiLocation::ConstIterator
			pLoc  = rLocations.begin();
			pLoc != rLocations.end(); pLoc++)
		{
			const wxString& rPath = pLoc->GetPath();
			// std::cout << "Compare " << mFullPath 
			//	<< " against " << rPath << "\n";
			if (GetFullPath().CompareTo(rPath) == 0) 
			{
				// std::cout << "Found location: " << pLoc->GetName() << "\n";
				mpLocation = mpConfig->GetLocation(*pLoc);
				break;
			}
		}
	}
	
	if (mpLocation && !(GetFullPath().StartsWith(mpLocation->GetPath())))
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_PATH_MISMATCH,
			_("Full path does not start with location path!"),
			_("Boxi Error"), wxOK | wxICON_ERROR, NULL);
	}
	
	/*	
	wxLogDebug(_("Checking %s against exclude list for %s"),
		mFullPath.c_str(), mpLocation->GetPath().c_str());
	*/
		
	bool matchedRule = false;
	
	if (!mpLocation)
	{
		// if this node doesn't belong to a location, then by definition
		// our parent node doesn't have one either, so the inherited
		// values are fine, leave them alone.
	}
	else if (mpExcludedBy && !mpIncludedBy)
	{
		// A node whose parent is excluded is also excluded,
		// since Box Backup will never scan it.
	}
	else
	{
		mpLocation->GetExcludedState(
			GetFullPath(), IsDirectory(), 
			&mpExcludedBy, &mpIncludedBy, &matchedRule);
	}

	// now decide which icon to use, based on the resulting state.
	
	if (mpIncludedBy != NULL)
	{
		if (matchedRule)
		{
			iconId = rImageList.GetAlwaysImageId();
		}
		else
		{
			iconId = rImageList.GetAlwaysGreyImageId();
		}
	}
	else if (mpExcludedBy != NULL)
	{
		if (matchedRule)
		{
			iconId = rImageList.GetCrossedImageId();
		}
		else
		{
			iconId = rImageList.GetCrossedGreyImageId();
		}
	}
	else if (mpLocation != NULL)
	{
		if (matchedRule)
		{
			iconId = rImageList.GetCheckedImageId();
		}
		else
		{
			iconId = rImageList.GetCheckedGreyImageId();
		}
	}
	else
	{
		// this node is not included in any location, but we need to
		// check whether our path is a prefix to any configured location,
		// to decide whether to display a blank or a partially included icon.
		
		const BoxiLocation::List& rLocations = mpConfig->GetLocations();
		wxFileName thisNodePath(GetFullPath());
		bool found = false;
		
		for (BoxiLocation::ConstIterator
			pLoc  = rLocations.begin();
			pLoc != rLocations.end() && !found; pLoc++)
		{
			#ifdef WIN32
			if (IsRoot())
			{
				// root node is always included if there is
				// at least one location, even though its
				// empty path (for My Computer) does not 
				// match in the algorithm below.
				found = true;
			}
			#endif
				
			wxFileName locPath(pLoc->GetPath(), wxT(""));
			
			while(locPath.GetDirCount() > 0 && !found)
			{
				locPath.RemoveLastDir();
				
				wxString fn1 = locPath.GetFullPath();
				wxString fn2 = thisNodePath.GetFullPath();
				
				while (fn1.Length() > 1 && 
					wxFileName::IsPathSeparator(fn1.GetChar(fn1.Length() - 1)))
				{
					fn1.Truncate(fn1.Length() - 1);
				}

				while (fn2.Length() > 1 && 
					wxFileName::IsPathSeparator(fn2.GetChar(fn2.Length() - 1)))
				{
					fn2.Truncate(fn2.Length() - 1);
				}
				
				found = fn1.IsSameAs(fn2);
			}
		}
				
		if (found)
		{
			iconId = rImageList.GetPartialImageId();
		}
	}

	return iconId;
}

class EditorPanel : public wxPanel, ConfigChangeListener
{
	protected:
	ClientConfig* mpConfig;
	wxBoxSizer*   mpTopSizer;
	wxListBox*    mpList;
	wxButton*     mpAddButton;
	wxButton*     mpEditButton;
	wxButton*     mpRemoveButton;
	wxStaticBoxSizer* mpListBoxSizer;
	wxStaticBoxSizer* mpDetailsBoxSizer;
	wxGridSizer*      mpDetailsParamSizer;
	
	public:
	EditorPanel(wxWindow* pParent, ClientConfig *pConfig, wxWindowID id);
	virtual ~EditorPanel() { mpConfig->RemoveListener(this); }
	void NotifyChange(); /* ConfigChangeListener interface */
	
	protected:
	virtual void PopulateList() = 0;
	virtual void UpdateEnabledState() = 0;
	virtual void OnSelectListItem   (wxCommandEvent& rEvent) = 0;
	virtual void OnClickButtonAdd   (wxCommandEvent& rEvent) = 0;
	virtual void OnClickButtonEdit  (wxCommandEvent& rEvent) = 0;
	virtual void OnClickButtonRemove(wxCommandEvent& rEvent) = 0;
	void PopulateLocationList(wxControlWithItems* pTargetList);
	
	DECLARE_EVENT_TABLE()
};

EditorPanel::EditorPanel(wxWindow* pParent, ClientConfig *pConfig, wxWindowID id)
: wxPanel(pParent, id),
  mpConfig(pConfig)
{
	mpConfig->AddListener(this);

	mpTopSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mpTopSizer);
	
	mpListBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT(""));
	mpTopSizer->Add(mpListBoxSizer, 1, wxGROW | wxALL, 8);
	
	mpList = new wxListBox(this, ID_Backup_LocationsList);
	mpListBoxSizer->Add(mpList, 1, wxGROW | wxALL, 8);
	
	mpDetailsBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT(""));
	mpTopSizer->Add(mpDetailsBoxSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpDetailsParamSizer = new wxGridSizer(2, 8, 8);
	mpDetailsBoxSizer->Add(mpDetailsParamSizer, 0, wxGROW | wxALL, 8);

	wxSizer* pButtonSizer = new wxGridSizer( 1, 0, 8, 8 );
	mpDetailsBoxSizer->Add(pButtonSizer, 0, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	mpAddButton = new wxButton(this, ID_Backup_LocationsAddButton, _("Add"));
	pButtonSizer->Add(mpAddButton, 1, wxGROW, 0);
	mpAddButton->Disable();
	
	mpEditButton = new wxButton(this, ID_Backup_LocationsEditButton, _("Edit"));
	pButtonSizer->Add(mpEditButton, 1, wxGROW, 0);
	mpEditButton->Disable();
	
	mpRemoveButton = new wxButton(this, ID_Backup_LocationsDelButton, _("Remove"));
	pButtonSizer->Add(mpRemoveButton, 1, wxGROW, 0);
	mpRemoveButton->Disable();
}

void EditorPanel::NotifyChange()
{
	PopulateList();
	UpdateEnabledState();
}	

void EditorPanel::PopulateLocationList(wxControlWithItems* pTargetList)
{
	BoxiLocation* pSelectedLoc = NULL;
	{
		long selected = pTargetList->GetSelection();
		if (selected != wxNOT_FOUND)
		{
			pSelectedLoc = (BoxiLocation*)( pTargetList->GetClientData(selected) );
		}
	}
	
	pTargetList->Clear();
	const BoxiLocation::List& rLocs = mpConfig->GetLocations();
	
	for (BoxiLocation::ConstIterator pLoc = rLocs.begin();
		pLoc != rLocs.end(); pLoc++)
	{
		const BoxiExcludeList& rExclude = pLoc->GetExcludeList();
		
		wxString locString;
		locString.Printf(_("%s -> %s"), 
			pLoc->GetPath().c_str(), pLoc->GetName().c_str());
		
		const BoxiExcludeEntry::List& rExcludes = rExclude.GetEntries();
		
		if (rExcludes.size() > 0)
		{
			locString.Append(_(" ("));

			int size = rExcludes.size();
			for (BoxiExcludeEntry::ConstIterator pExclude = rExcludes.begin();
				pExclude != rExcludes.end(); pExclude++)
			{
				wxString entryDesc(pExclude->ToString().c_str(), wxConvBoxi);
				locString.Append(entryDesc);
				
				if (--size)
				{
					locString.Append(_(", "));
				}
			}

			locString.Append(_(")"));
		}
			
		int newIndex = pTargetList->Append(locString);
		pTargetList->SetClientData(newIndex, mpConfig->GetLocation(*pLoc));
		
		if (&(*pLoc) == pSelectedLoc) 
		{
			pTargetList->SetSelection(newIndex);
		}
	}
	
	if (pTargetList->GetSelection() == wxNOT_FOUND && 
		pTargetList->GetCount() > 0)
	{
		pTargetList->SetSelection(0);
	}
}

BEGIN_EVENT_TABLE(EditorPanel, wxPanel)
	EVT_LISTBOX(ID_Backup_LocationsList, 
		EditorPanel::OnSelectListItem)
	EVT_BUTTON(ID_Backup_LocationsAddButton,  
		EditorPanel::OnClickButtonAdd)
	EVT_BUTTON(ID_Backup_LocationsEditButton, 
		EditorPanel::OnClickButtonEdit)
	EVT_BUTTON(ID_Backup_LocationsDelButton,  
		EditorPanel::OnClickButtonRemove)
END_EVENT_TABLE()

class LocationsPanel : public EditorPanel
{
	private:
	wxTextCtrl*   mpNameText;
	wxTextCtrl*   mpPathText;

	public:
	LocationsPanel(wxWindow* pParent, ClientConfig *pConfig);
	virtual void PopulateList();
	virtual void PopulateControls();
	virtual void UpdateEnabledState();
	virtual void OnSelectListItem       (wxCommandEvent& rEvent);
	virtual void OnClickButtonAdd       (wxCommandEvent& rEvent);
	virtual void OnClickButtonEdit      (wxCommandEvent& rEvent);
	virtual void OnClickButtonRemove    (wxCommandEvent& rEvent);
	virtual void OnChangeLocationDetails(wxCommandEvent& rEvent);
	
	private:
	void SelectLocation(const BoxiLocation& rLocation);
	BoxiLocation* GetSelectedLocation();
	
	DECLARE_EVENT_TABLE()
};

LocationsPanel::LocationsPanel(wxWindow* pParent, ClientConfig *pConfig)
: EditorPanel(pParent, pConfig, ID_BackupLoc_List_Panel)
{
	mpListBoxSizer   ->GetStaticBox()->SetLabel(_("&Locations"));
	mpDetailsBoxSizer->GetStaticBox()->SetLabel(_("&Selected or New Location"));

	mpNameText = new wxTextCtrl(this, ID_Backup_LocationNameCtrl, wxT(""));
	AddParam(this, _("Location &Name:"), mpNameText, true, mpDetailsParamSizer);
		
	mpPathText = new wxTextCtrl(this, ID_Backup_LocationPathCtrl, wxT(""));
	AddParam(this, _("Location &Path:"), mpPathText, true, mpDetailsParamSizer);

	NotifyChange();
}

void LocationsPanel::PopulateList()
{
	PopulateLocationList(mpList);
	PopulateControls();
}

void LocationsPanel::UpdateEnabledState() 
{
	const BoxiLocation::List& rLocs = mpConfig->GetLocations();

	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry

	bool matchExistingEntry = FALSE;
	
	for (BoxiLocation::ConstIterator pLocation = rLocs.begin();
		pLocation != rLocs.end(); pLocation++)
	{
		if (mpNameText->GetValue().IsSameAs(pLocation->GetName()) &&
			mpPathText->GetValue().IsSameAs(pLocation->GetPath()))
		{
			matchExistingEntry = TRUE;
			break;
		}
	}
	
	if (matchExistingEntry) 
	{
		mpAddButton->Disable();
	} 
	else 
	{
		mpAddButton->Enable();
	}
	
	if (mpList->GetSelection() == wxNOT_FOUND)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Disable();
	}
	else if (matchExistingEntry)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Enable();
	}
	else
	{
		mpEditButton  ->Enable();	
		mpRemoveButton->Disable();
	}		
}

BEGIN_EVENT_TABLE(LocationsPanel, EditorPanel)
	EVT_TEXT(ID_Backup_LocationNameCtrl, 
		LocationsPanel::OnChangeLocationDetails)
	EVT_TEXT(ID_Backup_LocationPathCtrl, 
		LocationsPanel::OnChangeLocationDetails)
END_EVENT_TABLE()

void LocationsPanel::OnSelectListItem(wxCommandEvent &event)
{
	PopulateControls();
}

BoxiLocation* LocationsPanel::GetSelectedLocation()
{
	int selected = mpList->GetSelection();	
	if (selected == wxNOT_FOUND)
	{
		return NULL;
	}
	
	BoxiLocation* pLocation = (BoxiLocation*)( mpList->GetClientData(selected) );
	return pLocation;
}

void LocationsPanel::PopulateControls()
{
	BoxiLocation* pLocation = GetSelectedLocation();

	if (pLocation)
	{
		mpNameText->SetValue(pLocation->GetName());
		mpPathText->SetValue(pLocation->GetPath());
	}
	else
	{
		mpNameText->SetValue(wxT(""));
		mpPathText->SetValue(wxT(""));
	}
	
	UpdateEnabledState();
}

void LocationsPanel::OnChangeLocationDetails(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}

void LocationsPanel::OnClickButtonAdd(wxCommandEvent &event)
{
	BoxiLocation newLoc(
		mpNameText->GetValue(),
		mpPathText->GetValue(), mpConfig);

	BoxiLocation* pOldLocation = GetSelectedLocation();
	if (pOldLocation)
	{
		newLoc.SetExcludeList(pOldLocation->GetExcludeList());
	}
	
	mpConfig->AddLocation(newLoc);
	SelectLocation(newLoc);
}

void LocationsPanel::OnClickButtonEdit(wxCommandEvent &event)
{
	BoxiLocation* pLocation = GetSelectedLocation();
	if (!pLocation)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			_("Editing nonexistant location!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}
	
	pLocation->SetName(mpNameText->GetValue());
	pLocation->SetPath(mpPathText->GetValue());
}

void LocationsPanel::OnClickButtonRemove(wxCommandEvent &event)
{
	BoxiLocation* pLocation = GetSelectedLocation();
	if (!pLocation)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			_("Removing nonexistant location!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}
	mpConfig->RemoveLocation(*pLocation);
	PopulateControls();
}

void LocationsPanel::SelectLocation(const BoxiLocation& rLocation)
{
	for (size_t i = 0; i < mpList->GetCount(); i++)
	{
		BoxiLocation* pLocation = (BoxiLocation*)( mpList->GetClientData(i) );
		if (pLocation->IsSameAs(rLocation)) 
		{
			mpList->SetSelection(i);
			PopulateControls();
			break;
		}
	}
}

class ExclusionsPanel : public EditorPanel
{
	private:
	wxChoice*   mpLocationList;
	wxChoice*   mpTypeList;
	wxTextCtrl* mpValueText;

	public:
	ExclusionsPanel(wxWindow* pParent, ClientConfig *pConfig);
	virtual void PopulateList();
	virtual void PopulateControls();
	virtual void UpdateEnabledState();
	virtual void OnSelectLocationItem  (wxCommandEvent& rEvent);
	virtual void OnSelectListItem      (wxCommandEvent& rEvent);
	virtual void OnClickButtonAdd      (wxCommandEvent& rEvent);
	virtual void OnClickButtonEdit     (wxCommandEvent& rEvent);
	virtual void OnClickButtonRemove   (wxCommandEvent& rEvent);
	virtual void OnChangeExcludeDetails(wxCommandEvent& rEvent);
	
	private:
	void SelectExclusion(const BoxiExcludeEntry& rEntry);
	BoxiLocation* GetSelectedLocation();
	BoxiExcludeEntry* GetSelectedEntry();
	BoxiExcludeEntry  CreateNewEntry();
	const BoxiExcludeEntry::List* GetSelectedLocEntries();
	
	DECLARE_EVENT_TABLE()
};

ExclusionsPanel::ExclusionsPanel(wxWindow* pParent, ClientConfig *pConfig)
: EditorPanel(pParent, pConfig, ID_BackupLoc_Excludes_Panel)
{
	wxStaticBoxSizer* pLocationListBox = new wxStaticBoxSizer(wxVERTICAL, 
		this, _("&Locations"));
	mpTopSizer->Insert(0, pLocationListBox, 0, 
		wxGROW | wxTOP | wxLEFT | wxRIGHT, 8);
	
	mpLocationList = new wxChoice(this, ID_BackupLoc_ExcludeLocList);
	pLocationListBox->Add(mpLocationList, 0, wxGROW | wxALL, 8);
	
	mpListBoxSizer   ->GetStaticBox()->SetLabel(_("&Exclusions"));
	mpDetailsBoxSizer->GetStaticBox()->SetLabel(_("&Selected or New Exclusion"));

	wxString excludeTypes[numExcludeTypes];
	
	for (size_t i = 0; i < numExcludeTypes; i++) 
	{
		excludeTypes[i] = wxString(
			theExcludeTypes[i].ToString().c_str(), 
			wxConvBoxi);
	}

	mpTypeList = new wxChoice(this, ID_BackupLoc_ExcludeTypeList, 
		wxDefaultPosition, wxDefaultSize, 8, excludeTypes);

	for (size_t i = 0; i < numExcludeTypes; i++) 
	{
		mpTypeList->SetClientData(i, &(theExcludeTypes[i]));
	}
	
	AddParam(this, _("Exclude Type:"), mpTypeList, true, 
		mpDetailsParamSizer);
	mpTypeList->SetSelection(0);
		
	mpValueText = new wxTextCtrl(this, ID_BackupLoc_ExcludePathCtrl, wxT(""));
	AddParam(this, _("Exclude Path:"), mpValueText, true, 
		mpDetailsParamSizer);

	NotifyChange();
}

BoxiLocation* ExclusionsPanel::GetSelectedLocation()
{
	int selectedLocIndex = mpLocationList->GetSelection();
	if (selectedLocIndex == wxNOT_FOUND) return NULL;
	
	BoxiLocation* mpLocation = (BoxiLocation*)( 
		mpLocationList->GetClientData(selectedLocIndex) );
	return mpLocation;
}
	
const BoxiExcludeEntry::List* ExclusionsPanel::GetSelectedLocEntries()
{	
	BoxiLocation* mpLocation = GetSelectedLocation();
	if (!mpLocation) return NULL;
		
	BoxiExcludeList& rExcludeList = mpLocation->GetExcludeList();
	return &rExcludeList.GetEntries();
}

BoxiExcludeEntry* ExclusionsPanel::GetSelectedEntry()
{
	int selected = mpList->GetSelection();
	if (selected == wxNOT_FOUND)
		return NULL;
	
	BoxiExcludeEntry* pEntry = (BoxiExcludeEntry*)( mpList->GetClientData(selected) );
	return pEntry;
}

void ExclusionsPanel::PopulateList()
{
	PopulateLocationList(mpLocationList);
	
	BoxiExcludeEntry* pSelectedEntry = GetSelectedEntry();	
	mpList->Clear();
	
	BoxiLocation* pLocation = GetSelectedLocation();
	if (!pLocation) return;
	
	BoxiExcludeList& rExcludeList = pLocation->GetExcludeList();
	const BoxiExcludeEntry::List& rEntries = rExcludeList.GetEntries();
	
	for (BoxiExcludeEntry::ConstIterator pEntry = rEntries.begin();
		pEntry != rEntries.end(); pEntry++)
	{
		wxString exString(pEntry->ToString().c_str(), wxConvBoxi);
		int newIndex = mpList->Append(exString);
		mpList->SetClientData(newIndex, rExcludeList.UnConstEntry(*pEntry));
		
		if (&(*pEntry) == pSelectedEntry)
		{
			mpList->SetSelection(newIndex);
		}
	}
	
	if (mpList->GetSelection() == wxNOT_FOUND &&
		mpList->GetCount() > 0)
	{
		mpList->SetSelection(0);
	}

	PopulateControls();
}

void ExclusionsPanel::UpdateEnabledState() 
{
	if (mpLocationList->GetSelection() == wxNOT_FOUND)
	{
		mpList->Disable();
		mpTypeList->Disable();
		mpValueText->Disable();
		mpAddButton->Disable();
		mpEditButton->Disable();
		mpRemoveButton->Disable();
		return;
	}
	
	mpList->Enable();
	mpTypeList->Enable();
	mpValueText->Enable();
	
	// Enable the Add and Edit buttons if the current values 
	// don't match any existing entry, and the Remove button 
	// if they do match an existing entry

	bool matchExistingEntry = FALSE;
	
	const BoxiExcludeEntry::List* pEntries = GetSelectedLocEntries();
	if (pEntries) 
	{
		for (BoxiExcludeEntry::ConstIterator pEntry = pEntries->begin();
			pEntry != pEntries->end(); pEntry++)
		{
			int selection = mpTypeList->GetSelection();
			if (selection != wxNOT_FOUND &&
				mpTypeList->GetClientData(selection) == pEntry->GetType() &&
				mpValueText->GetValue().IsSameAs(pEntry->GetValueString()))
			{
				matchExistingEntry = TRUE;
				break;
			}
		}
	}
	
	if (matchExistingEntry) 
	{
		mpAddButton->Disable();
	} 
	else 
	{
		mpAddButton->Enable();
	}
	
	if (mpList->GetSelection() == wxNOT_FOUND)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Disable();
	}
	else if (matchExistingEntry)
	{
		mpEditButton  ->Disable();	
		mpRemoveButton->Enable();
	}
	else
	{
		mpEditButton  ->Enable();	
		mpRemoveButton->Disable();
	}		
}

BEGIN_EVENT_TABLE(ExclusionsPanel, EditorPanel)
	EVT_CHOICE(ID_BackupLoc_ExcludeLocList,
		ExclusionsPanel::OnSelectLocationItem)
	EVT_CHOICE(ID_BackupLoc_ExcludeTypeList, 
		ExclusionsPanel::OnChangeExcludeDetails)
	EVT_TEXT(ID_BackupLoc_ExcludePathCtrl, 
		ExclusionsPanel::OnChangeExcludeDetails)
END_EVENT_TABLE()

void ExclusionsPanel::OnSelectLocationItem(wxCommandEvent &event)
{
	PopulateList();
	UpdateEnabledState();
}

void ExclusionsPanel::OnSelectListItem(wxCommandEvent &event)
{
	PopulateControls();
}

void ExclusionsPanel::PopulateControls()
{
	int selected = mpList->GetSelection();
	
	mpTypeList->SetSelection(0);
	mpValueText->SetValue(wxT(""));

	if (selected != wxNOT_FOUND)
	{
		BoxiExcludeEntry* pEntry =
			(BoxiExcludeEntry*)( mpList->GetClientData(selected) );
		
		for (size_t i = 0; i < mpTypeList->GetCount(); i++)
		{
			if (mpTypeList->GetClientData(i) == pEntry->GetType())
			{
				mpTypeList->SetSelection(i);
				break;
			}
		}
		
		mpValueText->SetValue(pEntry->GetValueString());
	}
	
	UpdateEnabledState();
}

void ExclusionsPanel::OnChangeExcludeDetails(wxCommandEvent& rEvent)
{
	UpdateEnabledState();
}

BoxiExcludeEntry ExclusionsPanel::CreateNewEntry()
{
	BoxiExcludeType* pType = NULL;
	
	long selectedType = mpTypeList->GetSelection();
	if (selectedType != wxNOT_FOUND)
	{
		pType = (BoxiExcludeType*)( mpTypeList->GetClientData(selectedType) );
	}

	return BoxiExcludeEntry(*pType, mpValueText->GetValue());
}

void ExclusionsPanel::OnClickButtonAdd(wxCommandEvent &event)
{
	if (mpTypeList->GetSelection() == wxNOT_FOUND)
	{
		wxGetApp().ShowMessageBox(BM_EXCLUDE_NO_TYPE_SELECTED,
			_("No type selected!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	BoxiExcludeEntry newEntry = CreateNewEntry();
	
	BoxiLocation* pLocation = GetSelectedLocation();
	BoxiExcludeList& rList = pLocation->GetExcludeList();	
	rList.AddEntry(newEntry);
	// calls listeners, updates list and tree automatically
	
	SelectExclusion(newEntry);
}

void ExclusionsPanel::OnClickButtonEdit(wxCommandEvent &event)
{
	long selected = mpLocationList->GetSelection();
	if (selected == wxNOT_FOUND)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			_("Editing nonexistant location!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	BoxiExcludeEntry* pOldEntry = GetSelectedEntry();
	if (!pOldEntry)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_EXCLUDE_ENTRY_NOT_SELECTED,
			_("No exclude entry selected!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	if (mpTypeList->GetSelection() == wxNOT_FOUND)
	{
		wxGetApp().ShowMessageBox(BM_EXCLUDE_NO_TYPE_SELECTED,
			_("No type selected!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	BoxiExcludeEntry newEntry = CreateNewEntry();
	
	BoxiLocation* pLocation = GetSelectedLocation();
	BoxiExcludeList& rList = pLocation->GetExcludeList();
	
	rList.ReplaceEntry(*pOldEntry, newEntry);
	SelectExclusion(newEntry);
}

void ExclusionsPanel::OnClickButtonRemove(wxCommandEvent &event)
{
	BoxiExcludeEntry* pOldEntry = GetSelectedEntry();
	if (!pOldEntry)
	{
		wxGetApp().ShowMessageBox(BM_INTERNAL_LOCATION_DOES_NOT_EXIST,
			_("Removing nonexistant location!"), _("Boxi Error"), 
			wxICON_ERROR | wxOK, this);
		return;
	}

	BoxiLocation* pLocation = GetSelectedLocation();
	BoxiExcludeList& rList = pLocation->GetExcludeList();
	rList.RemoveEntry(*pOldEntry);
	PopulateControls();
}

void ExclusionsPanel::SelectExclusion(const BoxiExcludeEntry& rEntry)
{
	for (size_t i = 0; i < mpList->GetCount(); i++)
	{
		BoxiExcludeEntry* pEntry = (BoxiExcludeEntry*)( mpList->GetClientData(i) );
		if (pEntry->IsSameAs(rEntry)) 
		{
			mpList->SetSelection(i);
			PopulateControls();
			break;
		}
	}
}

BEGIN_EVENT_TABLE(BackupLocationsPanel, wxPanel)
	EVT_TREE_ITEM_ACTIVATED(ID_Backup_Locations_Tree, 
		BackupLocationsPanel::OnTreeNodeActivate)
	EVT_BUTTON(wxID_CANCEL, BackupLocationsPanel::OnClickCloseButton)
END_EVENT_TABLE()

BackupLocationsPanel::BackupLocationsPanel
(
	ClientConfig* pConfig,
	wxWindow* pParent, 
	MainFrame* pMainFrame,
	wxPanel* pPanelToShowOnClose,
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name
)
:	wxPanel(pParent, id, pos, size, style, name),
	mpMainFrame(pMainFrame),
	mpPanelToShowOnClose(pPanelToShowOnClose)
{
	mpConfig = pConfig;
	mpConfig->AddListener(this);

	wxBoxSizer* pTopSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(pTopSizer);
	
	wxNotebook* pNotebook = new wxNotebook(this, wxID_ANY);
	pTopSizer->Add(pNotebook, 1, wxGROW | wxALL, 8);
	
	wxPanel* pBasicPanel = new wxPanel(pNotebook);
	pNotebook->AddPage(pBasicPanel, _("Basic"));
	
	wxBoxSizer* pBasicPanelSizer = new wxBoxSizer(wxVERTICAL);
	pBasicPanel->SetSizer(pBasicPanelSizer);

#ifdef WIN32
	mpRootNode = new BackupTreeNode(pConfig, wxEmptyString);
	wxString rootLabel = _("My Computer");
#else
	mpRootNode = new BackupTreeNode(pConfig, _("/"));
	wxString rootLabel = _("/ (local root)");
#endif
	
	mpTree = new LocalFileTree(pBasicPanel, ID_Backup_Locations_Tree, 
		mpRootNode, rootLabel);
	pBasicPanelSizer->Add(mpTree, 1, wxGROW | wxALL, 8);
	
	LocationsPanel* pLocationsPanel = new LocationsPanel(pNotebook, mpConfig);
	pNotebook->AddPage(pLocationsPanel, _("Locations"));

	ExclusionsPanel* pExclusionsPanel = new ExclusionsPanel(pNotebook, mpConfig);
	pNotebook->AddPage(pExclusionsPanel, _("Exclusions"));

	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pTopSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, wxID_CANCEL, _("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);
}

void BackupLocationsPanel::NotifyChange()
{
	UpdateTreeOnConfigChange();
}

void BackupLocationsPanel::UpdateTreeOnConfigChange()
{
	mpTree->UpdateStateIcon(mpRootNode, FALSE, TRUE);
}

void BackupLocationsPanel::OnTreeNodeActivate(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	BackupTreeNode* pTreeNode = (BackupTreeNode *)(mpTree->GetItemData(item));

	#ifdef WIN32
	if (pTreeNode->IsRoot()) return;
	#endif

	BoxiLocation*           pLocation   = pTreeNode->GetLocation();
	const BoxiExcludeEntry* pExcludedBy = pTreeNode->GetExcludedBy();
	const BoxiExcludeEntry* pIncludedBy = pTreeNode->GetIncludedBy();
	BoxiExcludeList*        pList       = NULL;

	if (pLocation) 
	{
		pList = &(pLocation->GetExcludeList());
	}

	BackupTreeNode* pUpdateFrom = NULL;

	// we will need the location root in several places,
	// any time an exclude entry is added or removed,
	// because all mpExcludedBy/IncludedBy pointers 
	// will be invalidated and must be recalculated.
	BackupTreeNode* pLocationNode = NULL;
	for (pLocationNode = pTreeNode; pLocationNode != NULL;)
	{
		BackupTreeNode* pParentNode = 
			(BackupTreeNode*)pLocationNode->GetParentNode();
		if (pParentNode == NULL ||
			pParentNode->GetLocation() != pLocation)
		{
			break;
		}
		pLocationNode = pParentNode;
	}

	wxASSERT(!(pLocation   && !pLocationNode));
	wxASSERT(!(pLocation   && !pList));
	wxASSERT(!(pIncludedBy && !pList));
	wxASSERT(!(pExcludedBy && !pList));
	
	// avoid updating whole tree
	mpConfig->RemoveListener(this);
	
	if (pIncludedBy != NULL)
	{
		// Tricky case. User wants to exclude an AlwaysIncluded item.
		// We can't override it with anything, and unless this item is 
		// the only match of the AlwaysInclude directive, we can't 
		// remove it without affecting other items. The best we can do is 
		// to warn the user, ask them whether to remove it.

		bool doDelete = false;
		bool warnUser = true;
		
		if (pIncludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(wxString(
				pIncludedBy->GetValue().c_str(), wxConvBoxi));
			if (fn.SameAs(pTreeNode->GetFullPath()))
			{
				doDelete = true;
				warnUser = false;
					
				// There may also be a couple of rules
				// which exclude all files and dirs
				// under this directory, added by the
				// AlwaysInclude tree code below.
				// If so, delete them both.

				wxString regex(_("^"));
				regex.Append(pTreeNode->GetFullPath());
				regex.Append(_("/"));
				
				BoxiExcludeEntry excludeFiles(
					ET_EXCLUDE_FILES_REGEX, regex);
				BoxiExcludeEntry excludeDirs(
					ET_EXCLUDE_DIRS_REGEX, regex);
				
				if (pList->Contains(excludeFiles) &&
					pList->Contains(excludeDirs))
				{
					pList->RemoveEntry(excludeFiles);
					pList->RemoveEntry(excludeDirs);
				}
			}
		}
		
		if (warnUser) 
		{
			wxString PathToExclude(pIncludedBy->GetValue().c_str(), 
				wxConvBoxi);
			
			wxString msg;
			msg.Printf(_(
				"To exclude this item, you will have to "
				"exclude all of %s. Do you really want to "
				"do this?"),
				PathToExclude.c_str());
			
			int result = wxGetApp().ShowMessageBox
			(
				BM_EXCLUDE_MUST_REMOVE_ALWAYSINCLUDE,
				msg, _("Boxi Warning"), 
				wxYES_NO | wxICON_EXCLAMATION, NULL
			);

			if (result == wxYES)
			{
				doDelete = true;
			}
		}
		
		if (doDelete && pList) 
		{
			pList->RemoveEntry(*pIncludedBy);
			pUpdateFrom = pLocationNode;
		}			
	} 
	else if (pExcludedBy != NULL)
	{
		// Node is excluded, and user wants to include it.
		// If the selected node is the value of an exact matching
		// Exclude(File|Dir) entry, we can delete that entry.
		// Otherwise, we may need to change the exclusion, and/or
		// add some AlwaysInclude entries for it.
		
		bool handled = false;
		
		if (pExcludedBy->GetMatch() == EM_EXACT)
		{
			wxFileName fn(wxString(
				pExcludedBy->GetValue().c_str(), wxConvBoxi));
			if (fn.SameAs(pTreeNode->GetFullPath()))
			{
				if (pList)
				{
					pList->RemoveEntry(*pExcludedBy);
				}
				handled = true;
			}
		}
		
		if (!handled && pList)
		{
			// walk up the tree, adding AlwaysIncludeDir nodes,
			// until we reach one which is not excluded, or which
			// is excluded by an exact match. In the latter case,
			// delete the exact match. In either case, stop there.
			
			int pos = pList->GetEntries().size();
			BackupTreeNode* pExclude;
			
			for (BackupTreeNode* pPos = pTreeNode;
				pPos->GetExcludedBy()
				&& !(pPos->GetIncludedBy())
				&& !(pPos->IsRoot())
				&& pList;
				pPos = (BackupTreeNode*)pPos->GetParentNode())
			{
				pExclude = pPos;
				
				BoxiExcludeEntry newEntry(
					pPos->IsDirectory() 
					? ET_ALWAYS_INCLUDE_DIR
					: ET_ALWAYS_INCLUDE_FILE,
					pPos->GetFullPath());
				
				pList->InsertEntry(pos, newEntry);
			}

			// pExclude is now the last (highest) node visited,
			// which may be the same as pTreeNode (e.g. if an
			// exclude regexp applies to pTreeNode)
			
			if (pTreeNode == pExclude)
			{
				// simple case, no need to exclude 
				// and alwaysinclude
			}
			else if (pExclude->IsDirectory())
			{
				wxString path(_("^"));
				path.Append(pExclude->GetFullPath());
				path.Append(_("/"));
				pList->InsertEntry(pos, BoxiExcludeEntry(
					ET_EXCLUDE_FILES_REGEX, path));
				pList->InsertEntry(pos, BoxiExcludeEntry(
					ET_EXCLUDE_DIRS_REGEX, path));
			}
			
			if (pTreeNode->IsDirectory())
			{
				wxString path(_("^"));
				path.Append(pTreeNode->GetFullPath());
				path.Append(_("/"));
				pList->AddEntry(BoxiExcludeEntry(
					ET_ALWAYS_INCLUDE_DIRS_REGEX, path));
				pList->AddEntry(BoxiExcludeEntry(
					ET_ALWAYS_INCLUDE_FILES_REGEX, path));
			}
			else
			{
				pList->AddEntry(BoxiExcludeEntry(
					ET_ALWAYS_INCLUDE_FILE, 
					pTreeNode->GetFullPath()));
			}
		}

		pUpdateFrom = pLocationNode;
	}
	else if (pLocation != NULL)
	{
		// User wants to exclude a location, or a file not already 
		// excluded within that location. Which is it? For a location
		// root, we prompt the user and delete the whole location.
		// For a subdirectory or file, we add an Exclude directive.
		bool handled = false;

		wxFileName fn(pLocation->GetPath());
		if (fn.SameAs(pTreeNode->GetFullPath()))
		{
			wxString msg;
			msg.Printf(_(
				"Are you sure you want to delete "
				"the location %s from the server, "
				"and remove its configuration?"),
				fn.GetFullPath().c_str());

			int result = wxGetApp().ShowMessageBox(
				BM_BACKUP_FILES_DELETE_LOCATION_QUESTION, msg, 
				_("Boxi Warning"), 
				wxYES_NO | wxICON_EXCLAMATION, this);

			if (result == wxYES)
			{
				mpConfig->RemoveLocation(*pLocation);
				pUpdateFrom = mpRootNode;
			}
			handled = true;
		}
		
		if (!handled && pList) 
		{
			BoxiExcludeEntry newEntry(
				theExcludeTypes[
					pTreeNode->IsDirectory() 
					? ETI_EXCLUDE_DIR
					: ETI_EXCLUDE_FILE],
				pTreeNode->GetFullPath());

			pList->AddEntry(newEntry);
			pUpdateFrom = pLocationNode;
		}
	}
	else
	{
		// outside of any existing location. create a new one.
		wxFileName path(pTreeNode->GetFileName());
		
		wxString newLocName = path.GetName();
		if (newLocName.IsSameAs(wxEmptyString))
		{
			newLocName = _("root");
		}
		
		const BoxiLocation::List& rLocs = mpConfig->GetLocations();
		bool foundUnique = false;
		int counter = 1;
		
		while (!foundUnique)
		{
			foundUnique = true;
			
			for (BoxiLocation::ConstIterator i = rLocs.begin();
				i != rLocs.end(); i++)
			{
				if (newLocName.IsSameAs(i->GetName().c_str()))
				{
					foundUnique = false;
					break;
				}
			}
			
			if (foundUnique) break;
				
			// generate a new filename, and try again
			newLocName.Printf(_("%s_%d"), 
				path.GetName().c_str(), counter++);
		}
		
		BoxiLocation newLoc(newLocName, pTreeNode->GetFullPath(), mpConfig);
		mpConfig->AddLocation(newLoc);
		pUpdateFrom = mpRootNode;
	}
	
	mpConfig->AddListener(this);
	
	if (pUpdateFrom)
	{
		mpTree->UpdateStateIcon(pUpdateFrom, false, true);
	}
	
	// doesn't work? - FIXME
	// event.Veto();
	event.Skip();
}

void BackupLocationsPanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
	mpMainFrame->ShowPanel(mpPanelToShowOnClose);
}
