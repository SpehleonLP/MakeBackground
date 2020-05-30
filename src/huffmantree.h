#ifndef HUFFMANTREE_H
#define HUFFMANTREE_H
#include <cstdint>

class HuffmanTree
{
public:
	enum CODES
	{
		UPDATE_TREE,
		USE_TREE,
		UNIQUE_STRING,
		REPEATED_STRING,
	};

	HuffmanTree();

	void DECOMPRESS(uint8_t * dst, uint8_t * src);
};

#endif // HUFFMANTREE_H
