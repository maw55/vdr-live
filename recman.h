#ifndef VDR_LIVE_RECORDINGS_H
#define VDR_LIVE_RECORDINGS_H

#include <ctime>
#include <map>
#include <vector>
#include <vdr/recording.h>
#include "stdext.h"

namespace vdrlive {

	// Forward declations from epg_events.h
	class EpgInfo;
	typedef std::tr1::shared_ptr<EpgInfo> EpgInfoPtr;

	/**
	 *  Some forward declarations
	 */
	class RecordingsManager;
	class RecordingsTree;
	class RecordingsTreePtr;
	class RecordingsList;
	class RecordingsListPtr;
	class RecordingsItem;

	typedef std::tr1::shared_ptr< RecordingsManager > RecordingsManagerPtr;
	typedef std::tr1::shared_ptr< RecordingsItem > RecordingsItemPtr;
	typedef std::tr1::weak_ptr< RecordingsItem > RecordingsItemWeakPtr;
	typedef std::multimap< std::string, RecordingsItemPtr > RecordingsMap;


	/**
	 *  Class for managing recordings inside the live plugin. It
	 *  provides some convenience methods and provides automatic
	 *  locking during requests on the recordings or during the
	 *  traversion of the recordings tree or lists, which can only be
	 *  obtained through methods of the RecordingsManager.
	 */
	class RecordingsManager
	{
		friend RecordingsManagerPtr LiveRecordingsManager();

		public:
			/**
			 *  Returns a shared pointer to a fully populated
			 *  recordings tree.
			 */
			RecordingsTreePtr GetRecordingsTree() const;

			/**
			 *  Return a shared pointer to a populated recordings
			 *  list. The list is optionally sorted ascending or
			 *  descending by date and may be constrained by a date
			 *  range.
			 */
			RecordingsListPtr GetRecordingsList(bool ascending = true) const;
			RecordingsListPtr GetRecordingsList(time_t begin, time_t end, bool ascending = true) const;

			/**
			 *	generates a Md5 hash from a cRecording entry. It can be used
			 *  to reidentify a recording.
			 */
			std::string Md5Hash(cRecording const * recording) const;

			/**
			 *  fetches a cRecording from VDR's Recordings collection. Returns
			 *  NULL if recording was not found
			 */
			cRecording const* GetByMd5Hash(std::string const & hash) const;

			/**
			 *  Delete a recording with the given hash according to
			 *  VDRs recording deletion mechanisms.
			 */
			void DeleteRecording(cRecording const * recording) const;

			/**
			 *	Determine wether the recording has been archived on
			 *	removable media (e.g. DVD-ROM)
			 */
			static bool IsArchived(cRecording const * recording);

			/**
			 *	Provide an identification of the removable media
			 *	(e.g. DVD-ROM Number or Name) where the recording has
			 *	been archived.
			 */
			static std::string const GetArchiveId(cRecording const * recording);

			static std::string const GetArchiveDescr(cRecording const * recording);

		private:
			RecordingsManager();

			static RecordingsManagerPtr EnsureValidData();

			static std::tr1::weak_ptr< RecordingsManager > m_recMan;
			static std::tr1::shared_ptr< RecordingsTree > m_recTree;
			static std::tr1::shared_ptr< RecordingsList > m_recList;
			static int m_recordingsState;

			cThreadLock m_recordingsLock;
	};


	/**
	 *  Base class for entries in recordings tree and recordings list.
	 *  All opeations possible on recordings are performed against
	 *  pointers to instances of recordings items. The C++ polymorphy
	 *  delegates them to the 'right' class.
	 */
	class RecordingsItem
	{
		friend class RecordingsTree;

		protected:
			RecordingsItem(const std::string& name, RecordingsItemPtr parent);

		public:
			virtual ~RecordingsItem();

			virtual time_t StartTime() const = 0;
			virtual bool IsDir() const = 0;
			virtual long Duration() const = 0;
			virtual const std::string& Name() const { return m_name; }
			virtual const std::string Id() const = 0;

			virtual const cRecording* Recording() const { return 0; }
			virtual const cRecordingInfo* RecInfo() const { return 0; }

			RecordingsMap::const_iterator begin() const { return m_entries.begin(); }
			RecordingsMap::const_iterator end() const { return m_entries.end(); }

		private:
			std::string m_name;
			RecordingsMap m_entries;
			RecordingsItemWeakPtr m_parent;
	};


	/**
	 *  A recordings item that resembles a directory with other
	 *  subdirectories and/or real recordings.
	 */
	class RecordingsItemDir : public RecordingsItem
	{
		public:
			RecordingsItemDir(const std::string& name, int level, RecordingsItemPtr parent);

			virtual ~RecordingsItemDir();

			virtual time_t StartTime() const { return 0; }
			virtual long Duration() const { return 0; }
			virtual bool IsDir() const { return true; }
			virtual std::string const Id() const { return ""; }

		private:
			int m_level;
	};


	/**
	 *  A recordings item that represents a real recording. This is
	 *  the leaf item in the recordings tree or one of the items in
	 *  the recordings list.
	 */
	class RecordingsItemRec : public RecordingsItem
	{
		public:
			RecordingsItemRec(const std::string& id, const std::string& name, const cRecording* recording, RecordingsItemPtr parent);

			virtual ~RecordingsItemRec();

			virtual time_t StartTime() const;
			virtual long Duration() const;
			virtual bool IsDir() const { return false; }
			virtual const std::string Id() const { return m_id; }

			virtual const cRecording* Recording() const { return m_recording; }
			virtual const cRecordingInfo* RecInfo() const { return m_recording->Info(); }

		private:
			const cRecording *m_recording;
			std::string m_id;
	};


	/**
	 *  The recordings tree contains all recordings in a file system
	 *  tree like fashion.
	 */
	class RecordingsTree
	{
		friend class RecordingsManager;

		private:
			RecordingsTree(RecordingsManagerPtr recManPtr);

		public:
			virtual ~RecordingsTree();

			RecordingsItemPtr const & Root() const { return m_root; }

			RecordingsMap::iterator begin(const std::vector< std::string >& path);
			RecordingsMap::iterator end(const std::vector< std::string >&path);

			int MaxLevel() const { return m_maxLevel; }

		private:
			int m_maxLevel;
			RecordingsItemPtr m_root;

			RecordingsMap::iterator findDir(RecordingsItemPtr& dir, const std::string& dirname);
	};


	/**
	 *  A smart pointer to a recordings tree. As long as an instance of this
	 *  exists the recordings are locked in the vdr.
	 */
	class RecordingsTreePtr : public std::tr1::shared_ptr< RecordingsTree >
	{
		friend class RecordingsManager;

		private:
			RecordingsTreePtr(RecordingsManagerPtr recManPtr, std::tr1::shared_ptr< RecordingsTree > recTree);

		public:
			RecordingsTreePtr();
			virtual ~RecordingsTreePtr();

		private:
			RecordingsManagerPtr m_recManPtr;
	};


	/**
	 *  The recordings list contains all real recordings in a list
	 *  sorted by a given sorting predicate function. The directory
	 *  entries are not part of this list. The path towards the root
	 *  can be obtained via the 'parent' members of the recordings
	 *  items.
	 */
	class RecordingsList
	{
		friend class RecordingsManager;

		private:
			RecordingsList(RecordingsTreePtr recTree);
			RecordingsList(std::tr1::shared_ptr< RecordingsList > recList, bool ascending);
			RecordingsList(std::tr1::shared_ptr< RecordingsList > recList, time_t begin, time_t end, bool ascending);

		public:
			typedef std::vector< RecordingsItemPtr > RecVecType;

			virtual ~RecordingsList();

			RecVecType::const_iterator begin() const { return m_pRecVec->begin(); }
			RecVecType::const_iterator end() const { return m_pRecVec->end(); }

			RecVecType::size_type size() const { return m_pRecVec->size(); }

		private:
			class Ascending
			{
				public:
					bool operator()(RecordingsItemPtr const &x, RecordingsItemPtr const &y) const { return x->StartTime() < y->StartTime(); }
			};

			class Descending
			{
				public:
					bool operator()(RecordingsItemPtr const &x, RecordingsItemPtr const &y) const { return y->StartTime() < x->StartTime(); }
			};

			class NotInRange
			{
				public:
					NotInRange(time_t begin, time_t end);

					bool operator()(RecordingsItemPtr const &x) const;

				private:
					time_t m_begin;
					time_t m_end;
			};

		private:
			RecVecType *m_pRecVec;
	};


	/**
	 *  A smart pointer to a recordings list. As long as an instance of this
	 *  exists the recordings are locked in the vdr.
	 */
	class RecordingsListPtr : public std::tr1::shared_ptr< RecordingsList >
	{
		friend class RecordingsManager;

		private:
			RecordingsListPtr(RecordingsManagerPtr recManPtr, std::tr1::shared_ptr< RecordingsList > recList);

		public:
			virtual ~RecordingsListPtr();

		private:
			RecordingsManagerPtr m_recManPtr;
	};


	/**
	 *	return singleton instance of RecordingsManager as a shared Pointer.
	 *  This ensures that after last use of the RecordingsManager it is
	 *  deleted. After deletion of the original RecordingsManager a repeated
	 *  call to this function creates a new RecordingsManager which is again
	 *	kept alive as long references to it exist.
	 */
	RecordingsManagerPtr LiveRecordingsManager();

} // namespace vdrlive

#endif // VDR_LIVE_RECORDINGS_H
