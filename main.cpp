
#include "clau_pool.h"
#include <iostream>



int main(void)
{
	clau::Arena* main_pool = new clau::Arena; // 4M ?
	clau::my_vector<int>* vec = nullptr;
	{
		clau::Arena* pool = new clau::Arena;
		
		vec = pool->allocate<clau::my_vector<int>>(sizeof(clau::my_vector<int>));
		new (vec) clau::my_vector<int>(pool, 0, 1);

		vec->push_back(1);

		main_pool->link_from(pool); // todo? main_pool->unlink(index1, index2); // move index1 to index2
	}
	vec->push_back(2);
	for (auto& x : *vec) {
		std::cout << x << " ";
	}
	std::cout << "\n";
	
	main_pool->Reset(); // kill data?
	auto block_manager = main_pool->DivideBlock();
	// blockManager <- now, free-list?
	std::cout << main_pool->blockManager.start_block << " " << main_pool->blockManager.last_block << "\n";
	for (auto& x : block_manager) {
		std::cout << x.start_block << " " << x.last_block << "\n";
	}
	
	// linked list + Index?
	
	delete main_pool;

	return 0;
}
