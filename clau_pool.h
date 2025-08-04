
#include <vector>
#include <iostream>

namespace clau {
	// todo - smartpointer? std::unique<Block> ?
	template <class Block>
	class BlockManager { // manager for not using?
	public: // or friend? or set_~~ 
		// start_block-> ...->last_block(or nullptr)
		Block* start_block = nullptr;
		Block* last_block = nullptr;
	public:
		[[nodiscard]]
		Block* Get(uint64_t cap) {
			if (!start_block) {
				start_block = new (std::nothrow)Block(cap);
				if (start_block == nullptr) { return nullptr; }
				Block* result = start_block;
				start_block = nullptr;
				return result;
			}
			Block* block = start_block;
			Block* before_block = nullptr;

			while (block && (block->capacity != cap)) {
				before_block = block;
				block = block->next;
			}
			if (block) {
				if (before_block) {
					before_block->next = block->next;
				}
				else { // 
					start_block = block->next;
					if (start_block == nullptr) {
						last_block = nullptr;
					}
				}
				block->offset = 0;
				block->next = nullptr;
				return block;
			}
			return new(std::nothrow)Block(cap);
		}
	public:
		BlockManager(Block* start_block, Block* last_block) : start_block(start_block), last_block(last_block) {
			if (last_block) {
				last_block->next = nullptr;
			}
		}

		// check last_block?, has bug?
		void RemoveBlocks() {
			//t64_t count = 0;
			Block* block = start_block;
			while (block) {
				Block* next = block->next;
				delete block;
				block = next;
				//unt++;
			}	//d::cout << "count1 " << count << " \n";
		}
	};

	// bug - 크기를 줄일떄? 메모리 소비?
	// memory_pool?
	class Arena {
	public:
		struct Block {
			Block* next; //
			uint64_t capacity;
			uint64_t offset;
			uint8_t* data;

			Block(uint64_t cap)
				: next(nullptr), capacity(cap), offset(0) {
				//data = (uint8_t*)mi_malloc(sizeof(uint8_t) * capacity); // 
				data = new (std::nothrow) uint8_t[capacity];
			}

			~Block() {
				delete[] data;
				data = nullptr;
				//	mi_free(data);
			}

			Block(const Block&) = delete;
			Block& operator=(const Block&) = delete;
		};
	public:
		Block* head[1];
		Block* rear[1];
		const uint64_t defaultBlockSize;
		Arena* now_pool;
		Arena* next;
		uint64_t count = 0;
	public:
		static const uint64_t initialSize = 1024 * 1024; //

	private:
		BlockManager<Block> blockManager;
		std::vector<Block*> startBlockVec;
		std::vector<Block*> lastBlockVec;

	private:
		void RemoveBlocks() {
			{
				Clear();
				std::vector<BlockManager<Block>> result = DivideBlock();
				for (auto& x : result) {
					x.RemoveBlocks();
				}
			}

			blockManager.RemoveBlocks();
			Block* block = head[0];
			//t count = 0;
			while (block) {
				Block* next = block->next;
				delete block;
				block = next;
				//unt++;
			}
			//d::cout << "count2 " << count << " \n";
		}
	public:

		void Clear() {
			if (lastBlockVec.empty()) {
				if (blockManager.last_block) {
					blockManager.last_block->next = head[0];
				}
				else {
					blockManager.start_block = head[0];
				}
				if (!blockManager.start_block) {
					blockManager.start_block = head[0];
				}

				blockManager.last_block = rear[0];
				if (blockManager.last_block) {
					blockManager.last_block->next = nullptr;
				}

				head[0] = blockManager.Get(defaultBlockSize);
				rear[0] = head[0];

				now_pool = this;
				// chk! memory leak.-fix
				while (next) {
					Arena* temp = next->next;
					next->next = nullptr;
					delete next;
					next = temp;
				}
				next = nullptr;
			}
			else {
				Reset();
			}
		}

		// link_from -> Reset -> DivideBlock -> link_from...
		void Reset() {
			if (lastBlockVec.empty()) {
				return;
			}
			if (blockManager.last_block) {
				blockManager.last_block->next = head[0];
			}
			else {
				blockManager.start_block = head[0];
			}
			if (!blockManager.start_block) {
				blockManager.start_block = head[0];
			}

			blockManager.last_block = lastBlockVec[0];
			if (blockManager.last_block) {
				blockManager.last_block->next = nullptr;
			}

			head[0] = blockManager.Get(defaultBlockSize);
			rear[0] = head[0];

			now_pool = this;
			// chk! memory leak.-fix
			while (next) {
				Arena* temp = next->next;
				next->next = nullptr;
				delete next;
				next = temp;
			}
			next = nullptr;
		}

		std::vector<BlockManager<Block>> DivideBlock() {
			if (lastBlockVec.empty()) { return {}; }
			lastBlockVec.push_back(nullptr);

			blockManager.last_block = lastBlockVec[0];
			if (blockManager.last_block) {
				blockManager.last_block->next = nullptr;
			}

			std::vector<BlockManager<Block>> blocks;
			for (uint64_t i = 1; i < lastBlockVec.size(); ++i) {
				blocks.push_back({ startBlockVec[i - 1], lastBlockVec[i] });
			}

			startBlockVec.clear();
			lastBlockVec.clear();

			return blocks;
		}
	public:
		Arena(uint64_t size = initialSize) : defaultBlockSize(size), blockManager(nullptr, nullptr) {
			for (int i = 0; i < 1; ++i) { // i < 4
				head[i] = blockManager.Get(defaultBlockSize);
				rear[i] = head[i];
			}
			now_pool = this;
			next = nullptr;
		}
		Arena(Block* start_block, Block* last_block, uint64_t size = initialSize)
			: defaultBlockSize(size), blockManager(start_block, last_block) {
			for (int i = 0; i < 1; ++i) { // i < 4
				head[i] = blockManager.Get(defaultBlockSize); // (new (std::nothrow) Block(initialSize));
				rear[i] = head[i];
			}
			now_pool = this;
			next = nullptr;
		}

		Arena(const Arena&) = delete;
		Arena& operator=(const Arena&) = delete;

	public:
		template <class T>
		T* allocate(uint64_t size, uint64_t align = alignof(T)) {
			{
				Block* block = now_pool->head[0];

				if (block) {
					if (block->offset + size < block->capacity) {
						uint64_t remain = block->capacity - block->offset;

						void* ptr = block->data + block->offset;
						void* aligned_ptr = ptr;

						if (block->offset < 0 && remain >= size) {
							block->offset += size;
							return reinterpret_cast<T*>(aligned_ptr);
						}
						else if (block->offset >= 0 && std::align(alignof(T), size, aligned_ptr, remain)) {
							size_t aligned_offset = static_cast<uint8_t*>(aligned_ptr) - block->data;

							block->offset = aligned_offset + size;

							return reinterpret_cast<T*>(aligned_ptr);
						}
					}
					block = block->next;
				}
			}

			// allocate new block
			uint64_t newCap = std::max(defaultBlockSize, size + 64);
			Block* newBlock = blockManager.Get(newCap); // new (std::nothrow) Block(newCap);
			if (!newBlock) {
				return nullptr;
			}
			//counter++;

			uint64_t remain = newBlock->capacity - newBlock->offset;
			void* ptr = newBlock->data + newBlock->offset;
			void* aligned_ptr = ptr;

			if (std::align(alignof(T), size, aligned_ptr, remain)) {
				uint64_t aligned_offset = static_cast<uint8_t*>(aligned_ptr) - newBlock->data;

				newBlock->offset = aligned_offset + size;

				newBlock->next = now_pool->head[0];

				now_pool->head[0] = newBlock;

				return reinterpret_cast<T*>(aligned_ptr);
			}

			return nullptr;
		}

		// expand
		template <class T>
		void deallocate(T* ptr, uint64_t len) {


			return;

			//return;

			// chk !
			Block* block = now_pool->head[0];

			while (block) {
				if ((uint8_t*)(ptr)+sizeof(T) * len == (uint8_t*)(block->data) + block->offset) {
					block->offset = (uint8_t*)ptr - (uint8_t*)block->data;
					//std::cout << "real_deallocated\n"; //
					return;
				}

				block = block->next;
			}
		}

		template<typename T, typename... Args>
		T* create(Args&&... args) {
			void* mem = allocate<T>(sizeof(T), alignof(T));
			return new (mem) T(std::forward<Args>(args)...);
		}

	public:
		~Arena() {
			if (this != now_pool) {
				return;
			}

			RemoveBlocks();

			while (next) {
				Arena* temp = next->next;
				next->next = nullptr;
				delete next;
				next = temp;
			}
		}

		// chk! when merge?
		void link_from(Arena* other) {
			for (int i = 0; i < 1; ++i) { // i < 1
				if (!this->head[i]) {
					this->head[i] = other->head[i];
					this->rear[i] = other->rear[i];
				}
				else {
					this->startBlockVec.push_back(other->head[i]);
					this->lastBlockVec.push_back(this->rear[i]);

					this->rear[i]->next = other->head[i];
					this->rear[i] = other->rear[i];
				}
			}

			if (this->blockManager.last_block) {
				this->blockManager.last_block->next = other->blockManager.start_block;
			}
			else {
				this->blockManager.start_block = other->blockManager.start_block;
			}
			if (!this->blockManager.start_block) {
				this->blockManager.start_block = other->blockManager.start_block;
			}
			this->blockManager.last_block = other->blockManager.last_block;

			other->blockManager.start_block = nullptr;
			other->blockManager.last_block = nullptr;

			other->now_pool = this->now_pool;

			other->next = this->next;
			this->next = other;

			for (int i = 0; i < 1; ++i) {
				other->head[i] = nullptr;
				other->rear[i] = nullptr;
			}
		}
	};

	template <class T>
	class Vector2 {
	private:
		Arena* pool = nullptr;
		T* m_arr = nullptr;
		uint32_t m_capacity = 0;
		uint32_t m_size = 0;
	public:
		// =delete?
		Vector2() : pool(nullptr) {
			//
		}
		Vector2(uint64_t sz) : pool(nullptr) {
			m_size = sz;
			m_capacity = 2 * sz;
			m_arr = new T[m_capacity]();
		}
		// with Arena..
		Vector2(Arena* pool, uint64_t sz) : pool(pool) {
			m_size = sz;
			m_capacity = 2 * sz;
			if (pool) {
				m_arr = (T*)pool->allocate<T>(sizeof(T) * m_capacity, alignof(T));

				//for (uint64_t i = 0; i < m_size; ++i) {
				//	new (&m_arr[i]) T();
				//}
			}
			else {
				std::cout << "pool is nullptr\n"; // chk?
				m_arr = new T[m_capacity]();
			}
		}
		Vector2(Arena* pool, uint64_t sz, uint64_t capacity) : pool(pool) {
			m_size = sz;
			m_capacity = capacity;
			// todo - sz <= capacity!
			if (pool) {
				m_arr = (T*)pool->allocate<T>(sizeof(T) * m_capacity, alignof(T));

				//for (uint64_t i = 0; i < m_size; ++i) {
				//	new (&m_arr[i]) T();
				//}
			}
			else {
				m_arr = new T[m_capacity]();
			}
		}
		~Vector2() {
			if (m_arr && !pool) { delete[] m_arr; }
			else if (m_arr) {
				//for (uint64_t i = 0; i < m_size; ++i) {
				//	m_arr[i].~T();
				//}

				if (m_capacity > 0) {
					pool->deallocate(m_arr, m_capacity);
				}
			}
		}
		Vector2(const Vector2& other) {
			if (other.m_arr) {
				this->pool = other.pool;
				if (pool) {
					this->m_arr = (T*)pool->allocate<T>(sizeof(T) * other.m_capacity, alignof(T));
				}
				else {
					this->m_arr = new T[other.m_capacity]();
				}
				//this->m_arr = new T[other.m_capacity];
				this->m_capacity = other.m_capacity;
				this->m_size = other.m_size;
				for (uint64_t i = 0; i < other.m_size; ++i) {
					new (&m_arr[i]) T(other.m_arr[i]);
				}
			}
		}
		Vector2(Vector2&& other) noexcept {
			std::swap(m_arr, other.m_arr);
			std::swap(m_capacity, other.m_capacity);
			std::swap(m_size, other.m_size);
			std::swap(pool, other.pool);
		}
	public:
		Vector2& operator=(const Vector2& other) {
			if (this == &other) { return *this; }

			if (other.m_arr) {
				if (this->m_arr && !this->pool) {
					delete[] this->m_arr; this->m_arr = nullptr;
				}
				else if (this->m_arr) {
					for (uint64_t i = 0; i < m_size; ++i) {
						m_arr[i].~T();
					}
					this->pool->deallocate(this->m_arr, this->m_capacity);
				}
				if (this->pool) {
					this->m_arr = pool->allocate<T>(sizeof(T) * other.m_capacity, alignof(T));
				}
				else {
					this->m_arr = new T[other.m_capacity]();
				}
				this->m_capacity = other.m_capacity;
				this->m_size = other.m_size;
				for (uint64_t i = 0; i < other.m_size; ++i) {
					new (&m_arr[i]) T(other.m_arr[i]);
				}
			}
			else {
				if (m_arr && !this->pool) { delete[] m_arr; }
				else if (m_arr) {
					for (uint64_t i = 0; i < m_size; ++i) {
						m_arr[i].~T();
					}
					this->pool->deallocate(this->m_arr, this->m_capacity);
				}
				m_arr = nullptr;
				m_capacity = 0;
				m_size = 0;
			}
			return *this;
		}
		void operator=(Vector2&& other) noexcept {
			if (this == &other) { return; }

			std::swap(m_arr, other.m_arr);
			std::swap(m_capacity, other.m_capacity);
			std::swap(m_size, other.m_size);
			std::swap(pool, other.pool);
		}
	public:
		Arena* get_pool() { return pool; }

	public:

		// rename...! // [start_idx ~ size())
		[[nodiscard]]
		Vector2<T> Divide(uint64_t start_idx) {
			Vector2<T> result;

			if (pool && start_idx < size()) {
				result.pool = this->pool->now_pool;
				result.m_arr = this->m_arr + start_idx;
				result.m_capacity = (capacity() - start_idx);
				result.m_size = size() - start_idx;
				this->m_capacity = start_idx; // ?
				this->m_size = start_idx;
			}
			else if (start_idx < size()) {
				for (uint64_t i = start_idx; i < size(); ++i) {
					result.push_back(std::move(this->m_arr[i]));
				}
				for (uint64_t i = m_size; i > start_idx; --i) {
					this->pop_back();
				}
			}

			return result;
		}
		void erase(T* p) {
			uint64_t idx = p - m_arr;

			for (uint64_t i = idx; i + 1 < m_size; ++i) {
				m_arr[i] = std::move(m_arr[i + 1]);
			}

			m_size--;
		}
		bool has_pool()const {
			return pool;
		}
		void insert(T* start, T* last) {
			uint64_t sz = m_size + (last - start);

			if (sz <= m_size) { return; }

			{
				if (sz > m_capacity) {
					expand(2 * sz);
				}
				for (uint64_t i = m_size; i < sz; ++i) {
					new (&m_arr[i]) T(std::move(start[i - m_size]));
					//	m_arr[i] = std::move(start[i - m_size]);
				}
			}
			m_size = sz;
		}

		void push_back(T x) {
			if (size() >= capacity()) {
				if (capacity() == 0) {
					expand(2);
				}
				else {
					expand(2 * capacity());
				}
			}

			//new (&m_arr[m_size]) T();
			new (&m_arr[m_size++]) T(std::move(x));
		}
		void pop_back() {
			if (m_size > 0) {
				m_size--;
			}
		}
		T& back() {
			return m_arr[m_size - 1];
		}
		const T& back() const {
			return m_arr[m_size - 1];
		}
		T& operator[](uint64_t idx) {
			return m_arr[idx];
		}
		const T& operator[](uint64_t idx) const {
			return m_arr[idx];
		}
		void reserve(uint64_t sz) {
			if (capacity() < sz) {
				expand(sz);
			}
		}
		bool empty() const { return 0 == m_size; }
		T* begin() { return m_arr; }
		T* end() { return m_arr + m_size; }
		const T* begin() const { return m_arr; }
		const T* end() const { return m_arr + m_size; }
		void clear() {
			m_size = 0;
		}
		uint64_t size() const { return m_size; }
		uint64_t capacity() const { return m_capacity; }

		void resize(uint64_t sz) {
			if (sz < m_capacity) {
				m_size = sz;
				return;
			}
			expand(sz * 2);
			m_size = sz;
		}
	private:
		void expand(uint64_t new_capacity) {
			if (pool) {
				T* temp = (T*)pool->allocate<T>(sizeof(T) * new_capacity);
				for (uint64_t i = 0; i < m_size; ++i) {
					//new (temp + i) T();
					new (temp + i) T(std::move(m_arr[i]));
				}

				//for (uint64_t i = 0; i < m_size; ++i) {
				//	m_arr[i].~T();
				//}
				if (temp != m_arr) {
					pool->deallocate<T>(m_arr, m_capacity);
				}
				m_arr = temp;
			}
			else {
				T* temp = new (std::nothrow) T[new_capacity]();
				if (m_arr) {
					for (uint64_t i = 0; i < m_size; ++i) {
						temp[i] = std::move(m_arr[i]);
					}
					delete[] m_arr;
				}

				m_arr = temp;
			}
			m_capacity = new_capacity;
		}
	};

	template <class T>
	using my_vector = Vector2<T>;
}
