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

  #define BLOCK_SIZE 2
  #define BLOCK_ADDR_MASK  0x7FFF
  #define BLOCK_LOWER_MASK 0x00FF
  #define BLOCK_UPPER_MASK 0xFF00
  #define BLOCK_USED_MASK  0x80     // bit is at front of first byte in block
  #define MIN_CHUNK_SIZE 1          // how much data must a chunk be able to store? set to > 0

  const int MM_POOL_SIZE = 65536;
  char MM_pool[MM_POOL_SIZE];

  //////////////////////////////////////////////////////////////////////////////
  // Support routines
  //////////////////////////////////////////////////////////////////////////////

  // Convert a 15-bit block address to the index in the MM_pool array
  int blockAddressToPoolIndex(int blockAddress)
  {
    int result = (blockAddress & BLOCK_ADDR_MASK) << 1;
    return result;
  }

  // Convert an index in the MM_pool array to a 15-bit block address
  int poolIndexToBlockAddress(int poolIndex)
  {
    return poolIndex >> 1;
  }

  // Get the value at the given block
  int getLinkedAddress(int blockAddress)
  {
    int firstByteIndex = blockAddressToPoolIndex(blockAddress);
    int firstByte = (unsigned char)MM_pool[firstByteIndex] & 0x7F;
    int secondByte = (unsigned char)MM_pool[firstByteIndex + 1];

    // Get the upper byte without the used bit
    int upperByte = firstByte << 8;

    // Add the lower byte
    int result = upperByte | secondByte;

    return result;
  }

  // Check if the chunk belonging to the given metablock is used
  bool isChunkUsed(int blockAddress)
  {
    return (MM_pool[blockAddressToPoolIndex(blockAddress)] & BLOCK_USED_MASK) > 0;
  }

  // Get the number of blocks between the given head and its corresponding tail
  int getChunkDataSize(int headBlockAddress)
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
    setBlock(headBlockAddress, (used ? 0x8000 : 0) | tailBlockAddress);
    setBlock(tailBlockAddress, headBlockAddress);
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

    // Validate arguments
    if (aSize <= 0)
      onIllegalOperation("allocate(%d) aSize must be > 0", aSize);

    // Calculate number of blocks required
    numBlocksRequested = aSize / BLOCK_SIZE + (aSize % BLOCK_SIZE > 0 ? 1 : 0);

    // Iterate through all blocks
    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      // Free space must also accommodate adding head/tail
      if (!isChunkUsed(currentHeadAddress))
      {
        dataBlocksInChunk = getChunkDataSize(currentHeadAddress);
        std::cout << "Chunk at " << currentHeadAddress << " has " << dataBlocksInChunk << " blocks free." << std::endl;

        if (dataBlocksInChunk >= numBlocksRequested + MIN_CHUNK_SIZE + 2)
        {
          setChunkMetadata(true, currentHeadAddress, numBlocksRequested);

          // Make another free chunk with the remaining area
          int remainingFreeChunks = dataBlocksInChunk - (numBlocksRequested + 2);
          setChunkMetadata(false, currentHeadAddress + numBlocksRequested + 2, remainingFreeChunks);

          return (MM_pool + blockAddressToPoolIndex(currentHeadAddress) + BLOCK_SIZE);
        }
        else if (dataBlocksInChunk >= numBlocksRequested)
        {
          // Convert the existing chunk to a used chunk
          setChunkMetadata(true, currentHeadAddress, dataBlocksInChunk);
          return (MM_pool + blockAddressToPoolIndex(currentHeadAddress) + BLOCK_SIZE);
        }
      }

      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    onOutOfMemory();
    return ((void*) 0);
  }

  // Free up a chunk previously allocated
  void deallocate(void* aPointer)
  {
    // TODO: IMPLEMENT ME
  }

  // Will scan the memory pool and return the total free space remaining
  int freeRemaining(void)
  {
    int currentHeadAddress = 0;
    int freeBlocks = 0;

    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      if (!isChunkUsed(currentHeadAddress))
      {
        std::cout << "Chunk at " << currentHeadAddress << " has " << getChunkDataSize(currentHeadAddress) << " blocks free" << std::endl;
        freeBlocks += getChunkDataSize(currentHeadAddress);
      }
      else
      {
        std::cout << "Chunk at " << currentHeadAddress << " has " << getChunkDataSize(currentHeadAddress) << " blocks used" << std::endl;
      }

      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    return freeBlocks * BLOCK_SIZE;
  }

  // Will scan the memory pool and return the largest free space remaining
  int largestFree(void)
  {
    int currentHeadAddress = 0;
    int largestFreeBlocks = 0;

    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      if (!isChunkUsed(currentHeadAddress))
      {
        int chunkSize = getChunkDataSize(currentHeadAddress);
        if (chunkSize > largestFreeBlocks)
        {
          largestFreeBlocks = chunkSize;
        }
      }

      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    return largestFreeBlocks * BLOCK_SIZE;
  }

  // Will scan the memory pool and return the smallest free space remaining
  int smallestFree(void)
  {
    int currentHeadAddress = 0;
    int smallestFreeBlocks = MM_POOL_SIZE;

    while (currentHeadAddress < MM_POOL_SIZE / BLOCK_SIZE)
    {
      if (!isChunkUsed(currentHeadAddress))
      {
        int chunkSize = getChunkDataSize(currentHeadAddress);
        if (chunkSize < smallestFreeBlocks)
        {
          smallestFreeBlocks = chunkSize;
        }
      }

      currentHeadAddress = getLinkedAddress(currentHeadAddress) + 1;
    }

    return smallestFreeBlocks * BLOCK_SIZE;
  }
 }