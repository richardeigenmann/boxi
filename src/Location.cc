/***************************************************************************
 *            Location.cc
 *
 *  Mon Feb 28 23:38:45 2005
 *  Copyright 2005-2008 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "SandBox.h"

#include <set>
#include <string>

#include <wx/filename.h>
#include <wx/log.h>

#include "Location.h"
#include "Utils.h"

BoxiExcludeType theExcludeTypes [] = {
	BoxiExcludeType(ES_EXCLUDE,       EFD_DIR,  EM_EXACT),
	BoxiExcludeType(ES_EXCLUDE,       EFD_DIR,  EM_REGEX),
	BoxiExcludeType(ES_EXCLUDE,       EFD_FILE, EM_EXACT),
	BoxiExcludeType(ES_EXCLUDE,       EFD_FILE, EM_REGEX),
	BoxiExcludeType(ES_ALWAYSINCLUDE, EFD_DIR,  EM_EXACT),
	BoxiExcludeType(ES_ALWAYSINCLUDE, EFD_DIR,  EM_REGEX),
	BoxiExcludeType(ES_ALWAYSINCLUDE, EFD_FILE, EM_EXACT),
	BoxiExcludeType(ES_ALWAYSINCLUDE, EFD_FILE, EM_REGEX),
};

BoxiExcludeList::BoxiExcludeList(const Configuration& conf, 
	LocationChangeListener* pListener) 
: mpListener(pListener)
{
	for (size_t i = 0; i < sizeof(theExcludeTypes) / sizeof(BoxiExcludeType); i++)
	{
		BoxiExcludeType& t = theExcludeTypes[i];
		_AddConfigList(conf, t.ToString(), t);
	}
}	

inline void BoxiExcludeList::_AddConfigList(const Configuration& conf, 
	const std::string& keyName, BoxiExcludeType& type)
{
	if (!conf.KeyExists(keyName.c_str())) return;
	std::string value = conf.GetKeyValue(keyName.c_str());
	_AddSeparatedList(value, type);
}

inline void BoxiExcludeList::_AddSeparatedList(const std::string& entries, 
	BoxiExcludeType& type)
{
	std::vector<std::string> temp;
	SplitString(entries, Configuration::MultiValueSeparator, temp);
	for (std::vector<std::string>::const_iterator i = 
		temp.begin(); i != temp.end(); i++)
	{
		std::string temp = *i;
		mEntries.push_back(BoxiExcludeEntry(type, temp));
	}
}

void BoxiExcludeList::AddEntry(const BoxiExcludeEntry& rNewEntry) 
{
	for (BoxiExcludeEntry::ConstIterator 
		i  = mEntries.begin();
		i != mEntries.end(); i++)
	{
		if (i->IsSameAs(rNewEntry))
		{
			return;
		}
	}

	mEntries.push_back(rNewEntry);
	
	if (mpListener)
	{
		mpListener->OnExcludeListChange(this);
	}
}

void BoxiExcludeList::InsertEntry(int index, const BoxiExcludeEntry& rNewEntry) 
{
	for (BoxiExcludeEntry::ConstIterator 
		i  = mEntries.begin();
		i != mEntries.end(); i++)
	{
		if (i->IsSameAs(rNewEntry))
		{
			return;
		}
	}

	BoxiExcludeEntry::Iterator i;
		
	for (i = mEntries.begin();
	     i != mEntries.end() && index > 0;
	     i++, index--)
	{ }

	mEntries.insert(i, rNewEntry);
	
	if (mpListener)
	{
		mpListener->OnExcludeListChange(this);
	}
}

void BoxiExcludeList::ReplaceEntry(const BoxiExcludeEntry& rOldEntry, 
	const BoxiExcludeEntry& rNewEntry) 
{
	std::list<BoxiExcludeEntry>::iterator current;
	for (current = mEntries.begin(); 
		current != mEntries.end() && !current->IsSameAs(rOldEntry); current++) 
		{ }
	if (current == mEntries.end()) throw "item not found";
	*current = rNewEntry;
	if (mpListener)
	{
		mpListener->OnExcludeListChange(this);
	}
}

/*
void BoxiExcludeList::RemoveEntry(int target) 
{
	std::list<BoxiExcludeEntry>::iterator current;
	for (current = mEntries.begin();
		current != mEntries.end() && target != 0;
		current++, target--) { }
	if (current == mEntries.end())
	{
		throw "index out of bounds";
	}
	mEntries.erase(current);
	if (mpListener)
	{
		mpListener->OnExcludeListChange(this);
	}
}
*/

void BoxiExcludeList::RemoveEntry(const BoxiExcludeEntry& rOldEntry) 
{
	std::list<BoxiExcludeEntry>::iterator current;
	for (current = mEntries.begin(); 
		current != mEntries.end() && !current->IsSameAs(rOldEntry); current++) 
		{ }
	if (current == mEntries.end())
	{
		throw "index out of bounds";
	}
	mEntries.erase(current);
	if (mpListener)
	{
		mpListener->OnExcludeListChange(this);
	}
}

BoxiExcludeEntry* BoxiExcludeList::UnConstEntry(const BoxiExcludeEntry& rEntry)
{
	std::list<BoxiExcludeEntry>::iterator current;
	for (current = mEntries.begin(); 
		current != mEntries.end() && !current->IsSameAs(rEntry); current++) 
		{ }
	if (current == mEntries.end())
	{
		return NULL;
	}
	return &(*current);
}

/*
bool Location::IsExcluded(const wxString& rLocalFileName, bool mIsDirectory, 
	const BoxiExcludeEntry** ppExcludedBy, const BoxiExcludeEntry** ppIncludedBy)
{
	ExcludedState state = GetExcludedState(rLocalFileName, mIsDirectory,
		ppExcludedBy, ppIncludedBy);
	assert(state == EST_KNOWN);
	return (
	return (state == EST_UNKNOWN || state == EST_NOLOC 
		||  state == EST_EXCLUDED);
}
*/

ExcludedState BoxiLocation::GetExcludedState(const wxString& rLocalFileName, 
	bool mIsDirectory, const BoxiExcludeEntry** ppExcludedBy, 
	const BoxiExcludeEntry** ppIncludedBy, bool* pMatched)
{
	//wxLogDebug(_(" checking whether %s is excluded..."), 
	//	rLocalFileName.c_str());
	
	const std::list<BoxiExcludeEntry>& rExcludeList =
		mExcluded.GetEntries();

	wxFileName fn(rLocalFileName);
	wxFileName locroot(GetPath());
	wxFileName root(fn.GetPath());
	root.SetPath(fn.GetPathSeparator());

	if (fn == locroot)
	{
		// The location root cannot be Excluded or AlwaysIncluded.
		
		if (pMatched)
		{
			*pMatched = true;
		}
		
		return EST_INCLUDED;
	}
	
	if (fn == root)
	{
		// If the root directory is not a Location,
		// then at least it's the base case. Don't try
		// to go further up!
		return EST_NOLOC;
	}
	
	ExcludedState isExcluded = EST_INCLUDED;
	
	// on pass 1, remove Excluded files
	// on pass 2, re-add AlwaysInclude files
	
	for (int pass = 1; pass <= 2; pass++) 
	{
		// wxLogDebug(_(" pass %d"), pass);

		for (BoxiExcludeEntry::ConstIterator 
			pEntry  = rExcludeList.begin();
			pEntry != rExcludeList.end(); pEntry++)
		{
			ExcludeMatch match = pEntry->GetMatch();
			std::string  value = pEntry->GetValue();
			wxString value2(value.c_str(), wxConvBoxi);
			bool matched = false;

			{
				std::string name = pEntry->ToString();
				wxString name2(name.c_str(), wxConvBoxi);
				//wxLogDebug(_("  checking against %s"),
				//	name2.c_str());
			}

			ExcludeSense sense = pEntry->GetSense();
			
			if (pass == 1 && sense != ES_EXCLUDE) 
			{
				//wxLogDebug(_("   not an Exclude entry"));
				continue;
			}
			
			if (pass == 2 && sense != ES_ALWAYSINCLUDE) 
			{
				//wxLogDebug(_("   not an AlwaysInclude entry"));
				continue;
			}
			
			ExcludeFileDir fileOrDir = pEntry->GetFileDir();
			
			if (fileOrDir == EFD_FILE && mIsDirectory) 
			{
				//wxLogDebug(_("   doesn't match directories"));
				continue;
			}
			
			if (fileOrDir == EFD_DIR && !mIsDirectory) 
			{
				//wxLogDebug(_("   doesn't match files"));
				continue;
			}
			
			if (match == EM_EXACT) 
			{
				if (rLocalFileName == value2) 
				{
					//wxLogDebug(_("    exact match"));
					matched = true;
				}
			} 
			else if (match == EM_REGEX) 
			{
				std::auto_ptr<regex_t> apr = 
					std::auto_ptr<regex_t>(new regex_t);
				if (::regcomp(apr.get(), value.c_str(),
					REG_EXTENDED | REG_NOSUB) != 0) 
				{
					wxLogError(_("Regular expression compile failed (%s)"),
						value2.c_str());
				}
				else
				{
					wxCharBuffer buf = rLocalFileName.mb_str(wxConvBoxi);
					int result = regexec(apr.get(), buf.data(), 0, 0, 0);
					matched = (result == 0);
				}
			}

			if (!matched) 
			{
				//wxLogDebug(_("   no match."));
				continue;
			}

			if (pMatched)
			{
				*pMatched = true;
			}

			//wxLogDebug(_("   matched!"));
			
			if (sense == ES_EXCLUDE)
			{
				isExcluded = EST_EXCLUDED;
				if (ppExcludedBy)
					*ppExcludedBy = &(*pEntry);
			} 
			else if (sense == ES_ALWAYSINCLUDE)
			{
				isExcluded = EST_ALWAYSINCLUDED;
				if (ppIncludedBy)
					*ppIncludedBy = &(*pEntry);
			}
			
			// Now break out of this loop, since it doesn't matter
			// if any other entry has the same effect as this one.
			break;
		}
	}
	
	if (isExcluded != EST_ALWAYSINCLUDED && ppIncludedBy)
	{
		*ppIncludedBy = NULL;
	}
	
	if (isExcluded == EST_INCLUDED && ppExcludedBy)
	{
		*ppExcludedBy = NULL;
	}
	
	return isExcluded;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    GetBoxExcludeList()
//		Purpose: Return a Box Backup-compatible ExcludeList.
//		Created: 28/1/04
//
// --------------------------------------------------------------------------
ExcludeList* BoxiLocation::GetBoxExcludeList(bool listDirs) const
{
	// Create the exclude list
	ExcludeList *pExclude = new ExcludeList;
	ExcludeList *pInclude = new ExcludeList;
	pExclude->SetAlwaysIncludeList(pInclude);
	
	const BoxiExcludeEntry::List& rEntries(mExcluded.GetEntries());
	
	try
	{
		for (BoxiExcludeEntry::ConstIterator 
			pEntry  = rEntries.begin();
			pEntry != rEntries.end(); pEntry++)
		{
			if (listDirs && (pEntry->GetFileDir() == EFD_FILE))
				continue;
			
			if (!listDirs && (pEntry->GetFileDir() == EFD_DIR))
				continue;
			
			ExcludeList* pList = NULL;
			if (pEntry->GetSense() == ES_EXCLUDE)
			{
				pList = pExclude;
			}
			else if (pEntry->GetSense() == ES_ALWAYSINCLUDE)
			{
				pList = pInclude;
			}
			
			if (pEntry->GetMatch() == EM_EXACT)
			{
				pList->AddDefiniteEntries(pEntry->GetValue());
			}
			else if (pEntry->GetMatch() == EM_REGEX)
			{
				pList->AddRegexEntries(pEntry->GetValue());
			}
		}
	}
	catch(...)
	{
		// Clean up
		delete pExclude;
		delete pInclude;
		throw;
	}

	return pExclude;
}
