#pragma once

#include <shared_mutex>
#include <vector>

template<typename ID, typename DataType>
struct IDArray
{
	explicit IDArray()
	{
		Data.push_back({});
		RefCounts.push_back(0);
	}

	ID Create()
	{
		ID id = MakeID_Lock();
		Data[(uint32_t)id] = {};
		return id;
	}

	ID Create(const DataType& data)
	{
		ID id = MakeID_Lock();
		Data[(uint32_t)id] = data;
		return id;
	}

	ID Create(DataType&& data)
	{
		ID id = MakeID_Lock();
		Data[(uint32_t)id] = std::move(data);
		return id;
	}

	ID Create(DataType** outData)
	{
		ID id = MakeID_Lock();
		*outData = &Data[(uint32_t)id];
		return id;
	}

	template<typename... Args>
	ID Make(Args&&... InArgs)
	{
		ID id = MakeID_Lock();
		Data[(uint32_t)id] = DataType(std::forward<Args>(InArgs)...);
		return id;
	}

	void Update(ID id, DataType& data)
	{
		auto lock = ReadScopeLock();

		assert((size_t)id < RefCounts.size());
		Data[(uint32_t)id] = data;
	}

	void AddRef(ID id)
	{
		auto lock = ReadScopeLock();

		assert((size_t)id < RefCounts.size());
		RefCounts[(uint32_t)id]++;
	}

	uint32_t RefCount(ID id)
	{
		auto lock = ReadScopeLock();

		assert((size_t)id < RefCounts.size());
		return RefCounts[(size_t)id];
	}

	bool Reffed(ID id)
	{
		return RefCount(id) > 0u;
	}

	bool Valid(ID id) noexcept
	{
		auto lock = ReadScopeLock();

		return Valid_AssumeLocked(id);
	}

	bool Release(ID id)
	{
		auto lock = WriteScopeLock();

		if (!Valid_AssumeLocked(id))
			return false;

		if (--RefCounts[(uint32_t)id] == 0)
		{
			FreeIDs.push_back(id);
			return true;
		}

		return false;
	}

	// Must ReadScopeLock before getting to ensure pointer remains valid
	DataType* Get(ID id)
	{
		return (Valid_AssumeLocked(id) && Reffed_AssumeLocked(id)) ? &Data[(uint32_t)id] : nullptr;
	}

	inline std::shared_lock<std::shared_mutex> ReadScopeLock() const noexcept { return std::shared_lock<std::shared_mutex>(Mutex); }
	inline std::unique_lock<std::shared_mutex> WriteScopeLock() const noexcept { return std::unique_lock<std::shared_mutex>(Mutex); }

	inline size_t Size() noexcept 
	{
		auto lock = ReadScopeLock();

		return Data.size(); 
	}

	inline size_t UsedSize() noexcept 
	{
		auto lock = ReadScopeLock();

		return Data.size() - FreeIDs.size(); 
	}

	template<typename Func>
	void ForEachNullIfValid(Func&& func)
	{
		auto lock = ReadScopeLock();

		for (size_t i = 0; i < Data.size(); i++)
		{
			func(Valid_AssumeLocked((ID)i) ? &Data[i] : nullptr);
		}
	}

	template<typename Func>
	void ForEachValid(Func&& func) const
	{
		auto lock = ReadScopeLock();

		for (size_t i = 0; i < Data.size(); i++)
		{
			if (Valid_AssumeLocked((ID)i))
			{
				if (!func((ID)i, Data[i]))
				{
					break;
				}
			}
				
		}
	}

private:
	std::vector<ID>				FreeIDs;
	std::vector<DataType>		Data;
	std::vector<uint32_t>		RefCounts;
	mutable std::shared_mutex	Mutex;

	uint32_t RefCount_AssumeLocked(ID id) const
	{
		assert((size_t)id < RefCounts.size());
		return RefCounts[(size_t)id];
	}

	bool Reffed_AssumeLocked(ID id) const
	{
		return RefCount_AssumeLocked(id) > 0u;
	}

	bool Valid_AssumeLocked(ID id) const noexcept
	{
		return id != ID::INVALID && (size_t)id < RefCounts.size() && RefCounts[(uint32_t)id] > 0;
	}

	ID MakeID_Lock()
	{
		auto lock = WriteScopeLock();

		return MakeID();
	}

	ID MakeID()
	{
		if (!FreeIDs.empty())
		{
			ID id = FreeIDs.back();
			FreeIDs.pop_back();
			RefCounts[(uint32_t)id]++;
			return id;
		}
		else
		{
			ID id = (ID)Data.size();
			Data.push_back({});
			RefCounts.push_back(1);

			return id;
		}
	}
};
