// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/EnumClassFlags.h"
#include "Templates/UniquePtr.h"

namespace BuildPatchServices
{
	enum class EAttributeFlags : uint32
	{
		// Value for no attributes.
		None        = 0,
		// Whether the file exists on the storage volume.
		Exists      = 1 << 0,
		// Whether the file is readonly.
		ReadOnly    = 1 << 1,
		// Whether the file is compressed.
		Compressed  = 1 << 2,
		// Whether the file is executable.
		Executable  = 1 << 3
	};
	ENUM_CLASS_FLAGS(EAttributeFlags);

	enum class EWriteFlags : uint32
	{
		None                = 0,
		NoFail              = 1 << 0,
		NoReplaceExisting   = 1 << 1,
		EvenIfReadOnly      = 1 << 2,
		Append              = 1 << 3,
		AllowRead           = 1 << 4
	};
	ENUM_CLASS_FLAGS(EWriteFlags);

	enum class EReadFlags : uint32
	{
		None                = 0,
		NoFail              = 1 << 0,
		Silent              = 1 << 1,
		AllowWrite          = 1 << 2
	};
	ENUM_CLASS_FLAGS(EReadFlags);

	/**
	 * The File System class is used for classes which require file access. It wraps Core IFileManager, and IPlatformFile. Also provides additional
	 * functionality missing from these classes at the time of writing.
	 * Using this wrapper allows dependants to be easily testable.
	 */
	class IFileSystem
	{
	public:
		virtual ~IFileSystem() {}

		/**
		 * Checks whether a directory exists.
		 * @param DirectoryPath     The directory to check.
		 * @return true if directory exists.
		 */
		virtual bool DirectoryExists(const TCHAR* DirectoryPath) const = 0;

		/**
		 * Create a directory path.
		 * @param DirectoryPath     The directory path to create.
		 * @return true if successful.
		 */
		virtual bool MakeDirectory(const TCHAR* DirectoryPath) const = 0;

		/**
		 * Get the size of a file.
		 * @param Filename          The filename for the request.
		 * @param FileSize          Receives the file size in bytes or INDEX_NONE if the file didn't exist.
		 * @return true if successful.
		 */
		virtual bool GetFileSize(const TCHAR* Filename, int64& FileSize) const = 0;

		/**
		 * Get the attributes for a file.
		 * @param Filename          The filename for the request.
		 * @param Attributes        Receives the attribute flags if successful.
		 * @return true if successful.
		 */
		virtual bool GetAttributes(const TCHAR* Filename, EAttributeFlags& Attributes) const = 0;

		/**
		 * Set whether the file is readonly.
		 * @param Filename          The filename for the request.
		 * @param bIsReadOnly       The state to set.
		 * @return true if successful.
		 */
		virtual bool SetReadOnly(const TCHAR* Filename, bool bIsReadOnly) const = 0;

		/**
		 * Set whether the file is compressed.
		 * @param Filename          The filename for the request.
		 * @param bIsCompressed     The state to set.
		 * @return true if successful.
		 */
		virtual bool SetCompressed(const TCHAR* Filename, bool bIsCompressed) const = 0;

		/**
		 * Set whether the file is executable.
		 * @param Filename          The filename for the request.
		 * @param bIsExecutable     The state to set.
		 * @return true if successful.
		 */
		virtual bool SetExecutable(const TCHAR* Filename, bool bIsExecutable) const = 0;

		/**
		 * Creates file reader archive.
		 * @param Filename          The filename for the request.
		 * @param ReadFlags         The file open flags.
		 * @return unique pointer to the created archive, invalid if failed to open the file.
		 */
		virtual TUniquePtr<FArchive> CreateFileReader(const TCHAR* Filename, EReadFlags ReadFlags = EReadFlags::None) const = 0;

		/**
		 * Creates file writer archive.
		 * @param Filename          The filename for the request.
		 * @param WriteFlags        The file open flags.
		 * @return unique pointer to the created archive, invalid if failed to open the file.
		 */
		virtual TUniquePtr<FArchive> CreateFileWriter(const TCHAR* Filename, EWriteFlags WriteFlags = EWriteFlags::None) const = 0;

		/**
		 * Delete a file.
		 * @param Filename          The file to delete.
		 * @return true if the file was deleted or did not exist.
		 */
		virtual bool DeleteFile(const TCHAR* Filename) const = 0;

		/**
		 * Move or rename a file.
		 * @param FileDest          The destination file path.
		 * @param FileSource        The source file path.
		 * @return true if the file was moved/renamed successfully.
		 */
		virtual bool MoveFile(const TCHAR* FileDest, const TCHAR* FileSource) const = 0;

		/**
		 * Copy a file.
		 * @param FileDest          The destination file path.
		 * @param FileSource        The source file path.
		 * @return true if the file was copied successfully.
		 */
		virtual bool CopyFile(const TCHAR* FileDest, const TCHAR* FileSource) const = 0;

		/**
		 * Checks whether a file exists.
		 * @param Filename          The file to check.
		 * @return true if file exists.
		 */
		virtual bool FileExists(const TCHAR* Filename) const = 0;
	};

	/**
	 * A factory for creating an IFileSystem instance.
	 */
	class FFileSystemFactory
	{
	public:
		/**
		 * Creates an implementation which wraps use of IFileManager, and implements additional functionality.
		 * @return the new IFileSystem instance created.
		 */
		static IFileSystem* Create();
	};
}