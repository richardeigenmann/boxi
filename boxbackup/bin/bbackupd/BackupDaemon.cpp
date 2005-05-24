// distribution boxbackup-0.09
// 
//  
// Copyright (c) 2003, 2004
//      Ben Summers.  All rights reserved.
//  
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All use of this software and associated advertising materials must 
//    display the following acknowledgement:
//        This product includes software developed by Ben Summers.
// 4. The names of the Authors may not be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
// 
// [Where legally impermissible the Authors do not disclaim liability for 
// direct physical injury or death caused solely by defects in the software 
// unless it is modified by a third party.]
// 
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//  
//  
//  
// --------------------------------------------------------------------------
//
// File
//		Name:    BackupDaemon.cpp
//		Purpose: Backup daemon
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------

#include "Box.h"

#include <unistd.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <signal.h>
#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
	#include <mntent.h>
#endif
#include <sys/wait.h>

#include "Configuration.h"
#include "IOStream.h"
#include "MemBlockStream.h"
#include "CommonException.h"

#include "SSLLib.h"
#include "TLSContext.h"

#include "BackupDaemon.h"
#include "BackupDaemonConfigVerify.h"
#include "BackupClientContext.h"
#include "BackupClientDirectoryRecord.h"
#include "BackupStoreDirectory.h"
#include "BackupClientFileAttributes.h"
#include "BackupStoreFilenameClear.h"
#include "BackupClientInodeToIDMap.h"
#include "autogen_BackupProtocolClient.h"
#include "BackupClientCryptoKeys.h"
#include "BannerText.h"
#include "BackupStoreFile.h"
#include "Random.h"
#include "ExcludeList.h"
#include "BackupClientMakeExcludeList.h"
#include "IOStreamGetLine.h"
#include "Utils.h"
#include "FileStream.h"
#include "BackupStoreException.h"
#include "BackupStoreConstants.h"
#include "LocalProcessStream.h"
#include "IOStreamGetLine.h"
#include "Conversion.h"

#include "MemLeakFindOn.h"

#define 	MAX_SLEEP_TIME	((unsigned int)1024)

// Make the actual sync period have a little bit of extra time, up to a 64th of the main sync period.
// This prevents repetative cycles of load on the server
#define		SYNC_PERIOD_RANDOM_EXTRA_TIME_SHIFT_BY	6

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::BackupDaemon()
//		Purpose: constructor
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
BackupDaemon::BackupDaemon()
	: mState(State_Initialising),
	  mpCommandSocketInfo(0),
	  mDeleteUnusedRootDirEntriesAfter(0)
{
	// Only ever one instance of a daemon
	SSLLib::Initialise();
	
	// Initialise notifcation sent status
	for(int l = 0; l <= NotifyEvent__MAX; ++l)
	{
		mNotificationsSent[l] = false;
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::~BackupDaemon()
//		Purpose: Destructor
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
BackupDaemon::~BackupDaemon()
{
	DeleteAllLocations();
	DeleteAllIDMaps();
	
	if(mpCommandSocketInfo != 0)
	{
		delete mpCommandSocketInfo;
		mpCommandSocketInfo = 0;
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DaemonName()
//		Purpose: Get name of daemon
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
const char *BackupDaemon::DaemonName() const
{
	return "bbackupd";
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DaemonBanner()
//		Purpose: Daemon banner
//		Created: 1/1/04
//
// --------------------------------------------------------------------------
const char *BackupDaemon::DaemonBanner() const
{
#ifndef NDEBUG
	// Don't display banner in debug builds
	return 0;
#else
	return BANNER_TEXT("Backup Client");
#endif
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::GetConfigVerify()
//		Purpose: Get configuration specification
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
const ConfigurationVerify *BackupDaemon::GetConfigVerify() const
{
	// Defined elsewhere
	return &BackupDaemonConfigVerify;
}

#ifdef PLATFORM_CANNOT_FIND_PEER_UID_OF_UNIX_SOCKET
// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::SetupInInitialProcess()
//		Purpose: Platforms with non-checkable credientals on local sockets only.
//				 Prints a warning if the command socket is used.
//		Created: 25/2/04
//
// --------------------------------------------------------------------------
void BackupDaemon::SetupInInitialProcess()
{
	// Print a warning on this platform if the CommandSocket is used.
	if(GetConfiguration().KeyExists("CommandSocket"))
	{
		printf(
				"============================================================================================\n" \
				"SECURITY WARNING: This platform cannot check the credentials of connections to the\n" \
				"command socket. This is a potential DoS security problem.\n" \
				"Remove the CommandSocket directive from the bbackupd.conf file if bbackupctl is not used.\n" \
				"============================================================================================\n"
			);
	}
}
#endif


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteAllLocations()
//		Purpose: Deletes all records stored
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void BackupDaemon::DeleteAllLocations()
{
	// Run through, and delete everything
	for(std::vector<Location *>::iterator i = mLocations.begin();
		i != mLocations.end(); ++i)
	{
		delete *i;
	}

	// Clear the contents of the map, so it is empty
	mLocations.clear();
	
	// And delete everything from the assoicated mount vector
	mIDMapMounts.clear();
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::Run()
//		Purpose: Run function for daemon
//		Created: 18/2/04
//
// --------------------------------------------------------------------------
void BackupDaemon::Run()
{
	// Ignore SIGPIPE (so that if a command connection is broken, the daemon doesn't terminate)
	::signal(SIGPIPE, SIG_IGN);

	// Create a command socket?
	const Configuration &conf(GetConfiguration());
	if(conf.KeyExists("CommandSocket"))
	{
		// Yes, create a local UNIX socket
		const char *socketName = conf.GetKeyValue("CommandSocket").c_str();
		mpCommandSocketInfo = new CommandSocketManager(conf, this, socketName);
	}

	// Handle things nicely on exceptions
	try
	{
		Run2();
	}
	catch(...)
	{
		if(mpCommandSocketInfo != 0)
		{
			delete mpCommandSocketInfo;
			mpCommandSocketInfo = 0;
		}

		throw;
	}
	
	// Clean up
	if(mpCommandSocketInfo != 0)
	{
		delete mpCommandSocketInfo;
		mpCommandSocketInfo = 0;
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::Run2()
//		Purpose: Run function for daemon (second stage)
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void BackupDaemon::Run2()
{
	// Read in the certificates creating a TLS context
	TLSContext tlsContext;
	const Configuration &conf(GetConfiguration());
	std::string certFile(conf.GetKeyValue("CertificateFile"));
	std::string keyFile(conf.GetKeyValue("PrivateKeyFile"));
	std::string caFile(conf.GetKeyValue("TrustedCAsFile"));
	tlsContext.Initialise(false /* as client */, certFile.c_str(), keyFile.c_str(), caFile.c_str());
	
	// Set up the keys for various things
	BackupClientCryptoKeys_Setup(conf.GetKeyValue("KeysFile").c_str());

	// Set maximum diffing time?
	if(conf.KeyExists("MaximumDiffingTime"))
	{
		BackupStoreFile::SetMaximumDiffingTime(conf.GetKeyValueInt("MaximumDiffingTime"));
	}

	// Setup various timings
	
	// How often to connect to the store (approximate)
	box_time_t updateStoreInterval = SecondsToBoxTime((uint32_t)conf.GetKeyValueInt("UpdateStoreInterval"));

	// But are we connecting automatically?
	bool automaticBackup = conf.GetKeyValueBool("AutomaticBackup");
	
	// The minimum age a file needs to be before it will be considered for uploading
	box_time_t minimumFileAge = SecondsToBoxTime((uint32_t)conf.GetKeyValueInt("MinimumFileAge"));

	// The maximum time we'll wait to upload a file, regardless of how often it's modified
	box_time_t maxUploadWait = SecondsToBoxTime((uint32_t)conf.GetKeyValueInt("MaxUploadWait"));
	// Adjust by subtracting the minimum file age, so is relative to sync period end in comparisons
	maxUploadWait = (maxUploadWait > minimumFileAge)?(maxUploadWait - minimumFileAge):(0);

	// When the next sync should take place -- which is ASAP
	box_time_t nextSyncTime = 0;

	// When the last sync started (only updated if the store was not full when the sync ended)
	box_time_t lastSyncTime = 0;
	
	// --------------------------------------------------------------------------------------------

	// And what's the current client store marker?
	int64_t clientStoreMarker = BackupClientContext::ClientStoreMarker_NotKnown;		// haven't contacted the store yet

	// Set state
	SetState(State_Idle);

	// Loop around doing backups
	do
	{
		// Flags used below
		bool storageLimitExceeded = false;

		// Is a delay necessary?
		{
			box_time_t currentTime;
			do
			{
				// Need to check the stop run thing here too, so this loop isn't run if we should be stopping
				if(StopRun()) break;
				
				currentTime = GetCurrentBoxTime();
	
				// Pause a while, but no more than MAX_SLEEP_TIME seconds (use the conditional because times are unsigned)
				box_time_t requiredDelay = (nextSyncTime < currentTime)?(0):(nextSyncTime - currentTime);
				// If there isn't automatic backup happening, set a long delay. And limit delays at the same time.
				if(!automaticBackup || requiredDelay > SecondsToBoxTime((uint32_t)MAX_SLEEP_TIME)) requiredDelay = SecondsToBoxTime((uint32_t)MAX_SLEEP_TIME);

				// Only do the delay if there is a delay required
				if(requiredDelay > 0)
				{
					// Sleep somehow. There are choices on how this should be done, depending on the state of the control connection
					if(mpCommandSocketInfo != 0)
					{
						// A command socket exists, so sleep by handling connections with it
						TRACE1("Wait on command socket, delay = %lld\n", requiredDelay);
						mpCommandSocketInfo->Wait(requiredDelay);
					}
					else
					{
						// No command socket or connection, just do a normal sleep
						int sleepSeconds = BoxTimeToSeconds(requiredDelay);
						::sleep((sleepSeconds <= 0)?1:sleepSeconds);
					}
				}
				
			} while((!automaticBackup || (currentTime < nextSyncTime)) 
				&& !mSyncRequested && !mSyncForced && !StopRun());
		}

		// Time of sync start, and if it's time for another sync (and we're doing automatic syncs), set the flag
		box_time_t currentSyncStartTime = GetCurrentBoxTime();
		if(automaticBackup && currentSyncStartTime >= nextSyncTime)
		{
			mSyncRequested = true;
		}
		
		bool doSync = mSyncForced;
		
		// Use a script to see if sync is allowed now?
		if(mSyncRequested && !StopRun())
		{
			int d = UseScriptToSeeIfSyncAllowed();
			if(d > 0)
			{
				// Script has asked for a delay
				nextSyncTime = GetCurrentBoxTime() + SecondsToBoxTime((uint32_t)d);
			}
			else
			{
				doSync = true;
			}
		}

		// Ready to sync? (but only if we're not supposed to be stopping)
		if(doSync && !StopRun())
		{
			mSyncRequested = false;
			mSyncForced    = false;
			
			// Touch a file to record times in filesystem
			TouchFileInWorkingDir("last_sync_start");
		
			// Tell anything connected to the command socket
			mpCommandSocketInfo->SendSyncStartOrFinish(true /* start */);
			
			// Reset statistics on uploads
			BackupStoreFile::ResetStats();
			
			// Calculate the sync period of files to examine
			box_time_t syncPeriodStart = lastSyncTime;
			box_time_t syncPeriodEnd = currentSyncStartTime - minimumFileAge;
			// Check logic
			ASSERT(syncPeriodEnd > syncPeriodStart);
			// Paranoid check on sync times
			if(syncPeriodStart >= syncPeriodEnd) continue;
			
			// Adjust syncPeriodEnd to emulate snapshot behaviour properly
			box_time_t syncPeriodEndExtended = syncPeriodEnd;
			// Using zero min file age?
			if(minimumFileAge == 0)
			{
				// Add a year on to the end of the end time, to make sure we sync
				// files which are modified after the scan run started.
				// Of course, they may be eligable to be synced again the next time round,
				// but this should be OK, because the changes only upload should upload no data.
				syncPeriodEndExtended += SecondsToBoxTime((uint32_t)(356*24*3600));
			}
			
			// Do sync
			bool errorOccurred = false;
			int errorCode = 0, errorSubCode = 0;
			try
			{
				// Set state and log start
				SetState(State_Connected);
				::syslog(LOG_INFO, "Beginning scan of local files");

				// Then create a client context object (don't just connect, as this may be unnecessary)
				BackupClientContext clientContext(*this, tlsContext, conf.GetKeyValue("StoreHostname"),
					conf.GetKeyValueInt("AccountNumber"), conf.GetKeyValueBool("ExtendedLogging"));
					
				// Set up the sync parameters
				BackupClientDirectoryRecord::SyncParams params(*this, clientContext);
				params.mSyncPeriodStart = syncPeriodStart;
				params.mSyncPeriodEnd = syncPeriodEndExtended; // use potentially extended end time
				params.mMaxUploadWait = maxUploadWait;
				params.mFileTrackingSizeThreshold = conf.GetKeyValueInt("FileTrackingSizeThreshold");
				params.mDiffingUploadSizeThreshold = conf.GetKeyValueInt("DiffingUploadSizeThreshold");
				params.mMaxFileTimeInFuture = SecondsToBoxTime((uint32_t)conf.GetKeyValueInt("MaxFileTimeInFuture"));
				params.mpCommandSocket = mpCommandSocketInfo;
				
				// Set store marker
				clientContext.SetClientStoreMarker(clientStoreMarker);
				
				// Set up the locations, if necessary -- need to do it here so we have a (potential) connection to use
				if(mLocations.empty())
				{
					const Configuration &locations(conf.GetSubConfiguration("BackupLocations"));
					
					// Make sure all the directory records are set up
					SetupLocations(clientContext, locations);
				}
				
				// Get some ID maps going
				SetupIDMapsForSync();

				// Delete any unused directories?
				DeleteUnusedRootDirEntries(clientContext);
								
				// Go through the records, syncing them
				for(std::vector<Location *>::const_iterator i(mLocations.begin()); i != mLocations.end(); ++i)
				{
					// Set current and new ID map pointers in the context
					clientContext.SetIDMaps(mCurrentIDMaps[(*i)->mIDMapIndex], mNewIDMaps[(*i)->mIDMapIndex]);
				
					// Set exclude lists (context doesn't take ownership)
					clientContext.SetExcludeLists((*i)->mpExcludeFiles, (*i)->mpExcludeDirs);

					// Sync the directory
					(*i)->mpDirectoryRecord->SyncDirectory(params, BackupProtocolClientListDirectory::RootDirectory, (*i)->mPath);

					// Unset exclude lists (just in case)
					clientContext.SetExcludeLists(0, 0);
				}
				
				// Errors reading any files?
				if(params.mReadErrorsOnFilesystemObjects)
				{
					// Notify administrator
					NotifySysadmin(NotifyEvent_ReadError);
				}
				else
				{
					// Unset the read error flag, so the error is
					// reported again in the future
					mNotificationsSent[NotifyEvent_ReadError] = false;
				}
				
				// Perform any deletions required -- these are delayed until the end
				// to allow renaming to happen neatly.
				clientContext.PerformDeletions();

				// Close any open connection
				clientContext.CloseAnyOpenConnection();
				
				// Get the new store marker
				clientStoreMarker = clientContext.GetClientStoreMarker();
				
				// Check the storage limit
				if(clientContext.StorageLimitExceeded())
				{
					// Tell the sysadmin about this
					NotifySysadmin(NotifyEvent_StoreFull);
				}
				else
				{
					// The start time of the next run is the end time of this run
					// This is only done if the storage limit wasn't exceeded (as things won't have been done properly if it was)
					lastSyncTime = syncPeriodEnd;
					// unflag the storage full notify flag so that next time the store is full, and alert will be sent
					mNotificationsSent[NotifyEvent_StoreFull] = false;
				}
				
				// Calculate when the next sync run should be
				nextSyncTime = currentSyncStartTime + updateStoreInterval + Random::RandomInt(updateStoreInterval >> SYNC_PERIOD_RANDOM_EXTRA_TIME_SHIFT_BY);
			
				// Commit the ID Maps
				CommitIDMapsAfterSync();

				// Log
				::syslog(LOG_INFO, "Finished scan of local files");
			}
			catch(BoxException &e)
			{
				errorOccurred = true;
				errorCode = e.GetType();
				errorSubCode = e.GetSubType();
			}
			catch(...)
			{
				// TODO: better handling of exceptions here... need to be very careful
				errorOccurred = true;
			}
			
			if(errorOccurred)
			{
				// Is it a berkely db failure?
				bool isBerkelyDbFailure = (errorCode == BackupStoreException::ExceptionType
					&& errorSubCode == BackupStoreException::BerkelyDBFailure);
				if(isBerkelyDbFailure)
				{
					// Delete corrupt files
					DeleteCorruptBerkelyDbFiles();
				}

				// Clear state data
				syncPeriodStart = 0;	// go back to beginning of time
				clientStoreMarker = BackupClientContext::ClientStoreMarker_NotKnown;	// no store marker, so download everything
				DeleteAllLocations();
				DeleteAllIDMaps();

				// Handle restart?
				if(StopRun())
				{
					::syslog(LOG_INFO, "Exception (%d/%d) due to signal", errorCode, errorSubCode);
					return;
				}

				// If the Berkely db files get corrupted, delete them and try again immediately
				if(isBerkelyDbFailure)
				{
					::syslog(LOG_ERR, "Berkely db inode map files corrupted, deleting and restarting scan. Renamed files and directories will not be tracked until after this scan.\n");
					::sleep(1);
				}
				else
				{
					// Not restart/terminate, pause and retry
					SetState(State_Error);
					::syslog(LOG_ERR, "Exception caught (%d/%d), reset state and waiting to retry...", errorCode, errorSubCode);
					
					// Sleep somehow. There are choices on how this should be
					// done, depending on the state of the control connection

					if(mpCommandSocketInfo != 0)
					{
						// A command socket exists, so sleep by handling connections with it
						mpCommandSocketInfo->Wait(100 * 1000 * 1000);
					}
					else
					{
						// No command socket or connection, just do a normal sleep
						::sleep(100);
					}
				}
			}

			// Log the stats
			::syslog(LOG_INFO, "File statistics: total file size uploaded %lld, bytes already on server %lld, encoded size %lld",
				BackupStoreFile::msStats.mBytesInEncodedFiles, BackupStoreFile::msStats.mBytesAlreadyOnServer,
				BackupStoreFile::msStats.mTotalFileStreamSize);
			BackupStoreFile::ResetStats();

			// Tell anything connected to the command socket
			mpCommandSocketInfo->SendSyncStartOrFinish(false /* finish */);

			// Touch a file to record times in filesystem
			TouchFileInWorkingDir("last_sync_finish");
		}
		
		// Set state
		SetState(storageLimitExceeded?State_StorageLimitExceeded:State_Idle);

	} while(!StopRun());
	
	// Make sure we have a clean start next time round (if restart)
	DeleteAllLocations();
	DeleteAllIDMaps();
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::UseScriptToSeeIfSyncAllowed()
//		Purpose: Private. Use a script to see if the sync should be allowed (if configured)
//				 Returns -1 if it's allowed, time in seconds to wait otherwise.
//		Created: 21/6/04
//
// --------------------------------------------------------------------------
int BackupDaemon::UseScriptToSeeIfSyncAllowed()
{
	const Configuration &conf(GetConfiguration());

	// Got a script to run?
	if(!conf.KeyExists("SyncAllowScript"))
	{
		// No. Do sync.
		return -1;
	}

	// If there's no result, try again in five minutes
	int waitInSeconds = (60*5);

	// Run it?
	pid_t pid = 0;
	try
	{
		std::auto_ptr<IOStream> pscript(LocalProcessStream(conf.GetKeyValue("SyncAllowScript").c_str(), pid));

		// Read in the result
		IOStreamGetLine getLine(*pscript);
		std::string line;
		if(getLine.GetLine(line, true, 30000)) // 30 seconds should be enough
		{
			// Got a string, intepret
			if(line == "now")
			{
				// Script says do it now. Obey.
				waitInSeconds = -1;
			}
			else
			{
				// How many seconds to wait?
				waitInSeconds = BoxConvert::Convert<int32_t, const std::string&>(line);
				::syslog(LOG_INFO, "Delaying sync by %d seconds (SyncAllowScript '%s')", waitInSeconds, conf.GetKeyValue("SyncAllowScript").c_str());
			}
		}
		
		// Wait and then cleanup child process
		int status = 0;
		::waitpid(pid, &status, 0);
	}
	catch(...)
	{
		// Ignore any exceptions
		// Log that something bad happened
		::syslog(LOG_ERR, "Error running SyncAllowScript '%s'", conf.GetKeyValue("SyncAllowScript").c_str());
		// Clean up though
		if(pid != 0)
		{
			int status = 0;
			::waitpid(pid, &status, 0);
		}
	}

	return waitInSeconds;
}







#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
	// string comparison ordering for when mount points are handled
	// by code, rather than the OS.
	typedef struct
	{
		bool operator()(const std::string &s1, const std::string &s2)
		{
			if(s1.size() == s2.size())
			{
				// Equal size, sort according to natural sort order
				return s1 < s2;
			}
			else
			{
				// Make sure longer strings go first
				return s1.size() > s2.size();
			}
		}
	} mntLenCompare;
#endif

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::SetupLocations(BackupClientContext &, const Configuration &)
//		Purpose: Makes sure that the list of directories records is correctly set up
//		Created: 2003/10/08
//
// --------------------------------------------------------------------------
void BackupDaemon::SetupLocations(BackupClientContext &rClientContext, const Configuration &rLocationsConf)
{
	if(!mLocations.empty())
	{
		// Looks correctly set up
		return;
	}

	// Make sure that if a directory is reinstated, then it doesn't get deleted	
	mDeleteUnusedRootDirEntriesAfter = 0;
	mUnusedRootDirEntries.clear();

	// Just a check to make sure it's right.
	DeleteAllLocations();
	
	// Going to need a copy of the root directory. Get a connection, and fetch it.
	BackupProtocolClient &connection(rClientContext.GetConnection());
	
	// Ask server for a list of everything in the root directory, which is a directory itself
	std::auto_ptr<BackupProtocolClientSuccess> dirreply(connection.QueryListDirectory(
			BackupProtocolClientListDirectory::RootDirectory,
			BackupProtocolClientListDirectory::Flags_Dir,	// only directories
			BackupProtocolClientListDirectory::Flags_Deleted | BackupProtocolClientListDirectory::Flags_OldVersion, // exclude old/deleted stuff
			false /* no attributes */));

	// Retrieve the directory from the stream following
	BackupStoreDirectory dir;
	std::auto_ptr<IOStream> dirstream(connection.ReceiveStream());
	dir.ReadFromStream(*dirstream, connection.GetTimeout());
	
	// Map of mount names to ID map index
	std::map<std::string, int> mounts;
	int numIDMaps = 0;

#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
	// Linux can't tell you where a directory is mounted. So we have to
	// read the mount entries from /etc/mtab! Bizarre that the OS itself
	// can't tell you, but there you go.
	std::set<std::string, mntLenCompare> mountPoints;
	// BLOCK
	FILE *mountPointsFile = 0;
	try
	{
		// Open mounts file
		mountPointsFile = ::setmntent("/etc/mtab", "r");
		if(mountPointsFile == 0)
		{
			THROW_EXCEPTION(CommonException, OSFileError);
		}
		
		// Read all the entries, and put them in the set
		struct mntent *entry = 0;
		while((entry = ::getmntent(mountPointsFile)) != 0)
		{
			TRACE1("Found mount point at %s\n", entry->mnt_dir);
			mountPoints.insert(std::string(entry->mnt_dir));
		}
		
		// Close mounts file
		::endmntent(mountPointsFile);
	}
	catch(...)
	{
		if(mountPointsFile != 0)
		{
			::endmntent(mountPointsFile);
		}
		throw;
	}
	// Check sorting and that things are as we expect
	ASSERT(mountPoints.size() > 0);
#ifndef NDEBUG
	{
		std::set<std::string, mntLenCompare>::const_reverse_iterator i(mountPoints.rbegin());
		ASSERT(*i == "/");
	}
#endif // n NDEBUG
#endif // PLATFORM_USES_MTAB_FILE_FOR_MOUNTS

	// Then... go through each of the entries in the configuration,
	// making sure there's a directory created for it.
	for(std::list<std::pair<std::string, Configuration> >::const_iterator i = rLocationsConf.mSubConfigurations.begin();
		i != rLocationsConf.mSubConfigurations.end(); ++i)
	{
TRACE0("new location\n");
		// Create a record for it
		Location *ploc = new Location;
		try
		{
			// Setup names in the location record
			ploc->mName = i->first;
			ploc->mPath = i->second.GetKeyValue("Path");
			
			// Read the exclude lists from the Configuration
			ploc->mpExcludeFiles = BackupClientMakeExcludeList_Files(i->second);
			ploc->mpExcludeDirs = BackupClientMakeExcludeList_Dirs(i->second);
			
			// Do a fsstat on the pathname to find out which mount it's on
			{
#ifdef PLATFORM_USES_MTAB_FILE_FOR_MOUNTS
				// Warn in logs if the directory isn't absolute
				if(ploc->mPath[0] != '/')
				{
					::syslog(LOG_ERR, "Location path '%s' isn't absolute", ploc->mPath.c_str());
				}
				// Go through the mount points found, and find a suitable one
				std::string mountName("/");
				{
					std::set<std::string, mntLenCompare>::const_iterator i(mountPoints.begin());
					TRACE1("%d potential mount points\n", mountPoints.size());
					for(; i != mountPoints.end(); ++i)
					{
						// Compare first n characters with the filename
						// If it matches, the file belongs in that mount point
						// (sorting order ensures this)
						TRACE1("checking against mount point %s\n", i->c_str());
						if(::strncmp(i->c_str(), ploc->mPath.c_str(), i->size()) == 0)
						{
							// Match
							mountName = *i;
							break;
						}
					}
					TRACE2("mount point chosen for %s is %s\n", ploc->mPath.c_str(), mountName.c_str());
				}
#else
				// BSD style statfs -- includes mount point, which is nice.
				struct statfs s;
				if(::statfs(ploc->mPath.c_str(), &s) != 0)
				{
					THROW_EXCEPTION(CommonException, OSFileError)
				}
				
				// Where the filesystem is mounted
				std::string mountName(s.f_mntonname);
#endif
				
				// Got it?
				std::map<std::string, int>::iterator f(mounts.find(mountName));
				if(f != mounts.end())
				{
					// Yes -- store the index
					ploc->mIDMapIndex = f->second;
				}
				else
				{
					// No -- new index
					ploc->mIDMapIndex = numIDMaps;
					mounts[mountName] = numIDMaps;
					
					// Store the mount name
					mIDMapMounts.push_back(mountName);
					
					// Increment number of maps
					++numIDMaps;
				}
			}
		
			// Does this exist on the server?
			BackupStoreDirectory::Iterator iter(dir);
			BackupStoreFilenameClear dirname(ploc->mName);	// generate the filename
			BackupStoreDirectory::Entry *en = iter.FindMatchingClearName(dirname);
			int64_t oid = 0;
			if(en != 0)
			{
				oid = en->GetObjectID();
				
				// Delete the entry from the directory, so we get a list of
				// unused root directories at the end of this.
				dir.DeleteEntry(oid);
			}
			else
			{
				// Doesn't exist, so it has to be created on the server. Let's go!
				// First, get the directory's attributes and modification time
				box_time_t attrModTime = 0;
				BackupClientFileAttributes attr;
				attr.ReadAttributes(ploc->mPath.c_str(), true /* directories have zero mod times */,
					0 /* not interested in mod time */, &attrModTime /* get the attribute modification time */);
				
				// Execute create directory command
				MemBlockStream attrStream(attr);
				std::auto_ptr<BackupProtocolClientSuccess> dirCreate(connection.QueryCreateDirectory(
					BackupProtocolClientListDirectory::RootDirectory,
					attrModTime, dirname, attrStream));
					
				// Object ID for later creation
				oid = dirCreate->GetObjectID();
			}

			// Create and store the directory object for the root of this location
			ASSERT(oid != 0);
			BackupClientDirectoryRecord *precord = new BackupClientDirectoryRecord(oid, i->first);
			ploc->mpDirectoryRecord.reset(precord);
			
			// Push it back on the vector of locations
			mLocations.push_back(ploc);
		}
		catch(...)
		{
			delete ploc;
			ploc = 0;
			throw;
		}
	}
	
	// Any entries in the root directory which need deleting?
	if(dir.GetNumberOfEntries() > 0)
	{
		::syslog(LOG_INFO, "%d redundant locations in root directory found, will delete from store after %d seconds.",
			dir.GetNumberOfEntries(), BACKUP_DELETE_UNUSED_ROOT_ENTRIES_AFTER);

		// Store directories in list of things to delete
		mUnusedRootDirEntries.clear();
		BackupStoreDirectory::Iterator iter(dir);
		BackupStoreDirectory::Entry *en = 0;
		while((en = iter.Next()) != 0)
		{
			// Add name to list
			BackupStoreFilenameClear clear(en->GetName());
			const std::string &name(clear.GetClearFilename());
			mUnusedRootDirEntries.push_back(std::pair<int64_t,std::string>(en->GetObjectID(), name));
			// Log this
			::syslog(LOG_INFO, "Unused location in root: %s", name.c_str());
		}
		ASSERT(mUnusedRootDirEntries.size() > 0);
		// Time to delete them
		mDeleteUnusedRootDirEntriesAfter =
			GetCurrentBoxTime() + SecondsToBoxTime((uint32_t)BACKUP_DELETE_UNUSED_ROOT_ENTRIES_AFTER);
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::SetupIDMapsForSync()
//		Purpose: Sets up ID maps for the sync process -- make sure they're all there
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupDaemon::SetupIDMapsForSync()
{
	// Need to do different things depending on whether it's an in memory implementation,
	// or whether it's all stored on disc.
	
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION

	// Make sure we have some blank, empty ID maps
	DeleteIDMapVector(mNewIDMaps);
	FillIDMapVector(mNewIDMaps, true /* new maps */);

	// Then make sure that the current maps have objects, even if they are empty
	// (for the very first run)
	if(mCurrentIDMaps.empty())
	{
		FillIDMapVector(mCurrentIDMaps, false /* current maps */);
	}

#else

	// Make sure we have some blank, empty ID maps
	DeleteIDMapVector(mNewIDMaps);
	FillIDMapVector(mNewIDMaps, true /* new maps */);
	DeleteIDMapVector(mCurrentIDMaps);
	FillIDMapVector(mCurrentIDMaps, false /* new maps */);

#endif
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::FillIDMapVector(std::vector<BackupClientInodeToIDMap *> &)
//		Purpose: Fills the vector with the right number of empty ID maps
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupDaemon::FillIDMapVector(std::vector<BackupClientInodeToIDMap *> &rVector, bool NewMaps)
{
	ASSERT(rVector.size() == 0);
	rVector.reserve(mIDMapMounts.size());
	
	for(unsigned int l = 0; l < mIDMapMounts.size(); ++l)
	{
		// Create the object
		BackupClientInodeToIDMap *pmap = new BackupClientInodeToIDMap();
		try
		{
			// Get the base filename of this map
			std::string filename;
			MakeMapBaseName(l, filename);
			
			// If it's a new one, add a suffix
			if(NewMaps)
			{
				filename += ".n";
			}

			// If it's not a new map, it may not exist in which case an empty map should be created
			if(!NewMaps && !FileExists(filename.c_str()))
			{
				pmap->OpenEmpty();
			}
			else
			{
				// Open the map
				pmap->Open(filename.c_str(), !NewMaps /* read only */, NewMaps /* create new */);
			}
			
			// Store on vector
			rVector.push_back(pmap);
		}
		catch(...)
		{
			delete pmap;
			throw;
		}
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteCorruptBerkelyDbFiles()
//		Purpose: Delete the Berkely db files from disc after they have been corrupted.
//		Created: 14/9/04
//
// --------------------------------------------------------------------------
void BackupDaemon::DeleteCorruptBerkelyDbFiles()
{
	for(unsigned int l = 0; l < mIDMapMounts.size(); ++l)
	{
		// Get the base filename of this map
		std::string filename;
		MakeMapBaseName(l, filename);
		
		// Delete the file
		TRACE1("Deleting %s\n", filename.c_str());
		::unlink(filename.c_str());
		
		// Add a suffix for the new map
		filename += ".n";

		// Delete that too
		TRACE1("Deleting %s\n", filename.c_str());
		::unlink(filename.c_str());
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    MakeMapBaseName(unsigned int, std::string &)
//		Purpose: Makes the base name for a inode map
//		Created: 20/11/03
//
// --------------------------------------------------------------------------
void BackupDaemon::MakeMapBaseName(unsigned int MountNumber, std::string &rNameOut) const
{
	// Get the directory for the maps
	const Configuration &config(GetConfiguration());
	std::string dir(config.GetKeyValue("DataDirectory"));

	// Make a leafname
	std::string leaf(mIDMapMounts[MountNumber]);
	for(unsigned int z = 0; z < leaf.size(); ++z)
	{
		if(leaf[z] == DIRECTORY_SEPARATOR_ASCHAR)
		{
			leaf[z] = '_';
		}
	}

	// Build the final filename
	rNameOut = dir + DIRECTORY_SEPARATOR "mnt" + leaf;
}




// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::CommitIDMapsAfterSync()
//		Purpose: Commits the new ID maps, so the 'new' maps are now the 'current' maps.
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupDaemon::CommitIDMapsAfterSync()
{
	// Need to do different things depending on whether it's an in memory implementation,
	// or whether it's all stored on disc.
	
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	// Remove the current ID maps
	DeleteIDMapVector(mCurrentIDMaps);

	// Copy the (pointers to) "new" maps over to be the new "current" maps
	mCurrentIDMaps = mNewIDMaps;
	
	// Clear the new ID maps vector (not delete them!)
	mNewIDMaps.clear();

#else

	// Get rid of the maps in memory (leaving them on disc of course)
	DeleteIDMapVector(mCurrentIDMaps);
	DeleteIDMapVector(mNewIDMaps);

	// Then move the old maps into the new places
	for(unsigned int l = 0; l < mIDMapMounts.size(); ++l)
	{
		std::string target;
		MakeMapBaseName(l, target);
		std::string newmap(target + ".n");
		
		// Try to rename
		if(::rename(newmap.c_str(), target.c_str()) != 0)
		{
			THROW_EXCEPTION(CommonException, OSFileError)
		}
	}

#endif
}



// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteIDMapVector(std::vector<BackupClientInodeToIDMap *> &)
//		Purpose: Deletes the contents of a vector of ID maps
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupDaemon::DeleteIDMapVector(std::vector<BackupClientInodeToIDMap *> &rVector)
{
	while(!rVector.empty())
	{
		// Pop off list
		BackupClientInodeToIDMap *toDel = rVector.back();
		rVector.pop_back();
		
		// Close and delete
		toDel->Close();
		delete toDel;
	}
	ASSERT(rVector.size() == 0);
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::FindLocationPathName(const std::string &, std::string &) const
//		Purpose: Tries to find the path of the root of a backup location. Returns true (and path in rPathOut)
//				 if it can be found, false otherwise.
//		Created: 12/11/03
//
// --------------------------------------------------------------------------
bool BackupDaemon::FindLocationPathName(const std::string &rLocationName, std::string &rPathOut) const
{
	// Search for the location
	for(std::vector<Location *>::const_iterator i(mLocations.begin()); i != mLocations.end(); ++i)
	{
		if((*i)->mName == rLocationName)
		{
			rPathOut = (*i)->mPath;
			return true;
		}
	}
	
	// Didn't find it
	return false;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::SetState(int)
//		Purpose: Record current action of daemon, and update process title to reflect this
//		Created: 11/12/03
//
// --------------------------------------------------------------------------
void BackupDaemon::SetState(state_t State)
{
	// Two little checks
	if(State == mState) return;
	if(State < 0) return;

	// Update
	mState = State;
	
	// Set process title
	const static char *stateText[] = {"idle", "connected", "error -- waiting for retry", "over limit on server -- not backing up"};
	SetProcessTitle(stateText[State]);
	
	// If there's a command socket connected, then inform it -- disconnecting from the
	// command socket if there's an error
	if(mpCommandSocketInfo != 0)
	{
		mpCommandSocketInfo->SendStateUpdate(mState);
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::TouchFileInWorkingDir(const char *)
//		Purpose: Make sure a zero length file of the name exists in the working directory.
//				 Use for marking times of events in the filesystem.
//		Created: 21/2/04
//
// --------------------------------------------------------------------------
void BackupDaemon::TouchFileInWorkingDir(const char *Filename)
{
	// Filename
	const Configuration &config(GetConfiguration());
	std::string fn(config.GetKeyValue("DataDirectory") + DIRECTORY_SEPARATOR_ASCHAR);
	fn += Filename;
	
	// Open and close it to update the timestamp
	FileStream touch(fn.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::NotifySysadmin(int)
//		Purpose: Run the script to tell the sysadmin about events which need attention.
//		Created: 25/2/04
//
// --------------------------------------------------------------------------
void BackupDaemon::NotifySysadmin(int Event)
{
	static const char *sEventNames[] = {"store-full", "read-error", 0};

	TRACE1("BackupDaemon::NotifySysadmin() called, event = %d\n", Event);

	if(Event < 0 || Event > NotifyEvent__MAX)
	{
		THROW_EXCEPTION(BackupStoreException, BadNotifySysadminEventCode);
	}

	// Don't send lots of repeated messages
	if(mNotificationsSent[Event])
	{
		return;
	}

	// Is there a notifation script?
	const Configuration &conf(GetConfiguration());
	if(!conf.KeyExists("NotifyScript"))
	{
		// Log, and then return
		::syslog(LOG_ERR, "Not notifying administrator about event %s -- set NotifyScript to do this in future", sEventNames[Event]);
		return;
	}

	// Script to run
	std::string script(conf.GetKeyValue("NotifyScript") + ' ' + sEventNames[Event]);
	
	// Log what we're about to do
	::syslog(LOG_INFO, "About to notify administrator about event %s, running script '%s'", sEventNames[Event], script.c_str());
	
	// Then do it
	if(::system(script.c_str()) != 0)
	{
		::syslog(LOG_ERR, "Notify script returned an error code. ('%s')", script.c_str());
	}

	// Flag that this is done so the administrator isn't constantly bombarded with lots of errors
	mNotificationsSent[Event] = true;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::DeleteUnusedRootDirEntries(BackupClientContext &)
//		Purpose: Deletes any unused entries in the root directory, if they're scheduled to be deleted.
//		Created: 13/5/04
//
// --------------------------------------------------------------------------
void BackupDaemon::DeleteUnusedRootDirEntries(BackupClientContext &rContext)
{
	if(mUnusedRootDirEntries.empty() || mDeleteUnusedRootDirEntriesAfter == 0)
	{
		// Nothing to do.
		return;
	}
	
	// Check time
	if(GetCurrentBoxTime() < mDeleteUnusedRootDirEntriesAfter)
	{
		// Too early to delete files
		return;
	}

	// Entries to delete, and it's the right time to do so...
	::syslog(LOG_INFO, "Deleting unused locations from store root...");
	BackupProtocolClient &connection(rContext.GetConnection());
	for(std::vector<std::pair<int64_t,std::string> >::iterator i(mUnusedRootDirEntries.begin()); i != mUnusedRootDirEntries.end(); ++i)
	{
		connection.QueryDeleteDirectory(i->first);
		
		// Log this
		::syslog(LOG_INFO, "Deleted %s (ID %08llx) from store root", i->second.c_str(), i->first);
	}

	// Reset state
	mDeleteUnusedRootDirEntriesAfter = 0;
	mUnusedRootDirEntries.clear();
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::Location::Location()
//		Purpose: Constructor
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
BackupDaemon::Location::Location()
	: mIDMapIndex(0),
	  mpExcludeFiles(0),
	  mpExcludeDirs(0)
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupDaemon::Location::~Location()
//		Purpose: Destructor
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
BackupDaemon::Location::~Location()
{
	// Clean up exclude locations
	if(mpExcludeDirs != 0)
	{
		delete mpExcludeDirs;
		mpExcludeDirs = 0;
	}
	if(mpExcludeFiles != 0)
	{
		delete mpExcludeFiles;
		mpExcludeFiles = 0;
	}
}
