// --------------------------------------------------------------------------
//
// File
//		Name:    Daemon.h
//		Purpose: Basic daemon functionality
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------

/*  NOTE: will log to local6: include a line like
	local6.info                                             /var/log/box
	in /etc/syslog.conf
*/


#ifndef DAEMON__H
#define DAEMON__H

#include <string>

#include "BoxTime.h"

class Configuration;
class ConfigurationVerify;

// --------------------------------------------------------------------------
//
// Class
//		Name:    Daemon
//		Purpose: Basic daemon functionality
//		Created: 2003/07/29
//
// --------------------------------------------------------------------------
class Daemon
{
public:
	Daemon();
	virtual ~Daemon();
private:
	Daemon(const Daemon &rToCopy);
public:

	virtual int Main(const char *DefaultConfigFile, int argc,
		const char *argv[]);

	/* override this Main() if you want custom option processing: */
	virtual int Main(const std::string &rConfigFile);
	
	virtual void Run();
	const Configuration &GetConfiguration() const;
	const std::string &GetConfigFileName() const {return mConfigFileName;}

	virtual const char *DaemonName() const;
	virtual std::string DaemonBanner() const;
	virtual const ConfigurationVerify *GetConfigVerify() const;
	virtual void Usage();
	
	bool StopRun() {return mReloadConfigWanted | mTerminateWanted;}
	bool IsReloadConfigWanted() {return mReloadConfigWanted;}
	bool IsTerminateWanted() {return mTerminateWanted;}

	// To allow derived classes to get these signals in other ways
	void SetReloadConfigWanted() {mReloadConfigWanted = true;}
	void SetTerminateWanted() {mTerminateWanted = true;}
	
	virtual void SetupInInitialProcess();
	virtual void EnterChild();
	
	static void SetProcessTitle(const char *format, ...);

protected:
	box_time_t GetLoadedConfigModifiedTime() const;
	bool IsSingleProcess() { return mSingleProcess; }
	virtual std::string GetOptionString();
	virtual int ProcessOption(signed int option);
	
private:
	static void SignalHandler(int sigraised);
	box_time_t GetConfigFileModifiedTime() const;
	
	std::string mConfigFileName;
	Configuration *mpConfiguration;
	box_time_t mLoadedConfigModifiedTime;
	bool mReloadConfigWanted;
	bool mTerminateWanted;
	bool mSingleProcess;
	bool mRunInForeground;
	bool mKeepConsoleOpenAfterFork;
	bool mHaveConfigFile;
	int mLogLevel; // need an int to do math with
	static Daemon *spDaemon;
	std::string mAppName;
};

#define DAEMON_VERIFY_SERVER_KEYS 	{"PidFile", 0, ConfigTest_Exists, 0}, \
									{"User", 0, ConfigTest_LastEntry, 0}

#endif // DAEMON__H
