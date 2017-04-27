#include "MemoryManager.h"

#include <iostream>

namespace MemoryManager
{
  // IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT 
  //
  // This is the only static memory that you may use, no other global variables may be
  // created, if you need to save data make it fit in MM_pool
  //
  // IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT

  // Change these to try different block sizes
  #define BLOCK_SIZE       2        // Number of bytes in a block
  #define BLOCK_ADDR_MASK  0x7FFF   // Mask for extracting an address from a block
  #define BLOCK_LOWER_MASK 0x00FF   // Mask for extracting the lower byte from a block
  #define BLOCK_BYTE_SHIFT 1        // The amount to shift a block to get bytes, and vice versa
  #define BLOCK_UPPER_MASK 0xFF00   // Mask for extracting the upper byte from a block
  #define BLOCK_USED_MASK  0x8000   // Mask for extracting the used/free bit from a block
  #define BYTE_ADDR_MASK   0x7F     // Mask for extracting an address from a byte
  #define BYTE_USED_MASK   0x80     // Mask for extracting the used/free bit from a byte
  #define MIN_CHUNK_SIZE   1        // Minimum number of usable blocks in chunk. Must be > 0

  const int MM_POOL_SIZE = 65536;
  char MM_pool[MM_POOL_SIZE];

  //////////////////////////////////////////////////////////////////////////////
  // Support routines
  //////////////////////////////////////////////////////////////////////////////

  // Convert a 15-bit block address to the index in the MM_pool array
  int blockAddressToPoolIndex(int blockAddress)
  {
    return (blockAddress & BLOCK_ADDR_MASK) << BLOCK_BYTE_SHIFT;
  }

  // Convert an index in the MM_pool array to a 15-bit block address
  int poolIndexToBlockAddress(int poolIndex)
  {
    return (poolIndex >> BLOCK_BYTE_SHIFT) & BLOCK_ADDR_MASK;
  }

  // Get the address stored in the head or tail block
  unsigned int getLinkedAddress(int blockAddress)
  {
    // Get the index of the first byte of the block
    unsigned int firstByteIndex = blockAddressToPoolIndex(blockAddress);

    // Get the first byte of the block, without the used/free bit
    unsigned int firstByte = (unsigned char)MM_pool[firstByteIndex] & BYTE_ADDR_MASK;

    // Get the second byte of the block
    unsigned int secondByte = (unsigned char)MM_pool[firstByteIndex + 1];

    return (firstByte << 8) | secondByte;
  }

  // Check if the chunk belonging to the given metablock is used
  bool isChunkUsed(int blockAddress)
  {
    return (MM_pool[blockAddressToPoolIndex(blockAddress)] & BYTE_USED_MASK) > 0;
  }

  // Get the number of blocks between the given head and its corresponding tail
  int getNumDataBlocksInChunk(int headBlockAddress)
  {
    int tailBlockAddress = getLinkedAddress(headBlockAddress);
    return tailBlockAddress - (headBlockAddress + 1);
  }

  // Set the block at the given address to the given 16-bit value
  void setBlock(int blockAddress, int s)
  {
    int firstBytePoolIndex = blockAddressToPoolIndex(blockAddress);
    int firstByte = s >> 8;
    int secondByte = s & BLOCK_LOWER_MASK;
    MM_pool[firstBytePoolIndex] = firstByte;
    MM_pool[firstBytePoolIndex + 1] = secondByte;
  }

  // Set the head and tail metadata for a chunk
  void setChunkMetadata(bool used, int headBlockAddress, int numBlocks)
  {
    int tailBlockAddress = headBlockAddress + numBlocks + 1;
    setBlock(headBlockAddress, (used ? BLOCK_USED_MASK : 0) | tailBlockAddress);
    setBlock(tailBlockAddress, headBlockAddress);
  }

  // Check if the chunk whose head is given is valid (head and tail addresses
  //   within bounds, head and tail point to each other)
  bool validateChunk(int headBlockAddress)
  {

  }

  //////////////////////////////////////////////////////////////////////////////
  // Core functions
  //////////////////////////////////////////////////////////////////////////////

  // Initialize set up any data needed to manage the memory pool
  void initializeMemoryManager(void)
  {
    setChunkMetadata(false, 0, MM_POOL_SIZE / BLOCK_SIZE - 2);
  }

  // Return a pointer inside the memory pool
  // If no chunk can accommodate aSize call onOutOfMemory()
  void* allocate(int aSize)
  {
    int dataBlocksInChunk;
    int currentHeadAddress = 0;
    int currentTailAddress = 0;
    int numBlocksRequested;

    // Validate argument
    if (aSize <= 0 || aSize >= MM_POOL_SIZE)
      onIllegalOperation("allocate(%d) aSize must be between 0 and %d", aSize, MM_POOL_SIZE);

    // Calculate number of blocks required
    numBlocksRequested = aSize / BLOCK_SIZE + (aSize % BLOCK_SIZE > 0 ? 1 : 0);

    // Iterate through all chunks
    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      // Skip used chunks
      if (!isChunkUsed(currentHeadAddress))
      {
        dataBlocksInChunk = getNumDataBlocksInChunk(currentHeadAddress);

        // Check if there's enough free space to create a new free chunk as well
        // Usable space in free chunk must be greater than MIN_CHUNK_SIZE
        if (dataBlocksInChunk >= numBlocksRequested + MIN_CHUNK_SIZE + 2)
        {
          // Create a used chunk
          setChunkMetadata(true, currentHeadAddress, numBlocksRequested);

          // Make another free chunk with the remaining area
          int remainingFreeChunks = dataBlocksInChunk - (numBlocksRequested + 2);
          setChunkMetadata(false, currentHeadAddress + numBlocksRequested + 2, remainingFreeChunks);

          // Pointer address = start of pool + index of current head + size of
          //   head in bytes
          return (MM_pool + blockAddressToPoolIndex(currentHeadAddress) + BLOCK_SIZE);
        }
        else if (dataBlocksInChunk >= numBlocksRequested)
        {
          // Convert the existing free chunk to a used chunk, even though not
          //   all space will be used
          setChunkMetadata(true, currentHeadAddress, dataBlocksInChunk);
          return (MM_pool + blockAddressToPoolIndex(currentHeadAddress) + BLOCK_SIZE);
        }
      }

      // Advance to the next chunk
      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    onOutOfMemory();
    return ((void*) 0);
  }

  // Free up a chunk previously allocated
  void deallocate(void* aPointer)
  {
    
  }

  // Will scan the memory pool and return the total free space remaining
  int freeRemaining(void)
  {
    int currentHeadAddress = 0;
    int freeBlocks = 0;

    // Iterate through the list of chunks, ignoring used chunks
    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      if (!isChunkUsed(currentHeadAddress))
      {
        freeBlocks += getNumDataBlocksInChunk(currentHeadAddress);
      }

      // Advance to the next chunk
      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    // Convert from blocks to bytes
    return freeBlocks * BLOCK_SIZE;
  }

  // Will scan the memory pool and return the largest free space remaining
  int largestFree(void)
  {
    int currentHeadAddress = 0;
    int mostFreeBlocks = 0;

    // Iterate through the list of chunks, ignoring used chunks
    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      if (!isChunkUsed(currentHeadAddress))
      {
        int chunkSize = getNumDataBlocksInChunk(currentHeadAddress);
        if (chunkSize > mostFreeBlocks)
        {
          mostFreeBlocks = chunkSize;
        }
      }

      // Advance to the next chunk
      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    // Convert from blocks to bytes
    return mostFreeBlocks * BLOCK_SIZE;
  }

  // Will scan the memory pool and return the smallest free space remaining
  int smallestFree(void)
  {
    int currentHeadAddress = 0;
    int fewestFreeBlocks = MM_POOL_SIZE;

    // Iterate through the list of chunks, ignoring used chunks
    while (currentHeadAddress < (MM_POOL_SIZE / BLOCK_SIZE))
    {
      if (!isChunkUsed(currentHeadAddress))
      {
        // Compare the number of usable data blocks with fewest so far
        int dataBlocksInChunk = getNumDataBlocksInChunk(currentHeadAddress);
        if (dataBlocksInChunk < fewestFreeBlocks)
        {
          fewestFreeBlocks = dataBlocksInChunk;
        }
      }

      // Advance to the next chunk
      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    // Convert from blocks to bytes
    return fewestFreeBlocks * BLOCK_SIZE;
  }
}
