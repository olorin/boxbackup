// --------------------------------------------------------------------------
//
// File
//		Name:    BackupStoreFilenameClear.cpp
//		Purpose: BackupStoreFilenames in the clear
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------

#include "Box.h"
#include "BackupStoreFilenameClear.h"
#include "BackupStoreException.h"
#include "CipherContext.h"
#include "CipherBlowfish.h"
#include "Guards.h"
#include "Logging.h"

#include "MemLeakFindOn.h"

// Hide private variables from the rest of the world
namespace
{
	int sEncodeMethod = BackupStoreFilename::Encoding_Clear;
	CipherContext sBlowfishEncrypt;
	CipherContext sBlowfishDecrypt;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::BackupStoreFilenameClear()
//		Purpose: Default constructor, creates an invalid filename
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
BackupStoreFilenameClear::BackupStoreFilenameClear()
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::BackupStoreFilenameClear(const std::string &)
//		Purpose: Creates a filename, encoding from the given string
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
BackupStoreFilenameClear::BackupStoreFilenameClear(const std::string &rToEncode)
{
	SetClearFilename(rToEncode);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::BackupStoreFilenameClear(const BackupStoreFilenameClear &)
//		Purpose: Copy constructor
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
BackupStoreFilenameClear::BackupStoreFilenameClear(const BackupStoreFilenameClear &rToCopy)
	: BackupStoreFilename(rToCopy),
	  mClearFilename(rToCopy.mClearFilename)
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::BackupStoreFilenameClear(const BackupStoreFilename &rToCopy)
//		Purpose: Copy from base class
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
BackupStoreFilenameClear::BackupStoreFilenameClear(const BackupStoreFilename &rToCopy)
	: BackupStoreFilename(rToCopy)
{
	// Will get a clear filename when it's required
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::~BackupStoreFilenameClear()
//		Purpose: Destructor
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
BackupStoreFilenameClear::~BackupStoreFilenameClear()
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::GetClearFilename()
//		Purpose: Get the unencoded filename
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
#ifdef BACKUPSTOREFILEAME_MALLOC_ALLOC_BASE_TYPE
const std::string BackupStoreFilenameClear::GetClearFilename() const
{
	MakeClearAvailable();
	// When modifying, remember to change back to reference return if at all possible
	// -- returns an object rather than a reference to allow easy use with other code.
	return std::string(mClearFilename.c_str(), mClearFilename.size());
}
#else
const std::string &BackupStoreFilenameClear::GetClearFilename() const
{
	MakeClearAvailable();
	return mClearFilename;
}
#endif

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::SetClearFilename(const std::string &)
//		Purpose: Encode and make available the clear filename
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::SetClearFilename(const std::string &rToEncode)
{
	// Only allow Blowfish encodings
	if(sEncodeMethod != Encoding_Blowfish)
	{
		THROW_EXCEPTION(BackupStoreException, FilenameEncryptionNotSetup)
	}

	// Make an encoded string with blowfish encryption
	EncryptClear(rToEncode, sBlowfishEncrypt, Encoding_Blowfish);
		
	// Store the clear filename
	mClearFilename.assign(rToEncode.c_str(), rToEncode.size());

	// Make sure we did the right thing
	if(!CheckValid(false))
	{
		THROW_EXCEPTION(BackupStoreException, Internal)
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::MakeClearAvailable()
//		Purpose: Private. Make sure the clear filename is available
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::MakeClearAvailable() const
{
	if(!mClearFilename.empty())
		return;		// nothing to do

	// Check valid
	CheckValid();
		
	// Decode the header
	int size = BACKUPSTOREFILENAME_GET_SIZE(GetEncodedFilename());
	int encoding = BACKUPSTOREFILENAME_GET_ENCODING(GetEncodedFilename());
	
	// Decode based on encoding given in the header
	switch(encoding)
	{
	case Encoding_Clear:
		BOX_TRACE("**** BackupStoreFilename encoded with "
			"Clear encoding ****");
		mClearFilename.assign(GetEncodedFilename().c_str() + 2,
			size - 2);
		break;
		
	case Encoding_Blowfish:
		DecryptEncoded(sBlowfishDecrypt);
		break;
	
	default:
		THROW_EXCEPTION(BackupStoreException, UnknownFilenameEncoding)
		break;	
	}
}


// Buffer for encoding and decoding -- do this all in one single buffer to
// avoid lots of string allocation, which stuffs up memory usage.
// These static memory vars are, of course, not thread safe, but we don't use threads.
static size_t sEncDecBufferSize = 0;
static MemoryBlockGuard<uint8_t *> *spEncDecBuffer = 0;

static void EnsureEncDecBufferSize(size_t BufSize)
{
	if(spEncDecBuffer == 0)
	{
#ifndef WIN32
		BOX_TRACE("Allocating filename encoding/decoding buffer "
			"with size " << BufSize);
#endif
		spEncDecBuffer = new MemoryBlockGuard<uint8_t *>(BufSize);
		MEMLEAKFINDER_NOT_A_LEAK(spEncDecBuffer);
		MEMLEAKFINDER_NOT_A_LEAK(*spEncDecBuffer);
		sEncDecBufferSize = BufSize;
	}
	else
	{
		if(sEncDecBufferSize < BufSize)
		{
			BOX_TRACE("Reallocating filename encoding/decoding "
				"buffer from " << sEncDecBufferSize <<
				" to " << BufSize);
			spEncDecBuffer->Resize(BufSize);
			sEncDecBufferSize = BufSize;
			MEMLEAKFINDER_NOT_A_LEAK(*spEncDecBuffer);
		}
	}
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::EncryptClear(const std::string &, CipherContext &, int)
//		Purpose: Private. Assigns the encoded filename string, encrypting.
//		Created: 1/12/03
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::EncryptClear(const std::string &rToEncode, CipherContext &rCipherContext, int StoreAsEncoding)
{
	// Work out max size
	size_t maxOutSize = rCipherContext.MaxOutSizeForInBufferSize(rToEncode.size()) + 4;
	
	// Make sure encode/decode buffer has enough space
	EnsureEncDecBufferSize(maxOutSize);
	
	// Pointer to buffer
	uint8_t *buffer = *spEncDecBuffer;
	
	// Encode -- do entire block in one go
	size_t encSize = rCipherContext.TransformBlock(buffer + 2, sEncDecBufferSize - 2, rToEncode.c_str(), rToEncode.size());
	// and add in header size
	encSize += 2;
	
	// Adjust header
	BACKUPSTOREFILENAME_MAKE_HDR(buffer, encSize, StoreAsEncoding);
	
	// Store the encoded string
	SetEncodedFilename(std::string((char*)buffer, encSize));
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::DecryptEncoded(CipherContext &)
//		Purpose: Decrypt the encoded filename using the cipher context
//		Created: 1/12/03
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::DecryptEncoded(CipherContext &rCipherContext) const
{
	const std::string& rEncoded = GetEncodedFilename();

	// Work out max size
	size_t maxOutSize = rCipherContext.MaxOutSizeForInBufferSize(rEncoded.size()) + 4;
	
	// Make sure encode/decode buffer has enough space
	EnsureEncDecBufferSize(maxOutSize);
	
	// Pointer to buffer
	uint8_t *buffer = *spEncDecBuffer;
	
	// Decrypt
	const char *str = rEncoded.c_str() + 2;
	size_t sizeOut = rCipherContext.TransformBlock(buffer, sEncDecBufferSize, str, rEncoded.size() - 2);
	
	// Assign to this
	mClearFilename.assign((char*)buffer, sizeOut);
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::EncodedFilenameChanged()
//		Purpose: The encoded filename stored has changed
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::EncodedFilenameChanged()
{
	BackupStoreFilename::EncodedFilenameChanged();

	// Delete stored filename in clear
	mClearFilename.erase();
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::SetBlowfishKey(const void *, int)
//		Purpose: Set the key used for Blowfish encryption of filenames
//		Created: 1/12/03
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::SetBlowfishKey(const void *pKey, int8_t KeyLength, const void *pIV, int8_t IVLength)
{
	// Initialisation vector not used. Can't use a different vector for each filename as
	// that would stop comparisions on the server working.
	sBlowfishEncrypt.Reset();
	sBlowfishEncrypt.Init(CipherContext::Encrypt, CipherBlowfish(CipherDescription::Mode_CBC, pKey, KeyLength));
	ASSERT(sBlowfishEncrypt.GetIVLength() == IVLength);
	sBlowfishEncrypt.SetIV(pIV);
	sBlowfishDecrypt.Reset();
	sBlowfishDecrypt.Init(CipherContext::Decrypt, CipherBlowfish(CipherDescription::Mode_CBC, pKey, KeyLength));
	ASSERT(sBlowfishDecrypt.GetIVLength() == IVLength);
	sBlowfishDecrypt.SetIV(pIV);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupStoreFilenameClear::SetEncodingMethod(int)
//		Purpose: Set the encoding method used for filenames
//		Created: 1/12/03
//
// --------------------------------------------------------------------------
void BackupStoreFilenameClear::SetEncodingMethod(int Method)
{
	sEncodeMethod = Method;
}



